#ifndef _UTILS_GTK_H_
#define _UTILS_GTK_H_

#include <gtk/gtk.h>

/* GTK-specific helpers, split out of util.h so that the main executable
 * (and eventually all non-GTK backends) can use util.h without pulling
 * in a GTK dependency. */

void atl_ensure_widget_snapshotability(GtkWidget *widget);
void atl_safe_gtk_label_set_text(GtkLabel *label, const char *str);
void atl_safe_gtk_widget_set_visible(GtkWidget *widget, gboolean visible);
void atl_safe_gtk_widget_queue_allocate(GtkWidget *widget);
void atl_safe_gtk_widget_queue_resize(GtkWidget *widget);

#endif
