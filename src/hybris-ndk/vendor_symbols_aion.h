/*
 * Every AIon* symbol caiman's /vendor/lib64/lib_aion_buffer.so exports, for
 * vendor_forward.c.  That library is the Pixel's dma-buf/gralloc allocator and
 * every buffer Google Camera's camera graph allocates comes from it; an empty
 * stub aborts the process (aion_context.cc CHECKs that the context is valid).
 * The app looks all of these up but the last three.
 *
 * Generated from the device's own library:
 *   readelf -sDW /vendor/lib64/lib_aion_buffer.so | awk '{print $NF}'
 */

#define ATL_VENDOR_SYMBOL_LIST(X)     \
	X(AIonInit)                       \
	X(AIonDeinit)                     \
	X(AIonAlloc)                      \
	X(AIonFree)                       \
	X(AIonGetFd)                      \
	X(AIonGetSize)                    \
	X(AIonGetFileDescriptors)         \
	X(AIonFindFromFd)                 \
	X(AIonImportFd)                   \
	X(AIonImportView)                 \
	X(AIonMapNativeImageBuffer)       \
	X(AIonMapNativeImageBufferWithOffset) \
	X(AIonUnmapNativeImageBuffer)     \
	X(AIonBufferSyncStart)            \
	X(AIonBufferSyncEnd)              \
	X(AIonGetGpuAlignment)            \
	X(AIonGetHeapId)                  \
	X(AIonGetProviderString)          \
	X(AIonHintIncomingGPUWorkFor)     \
	X(AIonHintPowerBoost)             \
	X(AIonHintPowerMode)
