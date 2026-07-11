/* Maliit D-Bus input-method backend (Lomiri on Ubuntu Touch, SailfishOS).
 *
 * These shells don't expose a text-input wayland protocol to clients; the
 * soft keyboard is driven over a peer-to-peer D-Bus connection published by
 * maliit-server (on Ubuntu Touch: lomiri-keyboard). libmaliit-glib wraps the
 * address handshake, the server proxy and the context object our callbacks
 * are invoked on. Everything runs on the GLib main loop, which is also
 * ATL's main loop, so the JNI upcalls are safe here.
 */
#include <stdbool.h>
#include <stdio.h>
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
#define TYPE_CLASS_TEXT                  1
#define TYPE_CLASS_NUMBER                2
#define TYPE_CLASS_PHONE                 3
#define TYPE_TEXT_VARIATION_EMAIL        0x020
#define TYPE_TEXT_VARIATION_PASSWORD     0x080
#define TYPE_TEXT_VARIATION_URI          0x010
#define TYPE_TEXT_VARIATION_WEB_EMAIL    0x0d0
#define TYPE_TEXT_VARIATION_WEB_PASSWORD 0x0e0
#define TYPE_NUMBER_VARIATION_PASSWORD   0x010

/* maliit content types (Maliit::TextContentType and friends) */
#define MALIIT_CONTENT_FREE_TEXT 0
#define MALIIT_CONTENT_NUMBER    1
#define MALIIT_CONTENT_PHONE     2
#define MALIIT_CONTENT_EMAIL     3
#define MALIIT_CONTENT_URL       4

static MaliitServer *server;
static MaliitContext *context;

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

static gboolean on_commit_string(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                 const gchar *string, int replacement_start,
                                 int replacement_length, int cursor_pos, gpointer user_data)
{
	atl_windows_ime_commit_text(string);
	return TRUE;
}

static gboolean on_update_preedit(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                  const gchar *string, GVariant *format_list_data,
                                  gint replace_start, gint replace_length, gint cursor_pos,
                                  gpointer user_data)
{
	/* We ask for prediction/correction to be disabled in the widget
	 * information, so the keyboard should commit directly; a composing-text
	 * UI needs InputConnection plumbing we don't have yet. */
	return TRUE;
}

static gboolean on_key_event(MaliitContext *obj, GDBusMethodInvocation *invocation,
                             gint type, gint key, gint modifiers, const gchar *text,
                             gboolean auto_repeat, int count, guchar request_type,
                             gpointer user_data)
{
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
		atl_windows_ime_commit_text(text);
	return TRUE;
}

static gboolean on_update_input_method_area(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                            gint x, gint y, gint width, gint height,
                                            gpointer user_data)
{
	atl_windows_set_ime_inset(height > 0 ? height : 0);
	return TRUE;
}

static gboolean on_im_initiated_hide(MaliitContext *obj, GDBusMethodInvocation *invocation,
                                     gpointer user_data)
{
	atl_windows_set_ime_inset(0);
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
	g_signal_connect(context, "handle-commit-string", G_CALLBACK(on_commit_string), NULL);
	g_signal_connect(context, "handle-update-preedit", G_CALLBACK(on_update_preedit), NULL);
	g_signal_connect(context, "handle-key-event", G_CALLBACK(on_key_event), NULL);
	g_signal_connect(context, "handle-update-input-method-area", G_CALLBACK(on_update_input_method_area), NULL);

	return true;
}

static void maliit_im_show(int input_type)
{
	GError *error = NULL;

	int klass = input_type & TYPE_MASK_CLASS;
	int variation = input_type & TYPE_MASK_VARIATION;
	int content_type = MALIIT_CONTENT_FREE_TEXT;
	gboolean hidden = FALSE;

	if (klass == TYPE_CLASS_NUMBER) {
		content_type = MALIIT_CONTENT_NUMBER;
		hidden = (variation == TYPE_NUMBER_VARIATION_PASSWORD);
	} else if (klass == TYPE_CLASS_PHONE) {
		content_type = MALIIT_CONTENT_PHONE;
	} else if (klass == TYPE_CLASS_TEXT) {
		if (variation == TYPE_TEXT_VARIATION_EMAIL || variation == TYPE_TEXT_VARIATION_WEB_EMAIL)
			content_type = MALIIT_CONTENT_EMAIL;
		else if (variation == TYPE_TEXT_VARIATION_URI)
			content_type = MALIIT_CONTENT_URL;
		hidden = (variation == TYPE_TEXT_VARIATION_PASSWORD ||
		          variation == TYPE_TEXT_VARIATION_WEB_PASSWORD);
	}

	/* Without composing-text support in the views, ask the keyboard to skip
	 * prediction/correction so text arrives as commit-string/key events. */
	GVariantBuilder state;
	g_variant_builder_init(&state, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&state, "{sv}", "focusState", g_variant_new_boolean(TRUE));
	g_variant_builder_add(&state, "{sv}", "contentType", g_variant_new_int32(content_type));
	g_variant_builder_add(&state, "{sv}", "hiddenText", g_variant_new_boolean(hidden));
	g_variant_builder_add(&state, "{sv}", "predictionEnabled", g_variant_new_boolean(FALSE));
	g_variant_builder_add(&state, "{sv}", "correctionEnabled", g_variant_new_boolean(FALSE));
	g_variant_builder_add(&state, "{sv}", "autocapitalizationEnabled", g_variant_new_boolean(FALSE));
	if (!maliit_server_call_update_widget_information_sync(server, g_variant_builder_end(&state),
	                                                       TRUE, NULL, &error)) {
		fprintf(stderr, "maliit: update_widget_information failed: %s\n", error->message);
		g_clear_error(&error);
	}

	if (!maliit_server_call_activate_context_sync(server, NULL, &error)) {
		fprintf(stderr, "maliit: activate_context failed: %s\n", error->message);
		g_clear_error(&error);
		return;
	}
	if (!maliit_server_call_show_input_method_sync(server, NULL, &error)) {
		fprintf(stderr, "maliit: show_input_method failed: %s\n", error->message);
		g_clear_error(&error);
	}
}

static void maliit_im_hide(void)
{
	GError *error = NULL;

	if (!maliit_server_call_reset_sync(server, NULL, &error)) {
		fprintf(stderr, "maliit: reset failed: %s\n", error->message);
		g_clear_error(&error);
	}
	if (!maliit_server_call_hide_input_method_sync(server, NULL, &error)) {
		fprintf(stderr, "maliit: hide_input_method failed: %s\n", error->message);
		g_clear_error(&error);
	}
	atl_windows_set_ime_inset(0);
}

const struct atl_im_backend atl_im_backend_maliit = {
	.name = "maliit",
	.init = maliit_im_init,
	.show = maliit_im_show,
	.hide = maliit_im_hide,
};
