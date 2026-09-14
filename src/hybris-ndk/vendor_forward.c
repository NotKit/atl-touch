/*
 * A Pixel vendor library, forwarded to the device's real one through libhybris.
 * One instance of this file is built per soname; see src/hybris-ndk/meson.build
 * and hybris_ndk.h.
 *
 * Google Camera's libgcastartup.so dlopen()s the Pixel's own libraries and
 * dlsym()s their entry points out of them.  An empty stub is not good enough
 * for the two its camera graph is built on:
 *
 *   * lib_aion_buffer.so is the dma-buf/gralloc allocator every buffer comes
 *     from, and its context CHECKs that the library answered - SIGABRT;
 *   * libOpenCL.so is late-bound with no null check at all, so the first call
 *     (clGetPlatformIDs) branches to address 0 - SIGSEGV.
 *
 * These are vendor interfaces with no public header (AIon) or with one we would
 * have to keep in step (OpenCL), so the forwarders declare no prototypes at
 * all.  Each exported symbol is a naked trampoline that branches to the real
 * function with the argument registers untouched, which is right for every
 * non-variadic C signature: x0-x7, v0-v7, the indirect-result register x8 and
 * the stack all pass straight through.
 *
 * The trampolines are aarch64, the only architecture libhybris runs on.
 * Elsewhere - a desktop build - nothing ever loads these libraries, and the
 * symbols are plain C stubs so the file still compiles.
 *
 * Build-time configuration:
 *   ATL_VENDOR_SONAME   the Android soname, as an app dlopen()s it
 *   ATL_VENDOR_PATH     absolute path to try when the soname does not resolve
 *   ATL_VENDOR_SYMBOLS  header defining ATL_VENDOR_SYMBOL_LIST(X)
 */

#include <stdio.h>

#include "hybris_ndk.h"

#include ATL_VENDOR_SYMBOLS

#ifdef __aarch64__

/*
 * One resolved-function slot per symbol, hidden so a trampoline can reach it
 * with adrp/ldr and no GOT.  Filled by the constructor below, which runs while
 * the app is still inside its dlopen() and before it can dlsym() anything.
 */
#define ATL_VENDOR_SLOT(name) \
	__attribute__((visibility("hidden"))) void *atl_vendor_p_##name;

ATL_VENDOR_SYMBOL_LIST(ATL_VENDOR_SLOT)

/*
 * name:  br <resolved>, or return 0 when the device's library did not have it.
 * Nothing is saved and nothing is clobbered but x16, which is the linker's own
 * scratch register and call-clobbered by the AAPCS.
 */
#define ATL_VENDOR_TRAMPOLINE(name)                             \
	__asm__(".text\n"                                           \
	        ".globl " #name "\n"                                \
	        ".type " #name ", %function\n"                      \
	        #name ":\n"                                         \
	        "  adrp x16, atl_vendor_p_" #name "\n"              \
	        "  ldr  x16, [x16, :lo12:atl_vendor_p_" #name "]\n" \
	        "  cbz  x16, 8f\n"                                  \
	        "  br   x16\n"                                      \
	        "8:\n"                                              \
	        "  mov  x0, #0\n"                                   \
	        "  ret\n"                                           \
	        ".size " #name ", .-" #name "\n");

ATL_VENDOR_SYMBOL_LIST(ATL_VENDOR_TRAMPOLINE)

/* the soname first, so libhybris' own search path and its refcount apply; the
 * absolute path second, for a loader configuration that does not look in
 * /vendor/lib64 */
static void *vendor_sym(const char *symbol)
{
	void *sym = atl_hybris_ndk_sym(ATL_VENDOR_SONAME, symbol);

	return sym ? sym : atl_hybris_ndk_sym(ATL_VENDOR_PATH, symbol);
}

/*
 * Resolved when this library is loaded rather than on the first call: the app
 * dlopen()s it and dlsym()s straight away, so there is no earlier moment, and a
 * lazy trampoline would have to spill the whole argument register file.
 *
 * The allocator behind lib_aion_buffer.so talks binder
 * (android.hardware.graphics.allocator), so the thread pool has to exist first.
 */
__attribute__((constructor)) static void atl_vendor_resolve(void)
{
	int found = 0, total = 0;

	atl_hybris_ndk_binder_pool();

#define ATL_VENDOR_RESOLVE(name)             \
	atl_vendor_p_##name = vendor_sym(#name); \
	found += atl_vendor_p_##name != NULL;    \
	total++;

	ATL_VENDOR_SYMBOL_LIST(ATL_VENDOR_RESOLVE)

	fprintf(stderr, "atl-ndk: %s forwarded to the device's vendor library, "
	                "%d of %d entry points\n", ATL_VENDOR_SONAME, found, total);
}

#else /* not aarch64: nothing loads these, but the file still has to build */

#define ATL_VENDOR_STUB(name) \
	void *name(void);         \
	void *name(void) { return NULL; }

ATL_VENDOR_SYMBOL_LIST(ATL_VENDOR_STUB)

__attribute__((constructor)) static void atl_vendor_announce(void)
{
	fprintf(stderr, "atl-ndk: %s cannot be forwarded on this architecture\n",
	        ATL_VENDOR_SONAME);
}

#endif
