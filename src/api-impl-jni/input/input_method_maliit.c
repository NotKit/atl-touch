/* Maliit D-Bus input-method backend (Lomiri on Ubuntu Touch, SailfishOS).
 *
 * These shells don't expose a text-input wayland protocol to clients; the
 * soft keyboard is driven over a peer-to-peer D-Bus connection published by
 * maliit-server (on Ubuntu Touch: lomiri-keyboard). libmaliit-glib wraps the
 * address handshake, the server proxy and the context object our callbacks
 * are invoked on. Everything runs on the GLib main loop, which is also
 * ATL's main loop, so the JNI upcalls are safe here.
 *
 * This follows Maliit's own Qt input context (input-context/minputcontext.cpp
 * in maliit/framework), which is the reference for the protocol's less obvious
 * requirements:
 *
 *  - The server keeps a copy of the editor state and its keyboard predicts from
 *    that copy, so updateWidgetInformation must carry the surrounding text and
 *    the cursor, and must be re-sent on every edit. Without it the keyboard
 *    goes on completing a word the app has already deleted.
 *  - reset() is asynchronous and racy: the keyboard may already have sent a
 *    commit for the preedit we are dropping. MInputContext ignores incoming
 *    commits and preedits until the reset call returns (pendingResets), and so
 *    do we — that is what stops removed text from coming back.
 *  - hideInputMethod is deferred by ~100 ms so that moving focus between two
 *    editors doesn't flap the panel.
 *  - Every method the server calls on the context has to be replied to. The
 *    handle-* signals stop emission once a handler returns TRUE, so a handler
 *    that doesn't complete the invocation also cuts off maliit-glib's own
 *    completing handlers, and the server blocks until D-Bus times it out.
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <maliit-glib/maliitbus.h>
#include <maliit-glib/maliitserver.h>
#include <maliit-glib/maliitcontext.h>

#include "input_method.h"
#include "../ATLWindow.h"

/* android KeyEvent actions/key codes (subset we synthesize) */
#define KEY_ACTION_DOWN 0
#define KEY_ACTION_UP   1

/* android inputType masks (android.text.InputType) */
#define TYPE_MASK_CLASS                  0x0000000f
#define TYPE_MASK_VARIATION              0x00000ff0
#define TYPE_MASK_FLAGS                  0x00fff000
#define TYPE_CLASS_TEXT                  1
#define TYPE_CLASS_NUMBER                2
#define TYPE_CLASS_PHONE                 3
#define TYPE_TEXT_VARIATION_EMAIL        0x020
#define TYPE_TEXT_VARIATION_PASSWORD     0x080
#define TYPE_TEXT_VARIATION_URI          0x010
#define TYPE_TEXT_VARIATION_WEB_EMAIL    0x0d0
#define TYPE_TEXT_VARIATION_WEB_PASSWORD 0x0e0
#define TYPE_TEXT_VARIATION_VISIBLE_PASSWORD 0x090
#define TYPE_NUMBER_VARIATION_PASSWORD   0x010
#define TYPE_TEXT_FLAG_AUTO_CORRECT      0x008000
#define TYPE_TEXT_FLAG_CAP_SENTENCES     0x004000
#define TYPE_TEXT_FLAG_CAP_WORDS         0x002000
#define TYPE_TEXT_FLAG_CAP_CHARACTERS    0x001000
#define TYPE_TEXT_FLAG_NO_SUGGESTIONS    0x080000

/* android EditorInfo.imeOptions -> the action key's label. Qt::EnterKeyType is
 * what maliit's keyboards read (EnterKeyDefault..EnterKeyPrevious). */
#define IME_MASK_ACTION   0x000000ff
#define IME_ACTION_GO     2
#define IME_ACTION_SEARCH 3
#define IME_ACTION_SEND   4
#define IME_ACTION_NEXT   5
#define IME_ACTION_DONE   6
#define IME_ACTION_PREVIOUS 7
#define IME_FLAG_NO_ENTER_ACTION 0x40000000

#define QT_ENTER_KEY_DEFAULT  0
#define QT_ENTER_KEY_RETURN   1
#define QT_ENTER_KEY_DONE     2
#define QT_ENTER_KEY_GO       3
#define QT_ENTER_KEY_SEND     4
#define QT_ENTER_KEY_SEARCH   5
#define QT_ENTER_KEY_NEXT     6
#define QT_ENTER_KEY_PREVIOUS 7

/* maliit content types (Maliit::TextContentType) */
#define MALIIT_CONTENT_FREE_TEXT 0
#define MALIIT_CONTENT_NUMBER    1
#define MALIIT_CONTENT_PHONE     2
#define MALIIT_CONTENT_EMAIL     3
#define MALIIT_CONTENT_URL       4

/* Maliit::EventRequestType */
#define MALIIT_EVENT_REQUEST_BOTH        0
#define MALIIT_EVENT_REQUEST_SIGNAL_ONLY 1

/* MInputContext's SoftwareInputPanelHideTimer */
#define HIDE_DELAY_MS 100

static MaliitServer *server;
static MaliitContext *context;

/* the server was told a context is active (MInputContext::active) */
static bool context_active;
/* what the last update() said, kept because show() and a reconnect both need to
 * re-send it; MInputContext re-queries the focus object instead */
static struct {
	bool focused;
	char *surrounding_text;
	int cursor_position;
	int anchor_position;
	int input_type;
	int ime_options;
} widget;

static enum { PANEL_HIDDEN, PANEL_SHOW_PENDING, PANEL_SHOWN } panel_state;
static guint hide_timer;

/* Our copy of the server's uncommitted text, and the reset calls still in
 * flight. Anything the server says about a preedit while a reset is pending
 * describes the preedit we just dropped, so it has to be ignored — this is the
 * guard that stops deleted text from being committed back. */
static char *preedit;
static int pending_resets;

/* The server can ask for hardware key events to be routed through it
 * (MInputContext::filterEvent). ATL has no hardware-keyboard plugin loaded, so
 * this is recorded and not acted on; the reply still has to be sent. */
static bool redirect_keys;

/* Qt key codes (lomiri-keyboard sends Qt codes) -> android key codes */
static const struct { guint32 qt; int android; } key_map[] = {
	{0x01000003, 67},  /* Key_Backspace -> KEYCODE_DEL */
	{0x01000004, 66},  /* Key_Return    -> KEYCODE_ENTER */
	{0x01000005, 66},  /* Key_Enter     -> KEYCODE_ENTER */
	{0x01000001, 61},  /* Key_Tab       -> KEYCODE_TAB */
	{0x01000012, 21},  /* Key_Left      -> KEYCODE_DPAD_LEFT */
	{0x01000013, 19},  /* Key_Up        -> KEYCODE_DPAD_UP */
	{0x01000014, 22},  /* Key_Right     -> KEYCODE_DPAD_RIGHT */
	{0x01000015, 20},  /* Key_Down      -> KEYCODE_DPAD_DOWN */
	{0x01000010, 122}, /* Key_Home      -> KEYCODE_MOVE_HOME */
	{0x01000011, 123}, /* Key_End       -> KEYCODE_MOVE_END */
	{0x01000007, 112}, /* Key_Delete    -> KEYCODE_FORWARD_DEL */
	{0x01000000, 111}, /* Key_Escape    -> KEYCODE_ESCAPE */
};

static void im_debug(const char *fmt, ...) G_GNUC_PRINTF(1, 2);

static void im_debug(const char *fmt, ...)
{
	if (!atl_debug_ime())
		return;
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "atl_ime: maliit: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

/* --- state pushed to the server ------------------------------------------ */

static int content_type_of(int input_type)
{
	int klass = input_type & TYPE_MASK_CLASS;
	int variation = input_type & TYPE_MASK_VARIATION;

	if (klass == TYPE_CLASS_NUMBER)
		return MALIIT_CONTENT_NUMBER;
	if (klass == TYPE_CLASS_PHONE)
		return MALIIT_CONTENT_PHONE;
	if (klass == TYPE_CLASS_TEXT) {
		if (variation == TYPE_TEXT_VARIATION_EMAIL || variation == TYPE_TEXT_VARIATION_WEB_EMAIL)
			return MALIIT_CONTENT_EMAIL;
		if (variation == TYPE_TEXT_VARIATION_URI)
			return MALIIT_CONTENT_URL;
	}
	return MALIIT_CONTENT_FREE_TEXT;
}

static bool is_hidden_text(int input_type)
{
	int klass = input_type & TYPE_MASK_CLASS;
	int variation = input_type & TYPE_MASK_VARIATION;

	if (klass == TYPE_CLASS_NUMBER)
		return variation == TYPE_NUMBER_VARIATION_PASSWORD;
	if (klass == TYPE_CLASS_TEXT)
		return variation == TYPE_TEXT_VARIATION_PASSWORD ||
		       variation == TYPE_TEXT_VARIATION_WEB_PASSWORD;
	return false;
}

static int enter_key_type_of(int ime_options)
{
	if (ime_options & IME_FLAG_NO_ENTER_ACTION)
		return QT_ENTER_KEY_DEFAULT;
	switch (ime_options & IME_MASK_ACTION) {
	case IME_ACTION_GO:       return QT_ENTER_KEY_GO;
	case IME_ACTION_SEARCH:   return QT_ENTER_KEY_SEARCH;
	case IME_ACTION_SEND:     return QT_ENTER_KEY_SEND;
	case IME_ACTION_NEXT:     return QT_ENTER_KEY_NEXT;
	case IME_ACTION_DONE:     return QT_ENTER_KEY_DONE;
	case IME_ACTION_PREVIOUS: return QT_ENTER_KEY_PREVIOUS;
	default:                  return QT_ENTER_KEY_DEFAULT;
	}
}

/* MInputContext::getStateInformation(). When nothing accepts input it sends
 * focusState alone — that is how the server learns the context was released. */
static GVariant *state_information(void)
{
	GVariantBuilder state;

	g_variant_builder_init(&state, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&state, "{sv}", "focusState", g_variant_new_boolean(widget.focused));
	if (!widget.focused)
		return g_variant_builder_end(&state);

	int input_type = widget.input_type;
	int flags = input_type & TYPE_MASK_FLAGS;
	int variation = input_type & TYPE_MASK_VARIATION;
	int cursor = widget.cursor_position, anchor = widget.anchor_position;
	gboolean password = is_hidden_text(input_type);
	/* Android and Qt state this in opposite directions: an android inputType
	 * asks for a behaviour, a Qt input hint suppresses one, and maliit's
	 * defaults follow Qt's (prediction and correction on unless told
	 * otherwise). So derive these from the flags that *forbid* them —
	 * TYPE_TEXT_FLAG_AUTO_CORRECT is a request, and reading it as a
	 * requirement would leave every ordinary field without correction. */
	gboolean no_predict = password || (flags & TYPE_TEXT_FLAG_NO_SUGGESTIONS) ||
	                      variation == TYPE_TEXT_VARIATION_VISIBLE_PASSWORD ||
	                      variation == TYPE_TEXT_VARIATION_URI ||
	                      content_type_of(input_type) == MALIIT_CONTENT_EMAIL ||
	                      (input_type & TYPE_MASK_CLASS) != TYPE_CLASS_TEXT;

	g_variant_builder_add(&state, "{sv}", "surroundingText",
	                      g_variant_new_string(widget.surrounding_text ? widget.surrounding_text : ""));
	g_variant_builder_add(&state, "{sv}", "cursorPosition", g_variant_new_int32(cursor));
	g_variant_builder_add(&state, "{sv}", "anchorPosition", g_variant_new_int32(anchor));
	g_variant_builder_add(&state, "{sv}", "hasSelection", g_variant_new_boolean(cursor != anchor));
	g_variant_builder_add(&state, "{sv}", "contentType", g_variant_new_int32(content_type_of(input_type)));
	g_variant_builder_add(&state, "{sv}", "hiddenText", g_variant_new_boolean(password));
	g_variant_builder_add(&state, "{sv}", "predictionEnabled", g_variant_new_boolean(!no_predict));
	g_variant_builder_add(&state, "{sv}", "correctionEnabled", g_variant_new_boolean(!no_predict));
	/* auto-capitalisation is the one android does state positively: no soft
	 * keyboard capitalises a field that did not ask for it */
	g_variant_builder_add(&state, "{sv}", "autocapitalizationEnabled",
	                      g_variant_new_boolean(!password &&
	                                            (flags & (TYPE_TEXT_FLAG_CAP_SENTENCES |
	                                                      TYPE_TEXT_FLAG_CAP_WORDS |
	                                                      TYPE_TEXT_FLAG_CAP_CHARACTERS)) != 0));
	g_variant_builder_add(&state, "{sv}", "enterKeyType",
	                      g_variant_new_int32(enter_key_type_of(widget.ime_options)));
	/* global attribute extension, registered on connect */
	g_variant_builder_add(&state, "{sv}", "toolbarId", g_variant_new_int32(0));
	return g_variant_builder_end(&state);
}

static void send_widget_information(bool focus_changed)
{
	if (!server)
		return;
	im_debug("updateWidgetInformation focus=%d focusChanged=%d cursor=%d anchor=%d text=%zu chars",
	         widget.focused, focus_changed, widget.cursor_position, widget.anchor_position,
	         widget.surrounding_text ? strlen(widget.surrounding_text) : 0);
	maliit_server_call_update_widget_information(server, state_information(), focus_changed,
	                                             NULL, NULL, NULL);
}

/* --- reset ---------------------------------------------------------------
 *
 * MInputContext::reset(): forget the preedit locally and tell the server to do
 * the same. When there was one, the call is tracked so that the commit the
 * keyboard may already have in flight for it is dropped instead of applied. */

static void on_reset_finished(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;

	if (!maliit_server_call_reset_finish(MALIIT_SERVER(source), res, &error)) {
		fprintf(stderr, "maliit: reset failed: %s\n", error ? error->message : "(unknown)");
		g_clear_error(&error);
	}
	if (pending_resets > 0)
		pending_resets--;
	im_debug("reset acknowledged, %d still pending", pending_resets);
}

static void maliit_im_reset(void)
{
	if (!server)
		return;

	bool had_preedit = preedit && *preedit;
	g_clear_pointer(&preedit, g_free);

	im_debug("reset (had preedit: %d)", had_preedit);
	if (had_preedit) {
		pending_resets++;
		maliit_server_call_reset(server, NULL, on_reset_finished, NULL);
	} else {
		maliit_server_call_reset(server, NULL, NULL, NULL);
	}
}

/* --- panel visibility ---------------------------------------------------- */

static gboolean on_hide_timeout(gpointer user_data)
{
	hide_timer = 0;
	if (server)
		maliit_server_call_hide_input_method(server, NULL, NULL, NULL);
	panel_state = PANEL_HIDDEN;
	atl_windows_set_ime_inset(0);
	return G_SOURCE_REMOVE;
}

static void cancel_hide(void)
{
	if (hide_timer) {
		g_source_remove(hide_timer);
		hide_timer = 0;
	}
}

static void maliit_im_show(void)
{
	if (!server)
		return;
	if (widget.focused)
		cancel_hide();

	/* Without an active context the server has no widget state to show the
	 * keyboard for; remember the request and issue it when focus arrives. */
	if (!context_active || !widget.focused) {
		panel_state = PANEL_SHOW_PENDING;
		im_debug("show deferred (active=%d focused=%d)", context_active, widget.focused);
		return;
	}
	im_debug("showInputMethod");
	maliit_server_call_show_input_method(server, NULL, NULL, NULL);
	panel_state = PANEL_SHOWN;
}

static void maliit_im_hide(void)
{
	if (!server)
		return;
	/* deferred, so that focus moving between two editors does not flap the
	 * panel: whoever gets focus next cancels this */
	cancel_hide();
	hide_timer = g_timeout_add(HIDE_DELAY_MS, on_hide_timeout, NULL);
}

/* --- state updates ------------------------------------------------------- */

static void maliit_im_update(const struct atl_im_state *state, bool focus_changed)
{
	bool was_focused = widget.focused;

	g_clear_pointer(&widget.surrounding_text, g_free);
	widget.focused = state->focused;
	widget.surrounding_text = g_strdup(state->surrounding_text ? state->surrounding_text : "");
	widget.cursor_position = state->cursor_position;
	widget.anchor_position = state->anchor_position;
	widget.input_type = state->input_type;
	widget.ime_options = state->ime_options;

	if (!server)
		return;

	if (!context_active && widget.focused) {
		im_debug("activateContext");
		maliit_server_call_activate_context(server, NULL, NULL, NULL);
		context_active = true;
	}

	/* Also report the update that clears focus, so the server stops predicting
	 * for an editor that is gone — that is the only notification it gets. */
	if (context_active && (widget.focused || was_focused))
		send_widget_information(focus_changed);

	if (panel_state == PANEL_SHOW_PENDING && widget.focused) {
		cancel_hide();
		maliit_server_call_show_input_method(server, NULL, NULL, NULL);
		panel_state = PANEL_SHOWN;
	}
}

/* --- calls from the server ----------------------------------------------- */

static gboolean on_commit_string(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                 const gchar *string, int replacement_start,
                                 int replacement_length, int cursor_pos, gpointer user_data)
{
	maliit_context_complete_commit_string(obj, invocation);
	if (pending_resets > 0) {
		im_debug("dropping commit '%s' across a pending reset", string ? string : "");
		return TRUE;
	}
	g_clear_pointer(&preedit, g_free);
	atl_windows_ime_commit_text(string ? string : "", replacement_start, replacement_length, cursor_pos);
	return TRUE;
}

static gboolean on_update_preedit(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                  const gchar *string, GVariant *format_list_data,
                                  gint replace_start, gint replace_length, gint cursor_pos,
                                  gpointer user_data)
{
	maliit_context_complete_update_preedit(obj, invocation);
	if (pending_resets > 0) {
		im_debug("dropping preedit '%s' across a pending reset", string ? string : "");
		return TRUE;
	}
	/* Show the in-progress word as composing (underlined) text; it is
	 * replaced in place by the next preedit update and finalized by
	 * commit-string. */
	g_free(preedit);
	preedit = g_strdup(string ? string : "");
	atl_windows_ime_set_composing(preedit, replace_start, replace_length, cursor_pos);
	return TRUE;
}

static gboolean on_key_event(MaliitContext *obj, GDBusMethodInvocation *invocation,
                             gint type, gint key, gint modifiers, const gchar *text,
                             gboolean auto_repeat, int count, guchar request_type,
                             gpointer user_data)
{
	maliit_context_complete_key_event(obj, invocation);

	/* the server only wants its own listeners notified, not the app */
	if (request_type == MALIIT_EVENT_REQUEST_SIGNAL_ONLY)
		return TRUE;

	/* QEvent::KeyPress = 6, QEvent::KeyRelease = 7 */
	int action = (type == 7) ? KEY_ACTION_UP : KEY_ACTION_DOWN;

	for (size_t i = 0; i < sizeof(key_map) / sizeof(key_map[0]); i++) {
		if ((guint32)key == key_map[i].qt) {
			atl_windows_ime_key(action, key_map[i].android);
			return TRUE;
		}
	}
	/* unmapped key carrying text (e.g. from a layout we don't know): commit
	 * the text once on press */
	if (action == KEY_ACTION_DOWN && text && *text)
		atl_windows_ime_commit_text(text, 0, 0, -1);
	return TRUE;
}

static gboolean on_update_input_method_area(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                            gint x, gint y, gint width, gint height,
                                            gpointer user_data)
{
	maliit_context_complete_update_input_method_area(obj, invocation);
	im_debug("area x=%d y=%d %dx%d", x, y, width, height);
	atl_windows_set_ime_inset(height > 0 ? height : 0);
	return TRUE;
}

static gboolean on_im_initiated_hide(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                     gpointer user_data)
{
	maliit_context_complete_im_initiated_hide(obj, invocation);
	im_debug("imInitiatedHide");
	panel_state = PANEL_HIDDEN;
	cancel_hide();
	atl_windows_set_ime_inset(0);
	/* the user dismissed the keyboard: drop the editor's focus, as
	 * MInputContext::imInitiatedHide does, so the app does not keep a caret in
	 * a field that can no longer be typed into */
	atl_windows_ime_initiated_hide();
	return TRUE;
}

static gboolean on_activation_lost(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                   gpointer user_data)
{
	maliit_context_complete_activation_lost_event(obj, invocation);
	im_debug("activation lost");
	context_active = false;
	panel_state = PANEL_HIDDEN;
	cancel_hide();
	g_clear_pointer(&preedit, g_free);
	atl_windows_set_ime_inset(0);
	atl_windows_ime_finish_composing();
	return TRUE;
}

static gboolean on_set_selection(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                 gint start, gint length, gpointer user_data)
{
	maliit_context_complete_set_selection(obj, invocation);
	atl_windows_ime_set_selection(start, length);
	return TRUE;
}

static gboolean on_selection(MaliitContext *obj, GDBusMethodInvocation *invocation,
                             gpointer user_data)
{
	char *selection = widget.focused ? atl_windows_ime_get_selection() : NULL;

	maliit_context_complete_selection(obj, invocation, selection != NULL, selection ? selection : "");
	g_free(selection);
	return TRUE;
}

static gboolean on_preedit_rectangle(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                     gpointer user_data)
{
	/* not supported, same as MInputContext::getPreeditRectangle */
	maliit_context_complete_preedit_rectangle(obj, invocation, FALSE, 0, 0, 0, 0);
	return TRUE;
}

static gboolean on_set_redirect_keys(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                     gboolean enabled, gpointer user_data)
{
	maliit_context_complete_set_redirect_keys(obj, invocation);
	redirect_keys = enabled;
	return TRUE;
}

static gboolean on_set_detectable_auto_repeat(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                              gboolean enabled, gpointer user_data)
{
	maliit_context_complete_set_detectable_auto_repeat(obj, invocation);
	return TRUE;
}

static gboolean on_set_global_correction_enabled(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                                 gboolean enabled, gpointer user_data)
{
	/* we don't inject preedits, so there is nothing to correct globally */
	maliit_context_complete_set_global_correction_enabled(obj, invocation);
	return TRUE;
}

static gboolean on_set_language(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                const gchar *language, gpointer user_data)
{
	maliit_context_complete_set_language(obj, invocation);
	im_debug("keyboard language is now %s", language ? language : "(none)");
	return TRUE;
}

static gboolean on_notify_extended_attribute_changed(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                                     gint id, const gchar *target, const gchar *target_item,
                                                     const gchar *attribute, GVariant *value,
                                                     gpointer user_data)
{
	maliit_context_complete_notify_extended_attribute_changed(obj, invocation);
	return TRUE;
}

static void on_invoke_action(MaliitServer *obj, const char *action, const char *sequence,
                             gpointer user_data)
{
}

static bool maliit_im_init(void)
{
	GError *error = NULL;

	server = maliit_get_server_sync(NULL, &error);
	if (!server) {
		fprintf(stderr, "maliit: no server: %s\n", error ? error->message : "(unknown)");
		g_clear_error(&error);
		return false;
	}
	g_object_ref(server);
	g_signal_connect(server, "invoke-action", G_CALLBACK(on_invoke_action), NULL);

	context = maliit_get_context_sync(NULL, &error);
	if (!context) {
		fprintf(stderr, "maliit: no context: %s\n", error ? error->message : "(unknown)");
		g_clear_error(&error);
		g_clear_object(&server);
		return false;
	}
	g_object_ref(context);
	g_signal_connect(context, "handle-im-initiated-hide", G_CALLBACK(on_im_initiated_hide), NULL);
	g_signal_connect(context, "handle-activation-lost-event", G_CALLBACK(on_activation_lost), NULL);
	g_signal_connect(context, "handle-commit-string", G_CALLBACK(on_commit_string), NULL);
	g_signal_connect(context, "handle-update-preedit", G_CALLBACK(on_update_preedit), NULL);
	g_signal_connect(context, "handle-key-event", G_CALLBACK(on_key_event), NULL);
	g_signal_connect(context, "handle-update-input-method-area", G_CALLBACK(on_update_input_method_area), NULL);
	g_signal_connect(context, "handle-set-selection", G_CALLBACK(on_set_selection), NULL);
	g_signal_connect(context, "handle-selection", G_CALLBACK(on_selection), NULL);
	g_signal_connect(context, "handle-preedit-rectangle", G_CALLBACK(on_preedit_rectangle), NULL);
	g_signal_connect(context, "handle-set-redirect-keys", G_CALLBACK(on_set_redirect_keys), NULL);
	g_signal_connect(context, "handle-set-detectable-auto-repeat", G_CALLBACK(on_set_detectable_auto_repeat), NULL);
	g_signal_connect(context, "handle-set-global-correction-enabled", G_CALLBACK(on_set_global_correction_enabled), NULL);
	g_signal_connect(context, "handle-set-language", G_CALLBACK(on_set_language), NULL);
	g_signal_connect(context, "handle-notify-extended-attribute-changed",
	                 G_CALLBACK(on_notify_extended_attribute_changed), NULL);

	/* one attribute extension for everything, as MInputContext does on connect;
	 * the state's toolbarId refers to it */
	maliit_server_call_register_attribute_extension(server, 0, "", NULL, NULL, NULL);

	return true;
}

const struct atl_im_backend atl_im_backend_maliit = {
	.name = "maliit",
	.init = maliit_im_init,
	.update = maliit_im_update,
	.reset = maliit_im_reset,
	.show = maliit_im_show,
	.hide = maliit_im_hide,
};
