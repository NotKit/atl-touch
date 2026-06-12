#include <vector>

#include "../defines.h"

#include "include/codec/SkBmpDecoder.h"
#include "include/codec/SkCodec.h"
#include "include/codec/SkGifDecoder.h"
#include "include/codec/SkIcoDecoder.h"
#include "include/codec/SkJpegDecoder.h"
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

SkBitmap *atl_decode_image_data(sk_sp<SkData> data)
{
	std::unique_ptr<SkCodec> codec = make_codec(std::move(data));
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

JNIEXPORT jlong JNICALL Java_android_graphics_BitmapFactory_nativeDecodeStream(JNIEnv *env, jclass clazz, jobject is, jbyteArray storage, jobject outPadding, jobject opts)
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
	return _INTPTR(atl_decode_image_data(SkData::MakeWithCopy(data.data(), data.size())));
}
