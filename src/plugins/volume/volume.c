/**
 * Copyright (c) 2006, 2008 LxDE Developers,
 * 	see the file AUTHORS for details.
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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define PLUGIN_PRIV_TYPE volume_t

#include <lxpanelx/panel.h>
#include <lxpanelx/misc.h>
#include <lxpanelx/plugin.h>

#include <lxpanelx/dbg.h>

#include "volume-impl.h"

#include "volume_xpm.h"
#undef const

int mixer_fd;

GtkSpinButton *vol_spin;
static gdouble vol_before_mute;
static gdouble curr_volume;
static GtkWidget *curr_image;
static gboolean skip_botton1_event;

typedef struct {
    GtkWidget *mainw;
    GtkWidget *dlg;
} volume_t;

static void
volume_destructor(Plugin *p)
{
    volume_t *vol = PRIV(p);

    ENTER;
    if (vol->dlg)
        gtk_widget_destroy(vol->dlg);
    gtk_widget_destroy(vol->mainw);
    if (mixer_fd)
        close(mixer_fd);
    g_free(vol);
    RET();
}

static void update_icon (Plugin* p)
{
	volume_t *vol = PRIV(p);
	
	GdkPixbuf *icon;
	GtkWidget *image;
	GtkIconTheme* theme;
	GtkIconInfo* info;
	
	theme = gtk_icon_theme_get_default();

	if (curr_volume <= 0) {
		info = gtk_icon_theme_lookup_icon( theme, "stock_volume-mute", plugin_get_icon_size(p), 0 );
	}
	else if (curr_volume > 0 && curr_volume <= 50) {
		info = gtk_icon_theme_lookup_icon( theme, "stock_volume-min", plugin_get_icon_size(p), 0 );
	}
	else if (curr_volume > 50 && curr_volume <= 75) {
		info = gtk_icon_theme_lookup_icon( theme, "stock_volume-med", plugin_get_icon_size(p), 0 );
	}
	else if (curr_volume > 75) {
		info = gtk_icon_theme_lookup_icon( theme, "stock_volume-max", plugin_get_icon_size(p), 0 );
	}

	if (info ) {
		icon = gdk_pixbuf_new_from_file_at_size(
				gtk_icon_info_get_filename( info ),
				plugin_get_icon_size(p), plugin_get_icon_size(p), NULL );
		gtk_icon_info_free( info );
	}
	else {
			icon = gdk_pixbuf_new_from_xpm_data((const char **) volume_xpm);
	}
	
	 
	
	if (icon) {
		if (curr_image) { 
			gtk_container_remove(GTK_CONTAINER (vol->mainw),curr_image);
			curr_image = NULL;
		}
		image = gtk_image_new_from_pixbuf(icon);
		gtk_container_add (GTK_CONTAINER (vol->mainw), image);
		 
		curr_image = image;
	}
	gtk_widget_show_all(vol->mainw);
	return;
}

static void on_volume_focus (GtkWidget* dlg, GdkEventFocus *event, Plugin* p)
{
	volume_t *vol = PRIV(p);
	
	if (! vol_spin) return;
	GtkAdjustment *vol_adjustment = gtk_spin_button_get_adjustment (vol_spin);
	if (! vol_adjustment) return;
	curr_volume = gtk_adjustment_get_value (vol_adjustment);
	
	update_icon(p);
	
	/* FIXME: use smarter method */
	gtk_widget_destroy( dlg );
	vol->dlg = NULL;
}

static void on_mouse_scroll (GtkWidget* widget, GdkEventScroll* evt, Plugin* p)
{
	volume_t *vol = PRIV(p);

	if ( ! vol->dlg ) {

		vol->dlg = create_volume_window();
		g_signal_connect( vol->mainw, "delete-event",
				G_CALLBACK(on_volume_focus), p );

	}
	else {
		if (! vol_spin) return;
		GtkAdjustment *vol_adjustment = 
			gtk_spin_button_get_adjustment (vol_spin);
		if (! vol_adjustment) return;

		curr_volume = gtk_adjustment_get_value (vol_adjustment);

		if (evt->direction == GDK_SCROLL_UP) {
			curr_volume += 2;
		}
		else /*if (evt->direction == GDK_SCROLL_DOWN)*/ {
			curr_volume -= 2;
		}
		update_icon(p);
		gtk_adjustment_set_value (vol_adjustment, curr_volume);
		gtk_spin_button_set_adjustment(vol_spin, vol_adjustment);
		skip_botton1_event = TRUE;
	}
}

static gboolean on_button_press (GtkWidget* widget, GdkEventButton* evt, Plugin* p)
{
	volume_t *vol = PRIV(p);

	if (plugin_button_press_event(widget, evt, p))
		return TRUE;

	/* for scroll correction */
	if (skip_botton1_event) {
		gtk_widget_destroy(vol->dlg);
		vol->dlg = NULL;
		skip_botton1_event = FALSE;
	}

	switch ( evt->button ) { 
	case 1:	{	/*  Left click */
		if ( ! vol->dlg ) {
			vol->dlg = create_volume_window();

			/* setting background to default */
			gtk_widget_set_style(vol->dlg, panel_get_default_style(plugin_panel(p)));

			g_signal_connect( vol->dlg, "focus-out-event",
					G_CALLBACK(on_volume_focus), p );

			gtk_window_present( GTK_WINDOW(vol->dlg) );
		}
		else {
			/* update icon */
			if (! vol_spin) return TRUE;
			GtkAdjustment *vol_adjustment =
				gtk_spin_button_get_adjustment (vol_spin);
			if (! vol_adjustment) return TRUE;
			curr_volume = gtk_adjustment_get_value (vol_adjustment);
			update_icon(p);

			gtk_widget_destroy(vol->dlg);
			vol->dlg = NULL;
		}
		break;
	}

	case 2:	{	/* middle mouse button */
		if ( ! vol->dlg ) {
			vol->dlg = create_volume_window();
		}

		if (! vol_spin) return TRUE;
		GtkAdjustment *vol_adjustment =
			gtk_spin_button_get_adjustment (vol_spin);
		if (! vol_adjustment) return TRUE;

		curr_volume = gtk_adjustment_get_value (vol_adjustment);

		if (curr_volume > 0) {
			/* turning to mute */
			vol_before_mute = curr_volume;
			curr_volume = 0;
		}
		else {
			curr_volume = vol_before_mute;
		}

		gtk_adjustment_set_value (vol_adjustment, curr_volume);
		gtk_spin_button_set_adjustment(vol_spin, vol_adjustment);

		update_icon(p);

		gtk_widget_destroy( vol->dlg );
		vol->dlg = NULL;
		break;
	}
	default:	/* never here */
		break;
	}
	return FALSE;
}

static int volume_constructor(Plugin *p, char **fp)
{
    volume_t *vol;
//    line s;
//    GdkPixbuf *icon;
//    GtkWidget *image;
//    GtkIconTheme* theme;
//    GtkIconInfo* info;

    vol_before_mute = 1;
    curr_volume = 0;
    curr_image = NULL;
    skip_botton1_event = FALSE;

    ENTER;
    vol = g_new0(volume_t, 1);
    g_return_val_if_fail(vol != NULL, 0);
    plugin_set_priv(p, vol);

    /* check if OSS mixer device could be open */
    mixer_fd = open ("/dev/mixer", O_RDWR, 0);
    if (mixer_fd < 0) {
        RET(0);
    }

    vol->mainw = gtk_event_box_new();

    gtk_widget_add_events( vol->mainw, GDK_BUTTON_PRESS_MASK );
    g_signal_connect( vol->mainw, "button-press-event",
            G_CALLBACK(on_button_press), p );
            
    g_signal_connect( vol->mainw, "scroll-event",
            G_CALLBACK(on_mouse_scroll), p );
    gtk_widget_set_size_request( vol->mainw, plugin_get_icon_size(p), plugin_get_icon_size(p) );

    /* obtain current volume */
    vol->dlg = create_volume_window();
    if (! vol_spin) return 0;
	GtkAdjustment *vol_adjustment =
		gtk_spin_button_get_adjustment (vol_spin);
    if (! vol_adjustment) return 0;
    curr_volume = gtk_adjustment_get_value (vol_adjustment);
    
    update_icon(p);
	gtk_widget_destroy( vol->dlg );
    vol->dlg = NULL;  

    /* FIXME: display current level in tooltip. ex: "Volume Control: 80%"  */
    gtk_widget_set_tooltip_text(vol->mainw, _("Volume control"));

    plugin_set_widget(p, vol->mainw);
    RET(1);
}


PluginClass volume_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "volume",
    name : N_("Volume Control (OSS)"),
    version: "1.0",
    description : "Display and control volume for Open Sound System",

    constructor : volume_constructor,
    destructor  : volume_destructor,
    config : NULL,
    save : NULL
};
