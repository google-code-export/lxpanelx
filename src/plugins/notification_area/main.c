
#if NULL
/* System tray main() */

/*
 * Copyright (C) 2002 Red Hat, Inc.
 * Copyright (C) 2003-2006 Vincent Untz
 * Copyright (C) 2011 Perberos
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>
#include <string.h>

#include <mate-panel-applet.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "na-tray-manager.h"
#include "na-tray.h"
#include "fixedtip.h"

#define NOTIFICATION_AREA_ICON "mate-panel-notification-area"

typedef struct {
	MatePanelApplet* applet;
	NaTray* tray;
} AppletData;

static GtkOrientation get_orientation_from_applet(MatePanelApplet* applet)
{
	GtkOrientation orientation;

	switch (mate_panel_applet_get_orient(applet))
	{
		case MATE_PANEL_APPLET_ORIENT_LEFT:
		case MATE_PANEL_APPLET_ORIENT_RIGHT:
			orientation = GTK_ORIENTATION_VERTICAL;
			break;
		case MATE_PANEL_APPLET_ORIENT_UP:
		case MATE_PANEL_APPLET_ORIENT_DOWN:
		default:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			break;
	}

	return orientation;
}

static void help_cb(GtkAction* action, AppletData* data)
{
	GError* error = NULL;
	char* uri;
	#define NA_HELP_DOC "user-guide"

	uri = g_strdup_printf("ghelp:%s?%s", NA_HELP_DOC, "panels-notification-area");

	gtk_show_uri(gtk_widget_get_screen(GTK_WIDGET(data->applet)), uri, gtk_get_current_event_time(), &error);

	g_free(uri);

	if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
	{
		g_error_free(error);
	}
	else if(error)
	{
		GtkWidget* dialog;
		char* primary;

		primary = g_markup_printf_escaped (_("Could not display help document '%s'"), NA_HELP_DOC);
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", primary);

		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", error->message);

		g_error_free(error);
		g_free(primary);

		g_signal_connect(dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_window_set_icon_name (GTK_WINDOW (dialog), NOTIFICATION_AREA_ICON);
		gtk_window_set_screen (GTK_WINDOW (dialog), gtk_widget_get_screen (GTK_WIDGET (data->applet)));
		/* we have no parent window */
		gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), FALSE);
		gtk_window_set_title (GTK_WINDOW (dialog), _("Error displaying help document"));

		gtk_widget_show (dialog);
	}
}

static void about_cb(GtkAction* action, AppletData* data)
{
	const gchar* authors[] = {
		"Havoc Pennington <hp@redhat.com>",
		"Anders Carlsson <andersca@gnu.org>",
		"Vincent Untz <vuntz@gnome.org>",
		NULL
	};

	const char* documenters[] = {
		"Sun GNOME Documentation Team <gdocteam@sun.com>",
		NULL
	};

	const char copyright[] = \
		"Copyright \xc2\xa9 2002 Red Hat, Inc.\n"
		"Copyright \xc2\xa9 2003-2006 Vincent Untz\n"
		"Copyright \xc2\xa9 2011 Perberos";

	gtk_show_about_dialog(NULL,
		"program-name", _("Notification Area"),
		"authors", authors,
		//"comments", _(comments),
		"copyright", copyright,
		"documenters", documenters,
		"logo-icon-name", NOTIFICATION_AREA_ICON,
		"translator-credits", _("translator-credits"),
		"version", VERSION,
		NULL);
}

static const GtkActionEntry menu_actions [] = {
	{ "SystemTrayHelp", GTK_STOCK_HELP, N_("_Help"),
	  NULL, NULL,
	  G_CALLBACK (help_cb) },
	{ "SystemTrayAbout", GTK_STOCK_ABOUT, N_("_About"),
	  NULL, NULL,
	  G_CALLBACK (about_cb) }
};

static void applet_change_background(MatePanelApplet* applet, MatePanelAppletBackgroundType type, GdkColor* color, GdkPixmap* pixmap, AppletData* data)
{
	na_tray_force_redraw(data->tray);
}


static void applet_change_orientation(MatePanelApplet* applet, MatePanelAppletOrient orient, AppletData* data)
{
	na_tray_set_orientation(data->tray, get_orientation_from_applet(applet));
}

static void applet_destroy(MatePanelApplet* applet, AppletData* data)
{
}

static void free_applet_data(AppletData* data)
{
	g_slice_free(AppletData, data);
}

static void on_applet_realized(GtkWidget* widget, gpointer user_data)
{
	MatePanelApplet* applet;
	AppletData* data;
	NaTray* tray;
	GtkActionGroup* action_group;
	gchar* ui_path;

	applet = MATE_PANEL_APPLET(widget);
	data = g_object_get_data(G_OBJECT(widget), "system-tray-data");

	if (data != NULL)
	{
		return;
	}

	tray = na_tray_new_for_screen(gtk_widget_get_screen(GTK_WIDGET(applet)), get_orientation_from_applet(applet));

	data = g_slice_new(AppletData);
	data->applet = applet;
	data->tray = tray;

	g_object_set_data_full(G_OBJECT(applet), "system-tray-data", data, (GDestroyNotify) free_applet_data);

	g_signal_connect(applet, "change_orient", G_CALLBACK (applet_change_orientation), data);
	g_signal_connect(applet, "change_background", G_CALLBACK (applet_change_background), data);
	g_signal_connect(applet, "destroy", G_CALLBACK (applet_destroy), data);

	gtk_container_add(GTK_CONTAINER (applet), GTK_WIDGET (tray));
	gtk_widget_show(GTK_WIDGET(tray));

	action_group = gtk_action_group_new("ClockApplet Menu Actions");
	gtk_action_group_set_translation_domain(action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions(action_group, menu_actions, G_N_ELEMENTS(menu_actions), data);
	ui_path = g_build_filename(NOTIFICATION_AREA_MENU_UI_DIR, "notification-area-menu.xml", NULL);
	mate_panel_applet_setup_menu_from_file(applet, ui_path, action_group);
	g_free(ui_path);
	g_object_unref(action_group);

}

static inline void force_no_focus_padding(GtkWidget* widget)
{
	static gboolean first_time = TRUE;

	if (first_time)
	{
		gtk_rc_parse_string ("\n"
			"   style \"na-tray-style\"\n"
			"   {\n"
			"      GtkWidget::focus-line-width=0\n"
			"      GtkWidget::focus-padding=0\n"
			"   }\n"
			"\n"
			"    widget \"*.PanelAppletNaTray\" style \"na-tray-style\"\n"
			"\n");

		first_time = FALSE;
	}

	/* El widget antes se llamaba na-tray
	 *
	 * Issue #27
	 */
	gtk_widget_set_name(widget, "PanelAppletNaTray");
}

static gboolean applet_factory(MatePanelApplet* applet, const gchar* iid, gpointer user_data)
{
	AtkObject* atko;

	if (!(strcmp (iid, "NotificationArea") == 0 || strcmp (iid, "SystemTrayApplet") == 0))
	{
		return FALSE;
	}

	/* Defer loading until applet is added to panel so
	 * gtk_widget_get_screen returns correct information */
	g_signal_connect(GTK_WIDGET(applet), "realize", G_CALLBACK(on_applet_realized), NULL);

	atko = gtk_widget_get_accessible (GTK_WIDGET (applet));
	atk_object_set_name (atko, _("Panel Notification Area"));

	mate_panel_applet_set_flags(applet, MATE_PANEL_APPLET_HAS_HANDLE | MATE_PANEL_APPLET_EXPAND_MINOR);

	mate_panel_applet_set_background_widget(applet, GTK_WIDGET(applet));

	force_no_focus_padding(GTK_WIDGET(applet));

	#ifndef NOTIFICATION_AREA_INPROCESS
		gtk_window_set_default_icon_name(NOTIFICATION_AREA_ICON);
	#endif

	gtk_widget_show_all(GTK_WIDGET(applet));
	return TRUE;
}

#ifdef NOTIFICATION_AREA_INPROCESS
	MATE_PANEL_APPLET_IN_PROCESS_FACTORY("NotificationAreaAppletFactory", PANEL_TYPE_APPLET, "NotificationArea", applet_factory, NULL)
#else
	MATE_PANEL_APPLET_OUT_PROCESS_FACTORY("NotificationAreaAppletFactory", PANEL_TYPE_APPLET, "NotificationArea", applet_factory, NULL)
#endif
#endif


#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>

#define PLUGIN_PRIV_TYPE NAPlugin

#include <lxpanelx/gtkcompat.h>
#include <lxpanelx/panel.h>
#include <lxpanelx/misc.h>
#include <lxpanelx/plugin.h>
#include <lxpanelx/Xsupport.h>
//#include "bg.h"

#include "na-tray-manager.h"
#include "na-tray.h"
#include "fixedtip.h"

/* Private context for system tray plugin. */
typedef struct {
    Plugin * plugin;
    GtkWidget * widget;
    NaTray * tray;
} NAPlugin;


static void on_container_realized(GtkWidget* widget, NAPlugin * na)
{
    if (na->tray)
        return;

    na->tray = na_tray_new_for_screen(gtk_widget_get_screen(GTK_WIDGET(na->widget)), plugin_get_orientation(na->plugin));
    na_tray_set_icon_size(na->tray, plugin_get_icon_size(na->plugin));

    gtk_container_add(GTK_CONTAINER(na->widget), GTK_WIDGET(na->tray));
    gtk_widget_show(GTK_WIDGET(na->tray));
}

static int na_constructor(Plugin * p, char ** fp)
{
    NAPlugin * na = g_new0(NAPlugin, 1);
    plugin_set_priv(p, na);
    na->plugin = p;

    na->widget = gtk_event_box_new();
    plugin_set_widget(p, na->widget);
    GTK_WIDGET_SET_FLAGS(na->widget, GTK_NO_WINDOW);
    gtk_widget_set_name(na->widget, "notification_area");
    gtk_container_set_border_width(GTK_CONTAINER(na->widget), 1);

    g_signal_connect(GTK_WIDGET(na->widget), "realize", G_CALLBACK(on_container_realized), na);

    return 1;
}

/* Plugin destructor. */
static void na_destructor(Plugin * p)
{
    NAPlugin * na = PRIV(p);

    g_free(na);
}

static void na_panel_configuration_changed(Plugin * p)
{
    NAPlugin * na = PRIV(p);
    if (na->tray)
    {
        na_tray_set_orientation(na->tray, plugin_get_orientation(na->plugin));
        na_tray_set_icon_size(na->tray, plugin_get_icon_size(na->plugin));
    }
}

/* Plugin descriptor. */
PluginClass notification_area_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "notification_area",
    name : N_("Notification Area"),
    version: "1.0",
    description : N_("Notification Area"),

    not_unloadable: TRUE,

    /* Set a flag to identify the system tray.  It is special in that only one per system can exist. */
    one_per_system : TRUE,

    constructor : na_constructor,
    destructor  : na_destructor,
/*    config : na_configure,
    save : na_save_configuration,*/
    panel_configuration_changed : na_panel_configuration_changed
};
