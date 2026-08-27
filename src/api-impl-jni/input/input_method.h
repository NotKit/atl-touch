#ifndef ATL_INPUT_METHOD_H
#define ATL_INPUT_METHOD_H

#include <stdbool.h>

/* The focused editor's state, as an input method needs to see it.
 *
 * This is Maliit's "widget state" (MInputContext::getStateInformation): the
 * server keeps a copy of it and its keyboard predicts from that copy, so it has
 * to be pushed on every edit, not only when the panel is shown.
 *
 * surrounding_text is the committed text *without* the composing region, and
 * cursor/anchor are offsets into it. */
struct atl_im_state {
	bool focused;                 /* an editor accepts input right now */
	const char *surrounding_text; /* UTF-8, never NULL when focused */
	int cursor_position;
	int anchor_position;
	int input_type;               /* android.text.InputType bits */
	int ime_options;              /* android.view.inputmethod.EditorInfo.imeOptions */
};

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
	/* the editor state changed; focus_changed marks the edit that moved focus
	 * in or out, which is what makes a backend (de)activate its context */
	void (*update)(const struct atl_im_state *state, bool focus_changed);
	/* drop the input method's uncommitted text without committing it, and
	 * ignore whatever it sends about it afterwards */
	void (*reset)(void);
	/* request/dismiss the panel for the editor last passed to update() */
	void (*show)(void);
	void (*hide)(void);
};

/* Defined iff the backend is compiled in (conditional sources in meson);
 * declared weak so the framework can probe for them at runtime. */
extern const struct atl_im_backend atl_im_backend_maliit __attribute__((weak));

#endif
