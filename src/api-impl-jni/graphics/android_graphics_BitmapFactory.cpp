#include <string.h>
#include <vector>

#include "../defines.h"
#include "NinePatchChunk.h"

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

static std::unique_ptr<SkCodec> make_codec(sk_sp<SkData> data)
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
	return SkCodec::MakeFromData(std::move(data), SkSpan(decoders, std::size(decoders)));
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

static SkBitmap *decode_from_codec(std::unique_ptr<SkCodec> codec)
{
	if (!codec)
		return nullptr;
	SkImageInfo info = codec->getInfo().makeColorType(kRGBA_8888_SkColorType).makeAlphaType(kPremul_SkAlphaType);
	SkBitmap *bitmap = new SkBitmap();
	bitmap->allocPixels(info);
	if (codec->getPixels(info, bitmap->getPixels(), bitmap->rowBytes()) != SkCodec::kSuccess) {
		delete bitmap;
		return nullptr;
	}
	return bitmap;
}

SkBitmap *atl_decode_image_data(sk_sp<SkData> data)
{
	return decode_from_codec(make_codec(std::move(data)));
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

	SkBitmap *bitmap;
	sk_sp<NinePatchChunkReader> chunk_reader;
	if (SkPngDecoder::IsPng(data.data(), data.size())) {
		/* decode PNGs through SkPngDecoder directly so a chunk reader can
		 * pick up the ninepatch metadata */
		chunk_reader = sk_make_sp<NinePatchChunkReader>();
		SkCodec::Result result;
		bitmap = decode_from_codec(SkPngDecoder::Decode(SkData::MakeWithCopy(data.data(), data.size()),
		                                                &result, chunk_reader.get()));
	} else {
		bitmap = atl_decode_image_data(SkData::MakeWithCopy(data.data(), data.size()));
	}
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
