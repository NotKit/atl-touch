/*
 * ffmpeg (libavcodec) codec backend: the mainline implementation.
 *
 * Hardware decode goes through libav hwdevice contexts (VAAPI / DRM_PRIME —
 * covers V4L2 request drivers via the DRM path); decoded hardware frames are
 * transferred to CPU memory and converted to RGBA for the raster Skia scene.
 * Zero-copy presentation (DRM prime dmabuf -> EGLImage) can come back once
 * the scene has a GL compositing path; decode itself is already accelerated.
 *
 * Parts of the original decoding code were taken from
 * https://git.sr.ht/~emersion/vaapi-decoder/tree/wayland/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#include "codec_backend.h"

struct atl_codec {
	AVCodecContext *avctx;
	union {
		struct {
			SwrContext *swr;
			int sample_rate;
		} audio;
		struct {
			struct SwsContext *sws;
			size_t extradata_size;
			uint8_t *extradata;
		} video;
	};
};

static struct atl_codec *ffmpeg_create(const char *codec_name)
{
	const AVCodec *codec = avcodec_find_decoder_by_name(codec_name);
	if (!codec) {
		fprintf(stderr, "MediaCodec/ffmpeg: codec '%s' not found\n", codec_name);
		return NULL;
	}
	AVCodecContext *avctx = avcodec_alloc_context3(codec);
	if (!avctx)
		return NULL;

	struct atl_codec *ctx = calloc(1, sizeof(struct atl_codec));
	ctx->avctx = avctx;
	return ctx;
}

static void ffmpeg_configure_audio(struct atl_codec *ctx, const uint8_t *extradata, size_t extradata_size,
                                   int sample_rate, int nb_channels)
{
	AVCodecContext *avctx = ctx->avctx;

	ctx->audio.sample_rate = sample_rate;
	avctx->sample_rate = sample_rate;
	if (nb_channels == 1)
		avctx->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
	else if (nb_channels == 2)
		avctx->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
	else
		fprintf(stderr, "MediaCodec/ffmpeg: unsupported channel count %d\n", nb_channels);

	if (extradata && extradata_size) {
		avctx->extradata_size = extradata_size;
		avctx->extradata = av_mallocz(extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);
		memcpy(avctx->extradata, extradata, extradata_size);
	}
}

static enum AVPixelFormat get_hw_format(AVCodecContext *avctx, const enum AVPixelFormat *pix_fmts)
{
	size_t i;
	for (i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
		if (pix_fmts[i] == AV_PIX_FMT_VAAPI || pix_fmts[i] == AV_PIX_FMT_DRM_PRIME)
			return pix_fmts[i];
	}

	fprintf(stderr, "MediaCodec/ffmpeg: no HW pixel format offered, falling back to software decode\n");
	return i > 0 ? pix_fmts[i - 1] : AV_PIX_FMT_NONE;
}

static void ffmpeg_configure_video(struct atl_codec *ctx, const uint8_t *csd, size_t csd_size)
{
	AVCodecContext *avctx = ctx->avctx;

	/* For some reason, using AVCodecContext.extradata doesn't work with
	 * livestreams. As a workaround, we inject the extradata into the first
	 * packet instead. */
	if (csd && csd_size) {
		ctx->video.extradata = av_mallocz(csd_size + AV_INPUT_BUFFER_PADDING_SIZE);
		memcpy(ctx->video.extradata, csd, csd_size);
		ctx->video.extradata_size = csd_size;
	}

	for (int i = 0;; i++) {
		const AVCodecHWConfig *config = avcodec_get_hw_config(avctx->codec, i);
		if (!config) {
			fprintf(stderr, "MediaCodec/ffmpeg: decoder %s has no VAAPI/DRM_PRIME support, "
			                "decoding in software\n",
			        avctx->codec->name);
			break;
		}

		if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
		    (config->pix_fmt == AV_PIX_FMT_VAAPI || config->pix_fmt == AV_PIX_FMT_DRM_PRIME)) {
			AVBufferRef *hw_device_ctx = NULL;
			if (av_hwdevice_ctx_create(&hw_device_ctx, config->device_type, NULL, NULL, 0) >= 0) {
				fprintf(stderr, "MediaCodec/ffmpeg: hardware decode via %s (%s)\n",
				        av_hwdevice_get_type_name(config->device_type),
				        av_get_pix_fmt_name(config->pix_fmt));
				avctx->get_format = get_hw_format;
				avctx->hw_device_ctx = hw_device_ctx;
				break;
			}
		}
	}
}

static bool ffmpeg_start(struct atl_codec *ctx)
{
	if (avcodec_open2(ctx->avctx, ctx->avctx->codec, NULL) < 0) {
		fprintf(stderr, "MediaCodec/ffmpeg: avcodec_open2 failed for %s\n", ctx->avctx->codec->name);
		return false;
	}
	return true;
}

static bool ffmpeg_is_video(struct atl_codec *ctx)
{
	return ctx->avctx->codec_type == AVMEDIA_TYPE_VIDEO;
}

static int ffmpeg_queue_input(struct atl_codec *ctx, const uint8_t *data, size_t size, int64_t pts_us)
{
	AVCodecContext *avctx = ctx->avctx;
	AVPacket *pkt = NULL;
	int ret;

	if (data) { /* NULL = end of stream */
		pkt = av_packet_alloc();
		if (avctx->codec_type == AVMEDIA_TYPE_VIDEO && ctx->video.extradata_size) {
			/* prepend the stashed codec-specific data to the first packet */
			pkt->size = size + ctx->video.extradata_size;
			pkt->data = av_malloc(pkt->size + AV_INPUT_BUFFER_PADDING_SIZE);
			memcpy(pkt->data, ctx->video.extradata, ctx->video.extradata_size);
			memcpy(pkt->data + ctx->video.extradata_size, data, size);
			av_freep(&ctx->video.extradata);
			ctx->video.extradata_size = 0;
		} else {
			pkt->size = size;
			pkt->data = av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE);
			memcpy(pkt->data, data, size);
		}
		pkt->pts = pts_us;
	}

	ret = avcodec_send_packet(avctx, pkt);
	av_packet_free(&pkt);
	if (ret == AVERROR(EAGAIN))
		return ATL_CODEC_AGAIN;
	if (ret < 0) {
		fprintf(stderr, "MediaCodec/ffmpeg: error sending packet: %s\n", av_err2str(ret));
		return ATL_CODEC_ERROR;
	}
	return 0;
}

static int ffmpeg_dequeue_audio(struct atl_codec *ctx, uint8_t *out, size_t capacity, int64_t *pts_us)
{
	AVCodecContext *avctx = ctx->avctx;
	AVFrame *frame = av_frame_alloc();

	int ret = avcodec_receive_frame(avctx, frame);
	if (ret < 0) {
		av_frame_free(&frame);
		if (ret == AVERROR_EOF)
			return ATL_CODEC_EOS;
		if (ret == AVERROR(EAGAIN))
			return ATL_CODEC_AGAIN;
		fprintf(stderr, "MediaCodec/ffmpeg: avcodec_receive_frame: %s\n", av_err2str(ret));
		return ATL_CODEC_ERROR;
	}

	if (!ctx->audio.swr) {
		ret = swr_alloc_set_opts2(&ctx->audio.swr,
		                          &avctx->ch_layout, AV_SAMPLE_FMT_S16, ctx->audio.sample_rate,
		                          &avctx->ch_layout, avctx->sample_fmt, avctx->sample_rate,
		                          0, NULL);
		if (ret != 0) {
			fprintf(stderr, "MediaCodec/ffmpeg: swresample alloc failed\n");
			av_frame_free(&frame);
			return ATL_CODEC_ERROR;
		}
		swr_init(ctx->audio.swr);
	}

	*pts_us = frame->pts;
	int out_samples = swr_convert(ctx->audio.swr, &out, frame->nb_samples,
	                              (uint8_t const **)frame->data, frame->nb_samples);
	int out_bytes = out_samples * 2 * avctx->ch_layout.nb_channels;
	av_frame_free(&frame);
	return out_bytes;
}

static struct atl_video_frame *ffmpeg_dequeue_video(struct atl_codec *ctx, int *status)
{
	AVCodecContext *avctx = ctx->avctx;
	AVFrame *frame = av_frame_alloc();

	int ret = avcodec_receive_frame(avctx, frame);
	if (ret < 0) {
		av_frame_free(&frame);
		*status = ret == AVERROR_EOF    ? ATL_CODEC_EOS
		          : ret == AVERROR(EAGAIN) ? ATL_CODEC_AGAIN
		                                   : ATL_CODEC_ERROR;
		if (*status == ATL_CODEC_ERROR)
			fprintf(stderr, "MediaCodec/ffmpeg: avcodec_receive_frame: %s\n", av_err2str(ret));
		return NULL;
	}

	/* hardware frame (VAAPI/DRM_PRIME): transfer to CPU memory */
	if (frame->hw_frames_ctx) {
		AVFrame *sw_frame = av_frame_alloc();
		ret = av_hwframe_transfer_data(sw_frame, frame, 0);
		sw_frame->pts = frame->pts;
		av_frame_free(&frame);
		if (ret < 0) {
			fprintf(stderr, "MediaCodec/ffmpeg: hw frame transfer failed: %s\n", av_err2str(ret));
			av_frame_free(&sw_frame);
			*status = ATL_CODEC_ERROR;
			return NULL;
		}
		frame = sw_frame;
	}

	int stride = frame->width * 4;
	uint8_t *pixels = malloc((size_t)frame->height * stride);
	ctx->video.sws = sws_getCachedContext(ctx->video.sws,
	                                      frame->width, frame->height, frame->format,
	                                      frame->width, frame->height, AV_PIX_FMT_RGBA,
	                                      0, NULL, NULL, NULL);
	sws_scale(ctx->video.sws, (const uint8_t *const *)frame->data, frame->linesize, 0,
	          frame->height, (uint8_t *[1]){pixels}, (int[1]){stride});

	struct atl_video_frame *out = calloc(1, sizeof(*out));
	out->width = frame->width;
	out->height = frame->height;
	out->pixels = pixels;
	out->pts_us = frame->pts;
	av_frame_free(&frame);
	*status = 0;
	return out;
}

static void ffmpeg_release(struct atl_codec *ctx)
{
	if (ctx->avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
		if (ctx->video.sws)
			sws_freeContext(ctx->video.sws);
		av_freep(&ctx->video.extradata);
	} else if (ctx->avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
		if (ctx->audio.swr)
			swr_free(&ctx->audio.swr);
	}
	avcodec_free_context(&ctx->avctx);
	free(ctx);
}

const struct atl_codec_backend atl_codec_backend_ffmpeg = {
	.name = "ffmpeg",
	.create = ffmpeg_create,
	.configure_audio = ffmpeg_configure_audio,
	.configure_video = ffmpeg_configure_video,
	.start = ffmpeg_start,
	.is_video = ffmpeg_is_video,
	.queue_input = ffmpeg_queue_input,
	.dequeue_audio = ffmpeg_dequeue_audio,
	.dequeue_video = ffmpeg_dequeue_video,
	.release = ffmpeg_release,
};
