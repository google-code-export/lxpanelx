/**
 * Copyright (c) 2011 Vadim Ushakov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GTKCOMPAT_H
#define GTKCOMPAT_H

#include <gmodule.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#if !GTK_CHECK_VERSION(2,18,0)

static
void gtk_widget_set_visible(GtkWidget *widget, gboolean visible)
{
    (visible ? gtk_widget_show : gtk_widget_hide)(widget);
}

static
gboolean gtk_widget_get_visible (GtkWidget *widget)
{
    return GTK_WIDGET_VISIBLE(widget) ? TRUE : FALSE;
}

#endif
/*
#if !GTK_CHECK_VERSION(2,20,0)

static
void gtk_widget_set_realized(GtkWidget *widget, gboolean realized)
{
    if (realized) {
       GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    } else {
       GTK_WIDGET_UNSET_FLAGS (widget, GTK_REALIZED);
    }
}

#endif
*/
#if !GTK_CHECK_VERSION(2,24,0)

static
void gdk_pixmap_get_size(GdkPixmap *pixmap, gint *width, gint *height)
{
    gdk_drawable_get_size(pixmap, width, height);
}

#endif


#endif