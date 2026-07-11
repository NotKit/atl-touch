#ifndef ATL_INPUT_METHOD_H
#define ATL_INPUT_METHOD_H

#include <stdbool.h>

/* A soft-keyboard (input method) backend.
 *
 * Different wayland shells expose different mechanisms — Lomiri (Ubuntu
 * Touch) and SailfishOS drive the keyboard over Maliit's D-Bus interface
 * and never send text-input events over wayland, while desktop compositors
 * offer zwp_text_input_v3 — so the transport is pluggable. Backends deliver
 * text and key events back into the scene through the atl_windows_ime_*
 * helpers in ATLWindow.h, i.e. through the same path as hardware keyboard
 * input.
 */
struct atl_im_backend {
	const char *name;
	/* probe/connect; return false if this backend can't serve this session */
	bool (*init)(void);
	/* request the panel for an editor of the given android inputType */
	void (*show)(int input_type);
	void (*hide)(void);
};

/* Defined iff the backend is compiled in (conditional sources in meson);
 * declared weak so the framework can probe for them at runtime. */
extern const struct atl_im_backend atl_im_backend_maliit __attribute__((weak));

#endif
