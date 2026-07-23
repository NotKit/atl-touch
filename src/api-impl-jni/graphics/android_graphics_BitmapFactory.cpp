#include <string.h>
#include <vector>

#include "../defines.h"
#include "NinePatchChunk.h"

#include "include/codec/SkAndroidCodec.h"
#include "include/codec/SkBmpDecoder.h"
#include "include/codec/SkCodec.h"
#include "include/codec/SkGifDecoder.h"
#include "include/codec/SkIcoDecoder.h"
#include "include/codec/SkJpegDecoder.h"
#include "include/codec/SkPngChunkReader.h"
#include "include/codec/SkPngDecoder.h"
#include "include/codec/SkWbmpDecoder.h"
#include "include/codec/SkWebpDecoder.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkData.h"

extern "C" {
#include "../generated_headers/android_graphics_BitmapFactory.h"
}

static std::unique_ptr<SkCodec> make_codec(sk_sp<SkData> data, SkPngChunkReader *chunk_reader = nullptr)
{
	static const SkCodecs::Decoder decoders[] = {
		SkPngDecoder::Decoder(),
		SkJpegDecoder::Decoder(),
		SkWebpDecoder::Decoder(),
		SkGifDecoder::Decoder(),
		SkBmpDecoder::Decoder(),
		SkIcoDecoder::Decoder(),
		SkWbmpDecoder::Decoder(),
	};
	return SkCodec::MakeFromData(std::move(data), SkSpan(decoders, std::size(decoders)), chunk_reader);
}

/* captures the npTc chunk aapt embeds in compiled .9.png files */
class NinePatchChunkReader : public SkPngChunkReader {
public:
	bool readChunk(const char tag[], const void *data, size_t length) override
	{
		if (!strcmp("npTc", tag) && length <= 1024 * 1024)
			chunk.assign((const uint8_t *)data, (const uint8_t *)data + length);
		return true;
	}
	std::vector<uint8_t> chunk;
};

/* Decode into a new SkBitmap, subsampling the source by sample_size like
 * AOSP's BitmapFactory (Options.inSampleSize) so a large image can be decoded
 * straight to a smaller bitmap instead of at full resolution. sample_size <= 1
 * decodes at full size. */
static SkBitmap *decode_from_codec(std::unique_ptr<SkCodec> codec, int sample_size)
{
	std::unique_ptr<SkAndroidCodec> android_codec = SkAndroidCodec::MakeFromCodec(std::move(codec));
	if (!android_codec)
		return nullptr;

	if (sample_size < 1)
		sample_size = 1;

	SkISize sampled = android_codec->getSampledDimensions(sample_size);
	SkImageInfo info = android_codec->getInfo()
	                       .makeWH(sampled.width(), sampled.height())
	                       .makeColorType(kRGBA_8888_SkColorType)
	                       .makeAlphaType(kPremul_SkAlphaType);

	SkBitmap *bitmap = new SkBitmap();
	if (!bitmap->tryAllocPixels(info)) {
		delete bitmap;
		return nullptr;
	}

	SkAndroidCodec::AndroidOptions options;
	options.fSampleSize = sample_size;
	SkCodec::Result result = android_codec->getAndroidPixels(info, bitmap->getPixels(), bitmap->rowBytes(), &options);
	if (result != SkCodec::kSuccess && result != SkCodec::kIncompleteInput) {
		delete bitmap;
		return nullptr;
	}
	return bitmap;
}

SkBitmap *atl_decode_image_data(sk_sp<SkData> data)
{
	return decode_from_codec(make_codec(std::move(data)), 1);
}

JNIEXPORT jlong JNICALL Java_android_graphics_BitmapFactory_nativeDecodeStream(JNIEnv *env, jclass clazz, jobject is, jbyteArray storage, jobject outPadding, jobject opts, jobjectArray nine_patch_chunk_out)
{
	jclass is_class = env->GetObjectClass(is);
	jmethodID read_method = env->GetMethodID(is_class, "read", "([BII)I");
	jint storage_size = env->GetArrayLength(storage);
	std::vector<uint8_t> data;
	while (true) {
		jint count = env->CallIntMethod(is, read_method, storage, 0, storage_size);
		if (env->ExceptionCheck()) {
			env->ExceptionDescribe();
			return 0;
		}
		if (count <= 0)
			break;
		size_t old_size = data.size();
		data.resize(old_size + count);
		env->GetByteArrayRegion(storage, 0, count, (jbyte *)data.data() + old_size);
	}

	int sample_size = 1;
	if (opts) {
		jclass opts_class = env->GetObjectClass(opts);
		sample_size = env->GetIntField(opts, env->GetFieldID(opts_class, "inSampleSize", "I"));
	}

	/* pass a chunk reader so a PNG's ninepatch metadata is captured; it stays
	 * empty for other formats */
	sk_sp<NinePatchChunkReader> chunk_reader = sk_make_sp<NinePatchChunkReader>();
	SkBitmap *bitmap = decode_from_codec(make_codec(SkData::MakeWithCopy(data.data(), data.size()), chunk_reader.get()),
	                                     sample_size);
	if (!bitmap)
		return 0;

	struct Res_png_9patch *chunk = NULL;
	if (chunk_reader && !chunk_reader->chunk.empty()) {
		chunk = atl_ninepatch_chunk_parse(chunk_reader->chunk.data(), chunk_reader->chunk.size());
		if (chunk && nine_patch_chunk_out) {
			jbyteArray chunk_array = env->NewByteArray(chunk_reader->chunk.size());
			env->SetByteArrayRegion(chunk_array, 0, chunk_reader->chunk.size(), (jbyte *)chunk_reader->chunk.data());
			env->SetObjectArrayElement(nine_patch_chunk_out, 0, chunk_array);
			env->DeleteLocalRef(chunk_array);
		}
	}

	if (outPadding) {
		jclass rect_class = env->GetObjectClass(outPadding);
		env->SetIntField(outPadding, env->GetFieldID(rect_class, "left", "I"), chunk ? chunk->paddingLeft : -1);
		env->SetIntField(outPadding, env->GetFieldID(rect_class, "top", "I"), chunk ? chunk->paddingTop : -1);
		env->SetIntField(outPadding, env->GetFieldID(rect_class, "right", "I"), chunk ? chunk->paddingRight : -1);
		env->SetIntField(outPadding, env->GetFieldID(rect_class, "bottom", "I"), chunk ? chunk->paddingBottom : -1);
	}
	free(chunk);

	return _INTPTR(bitmap);
}
