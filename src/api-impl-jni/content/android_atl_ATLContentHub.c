/* content-hub import client for Ubuntu Touch.
 *
 * content-hub is UT's inter-app content broker: the only sanctioned way a
 * confined app imports files/media. It enumerates the apps that can supply a
 * content type and runs the transfer; the source-app picker UI is drawn by us
 * (android.atl.ATLContentHubPicker). Ported from fluffychat's
 * content_hub_file_picker elinux plugin, with the state-changed abort fix.
 *
 * Built only on device (see meson.build: content-hub-glib is present in the
 * arm64 UT rootfs, not on the x86_64 desktop). When it isn't compiled in, the
 * Java native methods stay unresolved and ATLMediaContentProvider falls back
 * to ATLFilePicker.
 */
#define _GNU_SOURCE

#include <com/lomiri/content/glib/content-hub-glib.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../generated_headers/android_atl_ATLContentHub.h"

#define LOGE(...) do { fprintf(stderr, "[atl-ch] " __VA_ARGS__); fputc('\n', stderr); } while (0)
#define LOGI LOGE

/* Well-known bus name + object path the content-hub daemon registers. */
static const char *SERVICE_BUS_NAME = "com.lomiri.content.dbus.Service";
static const char *SERVICE_OBJECT_PATH = "/";

/* Transfer::State::aborted from transfer.h. */
#define CH_STATE_ABORTED 5

typedef struct {
	char          *id;         /* peer app id, e.g. "com.ubuntu.gallery_gallery" */
	char          *name;       /* display name; falls back to id */
	char          *icon_path;  /* icon file path or name; may be "" */
	unsigned char *icon;       /* raw icon bytes if the peer carries them, else NULL */
	size_t         icon_len;
} ch_peer;

/* ------------------------------------------------------------------ helpers */

/* Directory picked files are moved into. g_get_user_cache_dir() honors
 * XDG_CACHE_HOME, which on UT is the app-namespaced cache — the same
 * filesystem HubIncoming lives on, so a g_file_move is a rename. */
static char *temp_dir(void)
{
	return g_build_filename(g_get_user_cache_dir(), "content_hub_import", NULL);
}

static void remove_recursive(const char *path)
{
	GDir *dir = g_dir_open(path, 0, NULL);
	if (dir) {
		const gchar *name;
		while ((name = g_dir_read_name(dir))) {
			char *child = g_build_filename(path, name, NULL);
			if (g_file_test(child, G_FILE_TEST_IS_DIR) && !g_file_test(child, G_FILE_TEST_IS_SYMLINK))
				remove_recursive(child);
			else
				g_unlink(child);
			g_free(child);
		}
		g_dir_close(dir);
	}
	g_rmdir(path);
}

/* Strip trailing junk and the " (mode)" suffix from an AppArmor context. */
static void clean_apparmor_label(char *label)
{
	size_t n = strlen(label);
	while (n > 0 && (label[n-1] == '\n' || label[n-1] == ' ' || label[n-1] == '\0'))
		label[--n] = '\0';
	char *paren = strrchr(label, ' ');
	if (paren && paren > label && paren[1] == '(')
		*paren = '\0';
}

/* Our AppArmor profile exactly as content-hub sees it: the LinuxSecurityLabel
 * of our own bus connection, falling back to /proc/self/attr/current. For a
 * click this is a named profile even under an unconfined template — content-hub
 * rejects a mismatched app_id both when creating the transfer and when
 * registering the handler, so never hardcode it. Caller frees. */
static char *get_apparmor_label(GDBusConnection *conn)
{
	char *label = NULL;

	const char *unique = g_dbus_connection_get_unique_name(conn);
	if (unique) {
		GVariant *reply = g_dbus_connection_call_sync(
		    conn, "org.freedesktop.DBus", "/org/freedesktop/DBus",
		    "org.freedesktop.DBus", "GetConnectionCredentials",
		    g_variant_new("(s)", unique), G_VARIANT_TYPE("(a{sv})"),
		    G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
		if (reply) {
			GVariant *dict = g_variant_get_child_value(reply, 0);
			GVariant *lsl = g_variant_lookup_value(dict, "LinuxSecurityLabel",
			                                       G_VARIANT_TYPE_BYTESTRING);
			if (lsl) {
				gsize n = 0;
				const char *data = g_variant_get_fixed_array(lsl, &n, sizeof(char));
				if (data && n > 0) label = g_strndup(data, n);
				g_variant_unref(lsl);
			}
			g_variant_unref(dict);
			g_variant_unref(reply);
		}
	}

	if (!label) {
		FILE *f = fopen("/proc/self/attr/current", "r");
		if (f) {
			char buf[512] = {0};
			if (fgets(buf, sizeof buf, f)) label = g_strdup(buf);
			fclose(f);
		}
	}

	if (label) {
		clean_apparmor_label(label);
		if (*label) return label;
		g_free(label);
	}
	return g_strdup("unconfined");
}

/* Turn an arbitrary id into a valid D-Bus object-path element. */
static char *sanitize_path_element(const char *in)
{
	if (!in || !*in) return g_strdup("app");
	char *out = g_strdup(in);
	for (char *p = out; *p; ++p)
		if (!g_ascii_isalnum(*p)) *p = '_';
	return out;
}

/* Extract the file URL from one collected Item. content-hub marshals an Item
 * as (streamType:s, stream:ay, name:s, url:s) — the URL is the LAST field.
 * Prefer a string child that looks like a URL/path, else the last non-empty
 * string child. Caller frees. */
static char *item_url(GVariant *element)
{
	GVariant *inner = g_variant_is_of_type(element, G_VARIANT_TYPE_VARIANT)
	                      ? g_variant_get_variant(element)
	                      : g_variant_ref(element);
	char *url = NULL;
	if (inner && g_variant_is_of_type(inner, G_VARIANT_TYPE_TUPLE)) {
		gsize n = g_variant_n_children(inner);
		for (gsize i = 0; i < n; ++i) {
			GVariant *child = g_variant_get_child_value(inner, i);
			if (g_variant_is_of_type(child, G_VARIANT_TYPE_STRING)) {
				const char *s = g_variant_get_string(child, NULL);
				if (s && (strstr(s, "://") || s[0] == '/')) {
					g_free(url);
					url = g_strdup(s);
					g_variant_unref(child);
					break;
				}
				if (s && *s) { g_free(url); url = g_strdup(s); }
			}
			g_variant_unref(child);
		}
	}
	if (inner) g_variant_unref(inner);
	return url;
}

/* Fill one ch_peer from a Peer variant. The struct field order is not pinned
 * across content-hub versions, so walk children defensively: string children
 * are id/name/iconName in order; a byte-array child is icon data. Returns 0 if
 * an id was found. */
static int peer_from_variant(GVariant *element, ch_peer *out)
{
	GVariant *inner = g_variant_is_of_type(element, G_VARIANT_TYPE_VARIANT)
	                      ? g_variant_get_variant(element)
	                      : g_variant_ref(element);
	if (!inner) return -1;

	memset(out, 0, sizeof *out);
	int nstrings = 0;
	if (g_variant_is_of_type(inner, G_VARIANT_TYPE_TUPLE)) {
		gsize n = g_variant_n_children(inner);
		for (gsize i = 0; i < n; ++i) {
			GVariant *child = g_variant_get_child_value(inner, i);
			if (g_variant_is_of_type(child, G_VARIANT_TYPE_STRING)) {
				const char *s = g_variant_get_string(child, NULL);
				switch (nstrings++) {
					case 0: out->id = g_strdup(s ? s : ""); break;
					case 1: out->name = g_strdup(s ? s : ""); break;
					case 2: out->icon_path = g_strdup(s ? s : ""); break;
					default: break;
				}
			} else if (g_variant_is_of_type(child, G_VARIANT_TYPE("ay"))) {
				gsize len = 0;
				const guchar *data = g_variant_get_fixed_array(child, &len, sizeof(guchar));
				if (data && len > 0 && !out->icon) {
					out->icon = g_memdup2(data, len);
					out->icon_len = len;
				}
			}
			g_variant_unref(child);
		}
	}
	g_variant_unref(inner);

	if (!out->id || !*out->id) {
		g_free(out->id); g_free(out->name); g_free(out->icon_path); g_free(out->icon);
		memset(out, 0, sizeof *out);
		return -1;
	}
	if (!out->name || !*out->name) { g_free(out->name); out->name = g_strdup(out->id); }
	if (!out->icon_path) out->icon_path = g_strdup("");
	return 0;
}

/* ------------------------------------------------------------- list sources */

static int ch_list_sources(const char *content_type, ch_peer **out, size_t *count)
{
	*out = NULL;
	*count = 0;

	GError *err = NULL;
	GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
	if (!conn) {
		LOGE("session bus: %s", err ? err->message : "?");
		g_clear_error(&err);
		return -1;
	}

	ContentHubService *service = content_hub_service_proxy_new_sync(
	    conn, G_DBUS_PROXY_FLAGS_NONE, SERVICE_BUS_NAME, SERVICE_OBJECT_PATH, NULL, &err);
	if (!service) {
		LOGE("service proxy: %s", err ? err->message : "?");
		g_clear_error(&err);
		g_object_unref(conn);
		return -1;
	}

	GVariant *peers = NULL;
	if (!content_hub_service_call_known_sources_for_type_sync(
	        service, content_type, &peers, NULL, &err) || !peers) {
		LOGE("known_sources_for_type(%s): %s", content_type, err ? err->message : "?");
		g_clear_error(&err);
		g_object_unref(service);
		g_object_unref(conn);
		return -1;
	}

	/* 'av': array of variants each wrapping a Peer struct. */
	gsize max = g_variant_n_children(peers);
	ch_peer *list = g_new0(ch_peer, max ? max : 1);
	size_t n = 0;
	GVariantIter iter;
	g_variant_iter_init(&iter, peers);
	GVariant *element;
	while ((element = g_variant_iter_next_value(&iter))) {
		if (peer_from_variant(element, &list[n]) == 0) n++;
		g_variant_unref(element);
	}
	g_variant_unref(peers);
	g_object_unref(service);
	g_object_unref(conn);

	for (size_t i = 0; i < n; ++i)
		LOGI("source[%zu/%zu] id='%s' name='%s' icon=%zu bytes",
		     i + 1, n, list[i].id, list[i].name, list[i].icon_len);

	*out = list;
	*count = n;
	return 0;
}

static void ch_peers_free(ch_peer *peers, size_t count)
{
	if (!peers) return;
	for (size_t i = 0; i < count; ++i) {
		g_free(peers[i].id);
		g_free(peers[i].name);
		g_free(peers[i].icon_path);
		g_free(peers[i].icon);
	}
	g_free(peers);
}

/* ------------------------------------------------------------------- import */

typedef struct {
	GMainLoop *loop;
	GPtrArray *paths;    /* char*, moved-out local paths */
	int        aborted;  /* source app / daemon aborted the transfer */
	int        cancelled;/* our own UI cancelled */
	int        done;     /* handle-import ran to completion */
	char      *tmp;      /* our cache dir the files are moved into */
	guint      serial;   /* this import's number, part of the temp path */
} pick_state;

/* Bumped per import so two imports never land on the same path: an app that
 * caches thumbnails by file path would otherwise show the previous picture. */
static guint import_serial;

/* The single in-flight import, so a cancel from another thread can unblock it.
 * g_main_loop_quit is thread-safe. */
static GMutex g_import_lock;
static pick_state *g_current_import;

static void ch_import_cancel(void)
{
	g_mutex_lock(&g_import_lock);
	if (g_current_import) {
		g_current_import->cancelled = 1;
		g_main_loop_quit(g_current_import->loop);
	}
	g_mutex_unlock(&g_import_lock);
}

/* Fires on our private context when the source app finishes and content-hub
 * delivers the import. Moves every file out of HubIncoming before finalize()
 * — the default transient scope purges HubIncoming on finalize. */
static gboolean on_handle_import(ContentHubHandler *handler,
                                 GDBusMethodInvocation *invocation,
                                 const gchar *transfer_path,
                                 gpointer user_data)
{
	pick_state *st = user_data;

	GError *err = NULL;
	ContentHubTransfer *transfer = content_hub_transfer_proxy_new_sync(
	    g_dbus_method_invocation_get_connection(invocation),
	    G_DBUS_PROXY_FLAGS_NONE, SERVICE_BUS_NAME, transfer_path, NULL, &err);

	if (transfer && !err) {
		GVariant *items = NULL;
		if (content_hub_transfer_call_collect_sync(transfer, &items, NULL, &err) && items) {
			g_mkdir_with_parents(st->tmp, 0700);
			GVariantIter iter;
			g_variant_iter_init(&iter, items);
			GVariant *element;
			int idx = 0;
			int all_moved = 1;
			while ((element = g_variant_iter_next_value(&iter))) {
				char *url = item_url(element);
				g_variant_unref(element);
				if (!url) continue;

				GFile *src = g_str_has_prefix(url, "file://")
				                 ? g_file_new_for_uri(url)
				                 : g_file_new_for_path(url);
				/* Keep the original basename; a numbered subdir per file avoids
				 * collisions within a multi-select. */
				char *sub = g_strdup_printf("%s/%u-%d", st->tmp, st->serial, idx++);
				g_mkdir_with_parents(sub, 0700);
				char *base = g_file_get_basename(src);
				char *dest = g_build_filename(sub, base && *base ? base : "file", NULL);
				g_free(base);
				g_free(sub);

				GFile *dst = g_file_new_for_path(dest);
				GError *move_err = NULL;
				if (g_file_move(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &move_err)) {
					g_ptr_array_add(st->paths, dest);
				} else {
					LOGE("move failed: %s", move_err ? move_err->message : "?");
					all_moved = 0;
					/* Fall back to wherever content-hub left it. */
					char *fallback = g_str_has_prefix(url, "file://")
					                     ? g_filename_from_uri(url, NULL, NULL)
					                     : g_strdup(url);
					if (fallback) g_ptr_array_add(st->paths, fallback);
					g_free(dest);
				}
				g_clear_error(&move_err);
				g_object_unref(src);
				g_object_unref(dst);
				g_free(url);
			}
			g_variant_unref(items);

			/* finalize purges HubIncoming (transient scope) — only safe once
			 * every file has been moved out. */
			if (all_moved)
				content_hub_transfer_call_finalize_sync(transfer, NULL, NULL);
		}
		g_object_unref(transfer);
	}
	if (err) {
		LOGE("collect: %s", err->message);
		g_error_free(err);
	}

	content_hub_handler_complete_handle_import(handler, invocation);
	st->done = 1;
	g_main_loop_quit(st->loop);
	return TRUE;
}

/* Not for us; complete so the daemon isn't left waiting. */
static gboolean on_handle_export(ContentHubHandler *handler,
                                 GDBusMethodInvocation *invocation,
                                 const gchar *path, gpointer user_data)
{
	(void)path; (void)user_data;
	content_hub_handler_complete_handle_export(handler, invocation);
	return TRUE;
}
static gboolean on_handle_share(ContentHubHandler *handler,
                                GDBusMethodInvocation *invocation,
                                const gchar *path, gpointer user_data)
{
	(void)path; (void)user_data;
	content_hub_handler_complete_handle_share(handler, invocation);
	return TRUE;
}

/* Transfer state watch. content-hub reports state via the StateChanged D-Bus
 * signal ("state-changed" on the proxy) — NOT a property, so a
 * g-properties-changed watch never fires (that was a latent hang: cancelling
 * in the source app left the import blocked forever). */
static void on_transfer_state_changed(ContentHubTransfer *transfer, gint state,
                                      gpointer user_data)
{
	(void)transfer;
	pick_state *st = user_data;
	if (state == CH_STATE_ABORTED) {
		st->aborted = 1;
		g_main_loop_quit(st->loop);
	}
}

static char **ch_import(const char *peer_id, const char *content_type, int selection_type)
{
	GError *err = NULL;
	GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &err);
	if (!conn) {
		LOGE("session bus: %s", err ? err->message : "?");
		g_clear_error(&err);
		return NULL;
	}

	char *app_id = get_apparmor_label(conn);

	ContentHubService *service = content_hub_service_proxy_new_sync(
	    conn, G_DBUS_PROXY_FLAGS_NONE, SERVICE_BUS_NAME, SERVICE_OBJECT_PATH, NULL, &err);
	if (!service) {
		LOGE("service proxy: %s", err ? err->message : "?");
		g_clear_error(&err);
		g_free(app_id);
		g_object_unref(conn);
		return NULL;
	}

	gchar *transfer_path = NULL;
	if (!content_hub_service_call_create_import_from_peer_sync(
	        service, peer_id, app_id, content_type, &transfer_path, NULL, &err)
	    || !transfer_path) {
		LOGE("create_import_from_peer (app_id='%s'): %s", app_id, err ? err->message : "?");
		g_clear_error(&err);
		g_free(app_id);
		g_object_unref(service);
		g_object_unref(conn);
		return NULL;
	}

	/* Everything D-Bus from here runs on a private context so the blocking
	 * wait never collides with any default-context pump on another thread. */
	GMainContext *ctx = g_main_context_new();
	g_main_context_push_thread_default(ctx);

	char **result = NULL;
	GMainLoop *loop = g_main_loop_new(ctx, FALSE);
	pick_state st = { .loop = loop, .paths = g_ptr_array_new_with_free_func(g_free),
	                  .aborted = 0, .tmp = temp_dir(),
	                  .serial = g_atomic_int_add(&import_serial, 1) };

	/* Serials restart at 0 each launch, so clear what the last run left behind
	 * before reusing the numbers. Files from this run are never touched. */
	if (st.serial == 0)
		remove_recursive(st.tmp);

	ContentHubTransfer *transfer = content_hub_transfer_proxy_new_sync(
	    conn, G_DBUS_PROXY_FLAGS_NONE, SERVICE_BUS_NAME, transfer_path, NULL, &err);
	g_free(transfer_path);
	if (!transfer) {
		LOGE("transfer proxy: %s", err ? err->message : "?");
		g_clear_error(&err);
		goto out;
	}

	content_hub_transfer_call_set_selection_type_sync(transfer, selection_type, NULL, NULL);
	g_signal_connect(transfer, "state-changed",
	                 G_CALLBACK(on_transfer_state_changed), &st);

	/* Export our handler skeleton and register it so the daemon knows where to
	 * deliver the import callback. */
	ContentHubHandler *handler = content_hub_handler_skeleton_new();
	g_signal_connect(handler, "handle-handle-import", G_CALLBACK(on_handle_import), &st);
	g_signal_connect(handler, "handle-handle-export", G_CALLBACK(on_handle_export), NULL);
	g_signal_connect(handler, "handle-handle-share",  G_CALLBACK(on_handle_share),  NULL);

	char *elem = sanitize_path_element(app_id);
	char *handler_path = g_strdup_printf("/com/lomiri/content/handler/%s", elem);
	g_free(elem);

	if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(handler),
	                                      conn, handler_path, &err)) {
		LOGE("handler export: %s", err ? err->message : "?");
		g_clear_error(&err);
		g_free(handler_path);
		g_object_unref(handler);
		g_object_unref(transfer);
		goto out;
	}

	GError *reg_err = NULL;
	if (!content_hub_service_call_register_import_export_handler_sync(
	        service, app_id, handler_path, NULL, &reg_err)) {
		LOGE("register handler: %s", reg_err ? reg_err->message : "?");
	}
	g_clear_error(&reg_err);
	g_free(handler_path);

	/* "initiated": content-hub foregrounds the source app; the user picks. */
	g_mutex_lock(&g_import_lock);
	g_current_import = &st;
	g_mutex_unlock(&g_import_lock);

	if (content_hub_transfer_call_start_sync(transfer, NULL, &err)) {
		if (!st.cancelled)
			g_main_loop_run(loop);
	} else {
		LOGE("start: %s", err ? err->message : "?");
		g_clear_error(&err);
	}

	g_mutex_lock(&g_import_lock);
	g_current_import = NULL;
	g_mutex_unlock(&g_import_lock);

	/* Tell the daemon if the transfer didn't finish — it closes the source
	 * app's picker and tears the transfer down. */
	if (!st.done && (st.cancelled || !st.aborted))
		content_hub_transfer_call_abort_sync(transfer, NULL, NULL);

	g_dbus_interface_skeleton_unexport(G_DBUS_INTERFACE_SKELETON(handler));
	g_object_unref(handler);
	g_object_unref(transfer);

	if (!st.aborted && st.paths->len > 0) {
		g_ptr_array_add(st.paths, NULL);
		result = (char **)g_ptr_array_steal(st.paths, NULL);   /* transfer ownership */
	}

out:
	g_ptr_array_free(st.paths, TRUE);
	g_free(st.tmp);
	g_main_loop_unref(loop);
	g_main_context_pop_thread_default(ctx);
	g_main_context_unref(ctx);
	g_free(app_id);
	g_object_unref(service);
	g_object_unref(conn);
	return result;
}

static void ch_paths_free(char **paths)
{
	if (!paths) return;
	for (char **p = paths; *p; ++p) g_free(*p);
	g_free(paths);
}

/* ------------------------------------------------------------- JNI bridge */

/* Returns Object[n], each row an Object[]{ String id, String name,
 * String iconPath, byte[] icon|null }, or NULL on error. */
JNIEXPORT jobjectArray JNICALL Java_android_atl_ATLContentHub_nativeListSources(JNIEnv *env, jclass cls, jstring jtype)
{
	(void)cls;
	if (!jtype) return NULL;
	const char *type = (*env)->GetStringUTFChars(env, jtype, NULL);
	ch_peer *peers = NULL;
	size_t n = 0;
	int rc = ch_list_sources(type, &peers, &n);
	(*env)->ReleaseStringUTFChars(env, jtype, type);
	if (rc != 0) return NULL;

	jclass obj_arr_cls = (*env)->FindClass(env, "[Ljava/lang/Object;");
	jclass obj_cls = (*env)->FindClass(env, "java/lang/Object");
	jobjectArray rows = (*env)->NewObjectArray(env, (jsize)n, obj_arr_cls, NULL);
	for (size_t i = 0; i < n && rows; ++i) {
		jobjectArray row = (*env)->NewObjectArray(env, 4, obj_cls, NULL);
		jstring id = (*env)->NewStringUTF(env, peers[i].id);
		jstring name = (*env)->NewStringUTF(env, peers[i].name);
		jstring icon_path = (*env)->NewStringUTF(env, peers[i].icon_path);
		(*env)->SetObjectArrayElement(env, row, 0, id);
		(*env)->SetObjectArrayElement(env, row, 1, name);
		(*env)->SetObjectArrayElement(env, row, 2, icon_path);
		if (peers[i].icon && peers[i].icon_len > 0) {
			jbyteArray icon = (*env)->NewByteArray(env, (jsize)peers[i].icon_len);
			(*env)->SetByteArrayRegion(env, icon, 0, (jsize)peers[i].icon_len,
			                           (const jbyte *)peers[i].icon);
			(*env)->SetObjectArrayElement(env, row, 3, icon);
			(*env)->DeleteLocalRef(env, icon);
		}
		(*env)->SetObjectArrayElement(env, rows, (jsize)i, row);
		(*env)->DeleteLocalRef(env, id);
		(*env)->DeleteLocalRef(env, name);
		(*env)->DeleteLocalRef(env, icon_path);
		(*env)->DeleteLocalRef(env, row);
	}
	ch_peers_free(peers, n);
	return rows;
}

/* Returns String[] of local paths, or NULL when cancelled/failed. Blocks. */
JNIEXPORT jobjectArray JNICALL Java_android_atl_ATLContentHub_nativeImport(JNIEnv *env, jclass cls,
        jstring jpeer, jstring jtype, jint selection)
{
	(void)cls;
	if (!jpeer || !jtype) return NULL;
	const char *peer = (*env)->GetStringUTFChars(env, jpeer, NULL);
	const char *type = (*env)->GetStringUTFChars(env, jtype, NULL);
	char **paths = ch_import(peer, type, (int)selection);
	(*env)->ReleaseStringUTFChars(env, jpeer, peer);
	(*env)->ReleaseStringUTFChars(env, jtype, type);
	if (!paths) return NULL;

	size_t n = 0;
	while (paths[n]) n++;
	jclass str_cls = (*env)->FindClass(env, "java/lang/String");
	jobjectArray arr = (*env)->NewObjectArray(env, (jsize)n, str_cls, NULL);
	for (size_t i = 0; i < n && arr; ++i) {
		jstring s = (*env)->NewStringUTF(env, paths[i]);
		(*env)->SetObjectArrayElement(env, arr, (jsize)i, s);
		(*env)->DeleteLocalRef(env, s);
	}
	ch_paths_free(paths);
	return arr;
}

JNIEXPORT void JNICALL Java_android_atl_ATLContentHub_nativeImportCancel(JNIEnv *env, jclass cls)
{
	(void)env; (void)cls;
	ch_import_cancel();
}

JNIEXPORT void JNICALL Java_android_atl_ATLContentHub_nativeClearTemp(JNIEnv *env, jclass cls)
{
	(void)env; (void)cls;
	char *tmp = temp_dir();
	remove_recursive(tmp);
	g_free(tmp);
}
