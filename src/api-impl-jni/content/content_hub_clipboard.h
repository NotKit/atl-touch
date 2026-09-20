#pragma once

#include <stdbool.h>

/*
 * Copy and paste through Ubuntu Touch's content-hub, the clipboard every Qt
 * app on the system uses. The Wayland selection only ever reaches other
 * Wayland clients, so without this an app has a clipboard of its own.
 *
 * Optional at runtime: this speaks plain D-Bus, so it builds everywhere, and
 * on a system without content-hub the first call finds nobody on the bus and
 * every later one is a no-op. ATL_CONTENT_HUB_CLIPBOARD=0 turns it off.
 *
 * content-hub only serves the client owning the focused window, named by a Mir
 * persistent surface id. A Wayland client cannot ask for one, so we send an
 * empty id and recent hubs check the calling process instead. Older ones
 * refuse us, and the caller stays on the Wayland selection.
 */

/* Publishes text as the system-wide paste. False if content-hub is not there
 * or would not take it. */
bool atl_content_hub_clipboard_set(const char *text);

/* The newest paste, or NULL if there is nothing to read. Caller g_free()s. */
char *atl_content_hub_clipboard_get(void);
