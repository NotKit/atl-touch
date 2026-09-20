/* content-hub clipboard client for Ubuntu Touch, see content_hub_clipboard.h.
 *
 * Only the two pasteboard methods of the content-hub service are used, so this
 * talks to them over plain GDBus instead of linking content-hub-glib (which is
 * in the arm64 UT rootfs only, see android_atl_ATLContentHub.c). Everything
 * here is therefore built on the desktop too, where it stays inert.
 */
#include "content_hub_clipboard.h"

#include <gio/gio.h>
#include <glib.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGE(...) do { fprintf(stderr, "[atl-clip] " __VA_ARGS__); fputc('\n', stderr); } while (0)

/* Well-known bus name + object path the content-hub daemon registers. */
static const char *SERVICE_BUS_NAME = "com.lomiri.content.dbus.Service";
static const char *SERVICE_OBJECT_PATH = "/";
static const char *SERVICE_INTERFACE = "com.lomiri.content.dbus.Service";

/* Wayland clients have no Mir surface id to offer, see the header. */
static const char *EMPTY_SURFACE_ID = "";

#define MIME_TYPE_TEXT "text/plain"

/* The hub keeps the pastes in memory, so answers come back right away. Keep
 * the wait short anyway: these calls block the thread that copied or pasted. */
#define CALL_TIMEOUT_MS 2000

/* Both as in content-hub's src/com/lomiri/content/utils.cpp. */
#define MAX_FORMAT_COUNT 16
#define MAX_BUFFER_SIZE (4 * 1024 * 1024)

static bool debug(void)
{
	static int on = -1;
	if (on < 0)
		on = getenv("ATL_DEBUG_CLIPBOARD") != NULL;
	return on;
}

/* The session bus, connected to on first use. NULL once we know there is no
 * content-hub to talk to, which is every desktop and every older UT. */
static GDBusConnection *bus;
static bool bus_tried;
static bool hub_gone;

static GDBusConnection *connection(void)
{
	if (hub_gone)
		return NULL;
	if (bus_tried)
		return bus;
	bus_tried = true;

	const char *enabled = getenv("ATL_CONTENT_HUB_CLIPBOARD");
	if (enabled && !strcmp(enabled, "0")) {
		hub_gone = true;
		return NULL;
	}

	GError *err = NULL;
	bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
	if (!bus) {
		if (debug())
			LOGE("no session bus: %s", err->message);
		g_error_free(err);
		hub_gone = true;
	}
	return bus;
}

/* Stop trying once the bus tells us nobody owns the name: there is no
 * content-hub here, and every copy and paste would pay for a round trip. */
static void forget_hub(const GError *err)
{
	if (g_error_matches(err, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN)
	    || g_error_matches(err, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER))
		hub_gone = true;
}

/* content-hub labels a paste with the id of the app that made it. A click app
 * gets one from the launcher; its AppArmor profile carries the same string,
 * and is what the hub compares against. Caller frees. */
static char *app_id(void)
{
	const char *env = getenv("APP_ID");
	if (env && *env)
		return g_strdup(env);

	char *label = NULL;
	if (!g_file_get_contents("/proc/self/attr/current", &label, NULL, NULL))
		return g_strdup("");

	g_strstrip(label);
	/* The label reads "profile (mode)" while a mode is set. */
	char *mode = strstr(label, " (");
	if (mode)
		*mode = '\0';
	if (!strcmp(label, "unconfined"))
		*label = '\0';
	return label;
}

static int32_t read_int(const unsigned char *blob, size_t index)
{
	int32_t value;
	memcpy(&value, blob + index * sizeof(int32_t), sizeof(int32_t));
	return value;
}

/* A paste is a serialized QMimeData: an int32 format count, then four int32
 * per format (format offset, format size, data offset, data size), then the
 * bytes they point at. Nothing in it is Qt-specific. Caller g_free()s. */
static unsigned char *serialize_paste(const char *text, size_t *out_size)
{
	size_t format_size = strlen(MIME_TYPE_TEXT);
	size_t text_size = strlen(text);
	size_t header_size = sizeof(int32_t) * 5;
	size_t size = header_size + format_size + text_size;

	if (size > MAX_BUFFER_SIZE) {
		LOGE("not publishing %zu bytes, over content-hub's %d byte limit", size, MAX_BUFFER_SIZE);
		return NULL;
	}

	int32_t header[] = {
		1,
		(int32_t)header_size,
		(int32_t)format_size,
		(int32_t)(header_size + format_size),
		(int32_t)text_size,
	};
	unsigned char *blob = g_malloc(size);
	memcpy(blob, header, header_size);
	memcpy(blob + header[1], MIME_TYPE_TEXT, format_size);
	memcpy(blob + header[3], text, text_size);
	*out_size = size;
	return blob;
}

/* The text/plain entry of a serialized QMimeData, or NULL. Caller g_free()s. */
static char *parse_paste(const unsigned char *blob, size_t size)
{
	if (size < sizeof(int32_t))
		return NULL;

	int32_t count = MIN(read_int(blob, 0), MAX_FORMAT_COUNT);
	if (count <= 0 || size < sizeof(int32_t) * (1 + 4 * (size_t)count))
		return NULL;

	char *text = NULL;
	for (int32_t i = 0; i < count; i++) {
		int32_t format_offset = read_int(blob, i * 4 + 1);
		int32_t format_size = read_int(blob, i * 4 + 2);
		int32_t data_offset = read_int(blob, i * 4 + 3);
		int32_t data_size = read_int(blob, i * 4 + 4);
		if (format_offset < 0 || format_size < 0 || data_offset < 0 || data_size < 0
		    || (size_t)format_offset + format_size > size
		    || (size_t)data_offset + data_size > size) {
			g_free(text);
			return NULL;
		}

		/* Qt writes the plain text as "text/plain", other toolkits add a
		 * charset; either will do, but plain "text/plain" is the UTF-8 one. */
		char *format = g_strndup((const char *)blob + format_offset, format_size);
		bool exact = !strcmp(format, MIME_TYPE_TEXT);
		bool with_charset = g_str_has_prefix(format, MIME_TYPE_TEXT ";");
		g_free(format);
		if (!exact && !with_charset)
			continue;

		g_free(text);
		text = g_strndup((const char *)blob + data_offset, data_size);
		if (exact)
			break;
	}
	return text;
}

bool atl_content_hub_clipboard_set(const char *text)
{
	GDBusConnection *conn = connection();
	if (!conn || !text)
		return false;

	size_t size = 0;
	unsigned char *blob = serialize_paste(text, &size);
	if (!blob)
		return false;

	GVariant *mime_data = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, blob, size, sizeof(unsigned char));
	const char *types[] = {MIME_TYPE_TEXT, NULL};
	char *id = app_id();

	GError *err = NULL;
	GVariant *reply = g_dbus_connection_call_sync(conn, SERVICE_BUS_NAME, SERVICE_OBJECT_PATH, SERVICE_INTERFACE,
	                                              "CreatePaste",
	                                              g_variant_new("(ss@ay^as)", id, EMPTY_SURFACE_ID, mime_data, types),
	                                              G_VARIANT_TYPE("(b)"), G_DBUS_CALL_FLAGS_NONE, CALL_TIMEOUT_MS,
	                                              NULL, &err);
	g_free(id);
	g_free(blob);
	if (!reply) {
		if (debug())
			LOGE("CreatePaste failed: %s", err->message);
		forget_hub(err);
		g_error_free(err);
		return false;
	}

	gboolean created = FALSE;
	g_variant_get(reply, "(b)", &created);
	g_variant_unref(reply);
	if (!created && debug()) {
		/* Either the hub does not know about callers without a surface id, or
		 * it does and we are not the focused app. */
		LOGE("content-hub refused the clipboard contents");
	}
	return created;
}

char *atl_content_hub_clipboard_get(void)
{
	GDBusConnection *conn = connection();
	if (!conn)
		return NULL;

	GError *err = NULL;
	GVariant *reply = g_dbus_connection_call_sync(conn, SERVICE_BUS_NAME, SERVICE_OBJECT_PATH, SERVICE_INTERFACE,
	                                              "GetLatestPasteData", g_variant_new("(s)", EMPTY_SURFACE_ID),
	                                              G_VARIANT_TYPE("(ay)"), G_DBUS_CALL_FLAGS_NONE, CALL_TIMEOUT_MS,
	                                              NULL, &err);
	if (!reply) {
		if (debug())
			LOGE("GetLatestPasteData failed: %s", err->message);
		forget_hub(err);
		g_error_free(err);
		return NULL;
	}

	GVariant *blob = g_variant_get_child_value(reply, 0);
	gsize size = 0;
	const unsigned char *data = g_variant_get_fixed_array(blob, &size, sizeof(unsigned char));
	char *text = data ? parse_paste(data, size) : NULL;
	g_variant_unref(blob);
	g_variant_unref(reply);
	return text;
}
