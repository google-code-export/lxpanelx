/**
 * Copyright (c) 2008 LxDE Developers, see the file AUTHORS for details.
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
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <alsa/asoundlib.h>
#include <poll.h>

#define PLUGIN_PRIV_TYPE VolumeALSAPlugin

#include <lxpanelx/defaultapplications.h>
#include <lxpanelx/panel.h>
#include <lxpanelx/gtkcompat.h>
#include <lxpanelx/misc.h>
#include <lxpanelx/paths.h>
#include <lxpanelx/plugin.h>
#include <lxpanelx/dbg.h>

typedef struct {

    /* Graphics. */
    Plugin * plugin;				/* Back pointer to plugin */
    GtkWidget * tray_icon;			/* Displayed image */
    GtkWidget * popup_window;			/* Top level window for popup */
    GtkWidget * volume_scale;			/* Scale for volume */
    GtkWidget * mute_check;			/* Checkbox for mute state */
    gboolean show_popup;			/* Toggle to show and hide the popup on left click */
    guint volume_scale_handler;			/* Handler for vscale widget */
    guint mute_check_handler;			/* Handler for mute_check widget */

    /* ALSA interface. */
    snd_mixer_t * mixer;			/* The mixer */
    snd_mixer_selem_id_t * sid;			/* The element ID */
    snd_mixer_elem_t * master_element;		/* The Master element */
    guint mixer_evt_idle;			/* Timer to handle restarting poll */

    /* Settings. */
    gchar * volume_control_command;
} VolumeALSAPlugin;

static gboolean asound_find_element(VolumeALSAPlugin * vol, const char * ename);
static gboolean asound_reset_mixer_evt_idle(VolumeALSAPlugin * vol);
static gboolean asound_mixer_event(GIOChannel * channel, GIOCondition cond, gpointer vol_gpointer);
static gboolean asound_initialize(VolumeALSAPlugin * vol);
static gboolean asound_has_mute(VolumeALSAPlugin * vol);
static gboolean asound_is_muted(VolumeALSAPlugin * vol);
static int asound_get_volume(VolumeALSAPlugin * vol);
static void asound_set_volume(VolumeALSAPlugin * vol, int volume);
static void volumealsa_update_display(VolumeALSAPlugin * vol);
static gboolean volumealsa_button_press_event(GtkWidget * widget, GdkEventButton * event, VolumeALSAPlugin * vol);
static gboolean volumealsa_popup_focus_out(GtkWidget * widget, GdkEvent * event, VolumeALSAPlugin * vol);
static void volumealsa_popup_map(GtkWidget * widget, VolumeALSAPlugin * vol);
static void volumealsa_popup_scale_changed(GtkRange * range, VolumeALSAPlugin * vol);
static void volumealsa_popup_scale_scrolled(GtkScale * scale, GdkEventScroll * evt, VolumeALSAPlugin * vol);
static void volumealsa_popup_mute_toggled(GtkWidget * widget, VolumeALSAPlugin * vol);
static void volumealsa_build_popup_window(Plugin * p);
static int volumealsa_constructor(Plugin * p, char ** fp);
static void volumealsa_destructor(Plugin * p);
static void volumealsa_panel_configuration_changed(Plugin * p);

/*** ALSA ***/

static gboolean asound_find_element(VolumeALSAPlugin * vol, const char * ename)
{
    for (
      vol->master_element = snd_mixer_first_elem(vol->mixer);
      vol->master_element != NULL;
      vol->master_element = snd_mixer_elem_next(vol->master_element))
    {
        snd_mixer_selem_get_id(vol->master_element, vol->sid);
        if ((snd_mixer_selem_is_active(vol->master_element))
        && (strcmp(ename, snd_mixer_selem_id_get_name(vol->sid)) == 0))
            return TRUE;
    }
    return FALSE;
}

/* NOTE by PCMan:
 * This is magic! Since ALSA uses its own machanism to handle this part.
 * After polling of mixer fds, it requires that we should call
 * snd_mixer_handle_events to clear all pending mixer events.
 * However, when using the glib IO channels approach, we don't have
 * poll() and snd_mixer_poll_descriptors_revents(). Due to the design of
 * glib, on_mixer_event() will be called for every fd whose status was
 * changed. So, after each poll(), it's called for several times,
 * not just once. Therefore, we cannot call snd_mixer_handle_events()
 * directly in the event handler. Otherwise, it will get called for
 * several times, which might clear unprocessed pending events in the queue.
 * So, here we call it once in the event callback for the first fd.
 * Then, we don't call it for the following fds. After all fds with changed
 * status are handled, we remove this restriction in an idle handler.
 * The next time the event callback is involked for the first fs, we can
 * call snd_mixer_handle_events() again. Racing shouldn't happen here
 * because the idle handler has the same priority as the io channel callback.
 * So, io callbacks for future pending events should be in the next gmain
 * iteration, and won't be affected.
 */

static gboolean asound_reset_mixer_evt_idle(VolumeALSAPlugin * vol)
{
    vol->mixer_evt_idle = 0;
    return FALSE;
}

/* Handler for I/O event on ALSA channel. */
static gboolean asound_mixer_event(GIOChannel * channel, GIOCondition cond, gpointer vol_gpointer)
{
    VolumeALSAPlugin * vol = (VolumeALSAPlugin *) vol_gpointer;

    if (vol->mixer_evt_idle == 0)
    {
        vol->mixer_evt_idle = g_idle_add_full(G_PRIORITY_DEFAULT, (GSourceFunc) asound_reset_mixer_evt_idle, vol, NULL);
        snd_mixer_handle_events(vol->mixer);
    }

    if (cond & G_IO_IN)
    {
        /* the status of mixer is changed. update of display is needed. */
        volumealsa_update_display(vol);
    }

    if (cond & G_IO_HUP)
    {
        /* This means there're some problems with alsa. */
        return FALSE;
    }

    return TRUE;
}

/* Initialize the ALSA interface. */
static gboolean asound_initialize(VolumeALSAPlugin * vol)
{
    /* Access the "default" device. */
    snd_mixer_selem_id_alloca(&vol->sid);
    snd_mixer_open(&vol->mixer, 0);
    snd_mixer_attach(vol->mixer, "default");
    snd_mixer_selem_register(vol->mixer, NULL, NULL);
    snd_mixer_load(vol->mixer);

    /* Find Master element, or Front element, or PCM element, or LineOut element.
     * If one of these succeeds, master_element is valid. */
    if ( ! asound_find_element(vol, "Master"))
        if ( ! asound_find_element(vol, "Front"))
            if ( ! asound_find_element(vol, "PCM"))
            	if ( ! asound_find_element(vol, "LineOut"))
                    return FALSE;

    /* Set the playback volume range as we wish it. */
    snd_mixer_selem_set_playback_volume_range(vol->master_element, 0, 100);

    /* Listen to events from ALSA. */
    int n_fds = snd_mixer_poll_descriptors_count(vol->mixer);
    struct pollfd * fds = g_new0(struct pollfd, n_fds);

    snd_mixer_poll_descriptors(vol->mixer, fds, n_fds);
    int i;
    for (i = 0; i < n_fds; ++i)
    {
        GIOChannel* channel = g_io_channel_unix_new(fds[i].fd);
        g_io_add_watch(channel, G_IO_IN | G_IO_HUP, asound_mixer_event, vol);
        g_io_channel_unref(channel);
    }
    g_free(fds);
    return TRUE;
}

/* Get the presence of the mute control from the sound system. */
static gboolean asound_has_mute(VolumeALSAPlugin * vol)
{
    return ((vol->master_element != NULL) ? snd_mixer_selem_has_playback_switch(vol->master_element) : FALSE);
}

/* Get the condition of the mute control from the sound system. */
static gboolean asound_is_muted(VolumeALSAPlugin * vol)
{
    /* The switch is on if sound is not muted, and off if the sound is muted.
     * Initialize so that the sound appears unmuted if the control does not exist. */
    int value = 1;
    if (vol->master_element != NULL)
        snd_mixer_selem_get_playback_switch(vol->master_element, 0, &value);
    return (value == 0);
}

/* Get the volume from the sound system.
 * This implementation returns the average of the Front Left and Front Right channels. */
static int asound_get_volume(VolumeALSAPlugin * vol)
{
    long aleft = 0;
    long aright = 0;
    if (vol->master_element != NULL)
    {
        snd_mixer_selem_get_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_LEFT, &aleft);
        snd_mixer_selem_get_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_RIGHT, &aright);
    }
    return (aleft + aright) >> 1;
}

/* Set the volume to the sound system.
 * This implementation sets the Front Left and Front Right channels to the specified value. */
static void asound_set_volume(VolumeALSAPlugin * vol, int volume)
{
    if (vol->master_element != NULL)
    {
//        snd_mixer_selem_set_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_LEFT, volume);
//        snd_mixer_selem_set_playback_volume(vol->master_element, SND_MIXER_SCHN_FRONT_RIGHT, volume);
          snd_mixer_selem_set_playback_volume_all(vol->master_element, volume);
    }
}

/*** Graphics ***/

/* Handler for "button-press-event" signal on main widget. */
static gchar * volumealsa_get_volume_control_command(VolumeALSAPlugin * vol)
{
    if (strempty(vol->volume_control_command))
        return get_default_application("volume-control");
    else
        return vol->volume_control_command;
}

/* Do a full redraw of the display. */
static void volumealsa_update_display(VolumeALSAPlugin * vol)
{
    /* Mute status. */
    gboolean mute = asound_is_muted(vol);
    int level = asound_get_volume(vol);

    gboolean icon_updated = FALSE;

    if (mute)
    {
         icon_updated = panel_image_set_icon_theme(plugin_panel(vol->plugin), vol->tray_icon, "audio-volume-muted");
         if (!icon_updated)
         {
             gchar * mute_icon_path = get_private_resource_path(RESOURCE_DATA, "images", "mute.png", 0);
             panel_image_set_from_file(plugin_panel(vol->plugin), vol->tray_icon, mute_icon_path);
             g_free(mute_icon_path);
         }
         icon_updated = TRUE;
    }
    
    if (!icon_updated && level < 30)
    {
        icon_updated = panel_image_set_icon_theme(plugin_panel(vol->plugin), vol->tray_icon, "audio-volume-low");
    }

    if (!icon_updated && level < 70)
    {
        icon_updated = panel_image_set_icon_theme(plugin_panel(vol->plugin), vol->tray_icon, "audio-volume-medium");
    }

    if (!icon_updated)
    {
        icon_updated = panel_image_set_icon_theme(plugin_panel(vol->plugin), vol->tray_icon, "audio-volume-high");
    }

    if (!icon_updated)
    {
        gchar * volume_icon_path = get_private_resource_path(RESOURCE_DATA, "images", "volume.png", 0);
        panel_image_set_from_file(plugin_panel(vol->plugin), vol->tray_icon, volume_icon_path);
        g_free(volume_icon_path);
    }

    g_signal_handler_block(vol->mute_check, vol->mute_check_handler);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vol->mute_check), mute);
    gtk_widget_set_sensitive(vol->mute_check, asound_has_mute(vol));
    g_signal_handler_unblock(vol->mute_check, vol->mute_check_handler);

    /* Volume. */
    if (vol->volume_scale != NULL)
    {
        g_signal_handler_block(vol->volume_scale, vol->volume_scale_handler);
        gtk_range_set_value(GTK_RANGE(vol->volume_scale), asound_get_volume(vol));
        g_signal_handler_unblock(vol->volume_scale, vol->volume_scale_handler);
    }

    /* Display current level in tooltip. */
    char * tooltip = g_strdup_printf("%s %d", _("Volume control"), level);
    gtk_widget_set_tooltip_text(plugin_widget(vol->plugin), tooltip);
    g_free(tooltip);
}

/* Handler for "button-press-event" signal on main widget. */
static gboolean volumealsa_button_press_event(GtkWidget * widget, GdkEventButton * event, VolumeALSAPlugin * vol)
{
    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, event, vol->plugin))
        return TRUE;

    /* Left-click.  Show or hide the popup window. */
    if (event->button == 1)
    {
        if (event->type==GDK_2BUTTON_PRESS)
        {
            if (vol->show_popup)
            {
                gtk_widget_hide(vol->popup_window);
                vol->show_popup = FALSE;
            }
            lxpanel_launch(volumealsa_get_volume_control_command(vol), NULL);
        }
        else
        {
            if (vol->show_popup)
            {
                gtk_widget_hide(vol->popup_window);
                vol->show_popup = FALSE;
            }
            else
            {
                plugin_adjust_popup_position(vol->popup_window, vol->plugin);
                gtk_widget_show_all(vol->popup_window);
                vol->show_popup = TRUE;
            }
        }
    }

    /* Middle-click.  Toggle the mute status. */
    else if (event->button == 2)
    {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(vol->mute_check), ! gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vol->mute_check)));
    }
    return TRUE;
}

/* Handler for "focus-out" signal on popup window. */
static gboolean volumealsa_popup_focus_out(GtkWidget * widget, GdkEvent * event, VolumeALSAPlugin * vol)
{
    /* Hide the widget. */
    gtk_widget_hide(vol->popup_window);
    vol->show_popup = FALSE;
    return FALSE;
}

/* Handler for "map" signal on popup window. */
static void volumealsa_popup_map(GtkWidget * widget, VolumeALSAPlugin * vol)
{
    plugin_adjust_popup_position(widget, vol->plugin);
}

/* Handler for "value_changed" signal on popup window vertical scale. */
static void volumealsa_popup_scale_changed(GtkRange * range, VolumeALSAPlugin * vol)
{
    /* Reflect the value of the control to the sound system. */
    asound_set_volume(vol, gtk_range_get_value(range));

    /* Redraw the controls. */
    volumealsa_update_display(vol);
}

/* Handler for "scroll-event" signal on popup window vertical scale. */
static void volumealsa_popup_scale_scrolled(GtkScale * scale, GdkEventScroll * evt, VolumeALSAPlugin * vol)
{
    /* Get the state of the vertical scale. */
    gdouble val = gtk_range_get_value(GTK_RANGE(vol->volume_scale));

    /* Dispatch on scroll direction to update the value. */
    if ((evt->direction == GDK_SCROLL_UP) || (evt->direction == GDK_SCROLL_LEFT))
        val += 2;
    else
        val -= 2;

    /* Reset the state of the vertical scale.  This provokes a "value_changed" event. */
    gtk_range_set_value(GTK_RANGE(vol->volume_scale), CLAMP((int)val, 0, 100));
}

/* Handler for "toggled" signal on popup window mute checkbox. */
static void volumealsa_popup_mute_toggled(GtkWidget * widget, VolumeALSAPlugin * vol)
{
    /* Get the state of the mute toggle. */
    gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

    /* Reflect the mute toggle to the sound system. */
    if (vol->master_element != NULL)
    {
        int chn;
        for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++)
            snd_mixer_selem_set_playback_switch(vol->master_element, chn, ((active) ? 0 : 1));
    }

    /* Redraw the controls. */
    volumealsa_update_display(vol);
}

/* Build the window that appears when the top level widget is clicked. */
static void volumealsa_build_popup_window(Plugin * p)
{
    VolumeALSAPlugin * vol = PRIV(p);

    /* Create a new window. */
    vol->popup_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(vol->popup_window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(vol->popup_window), 5);
    gtk_window_set_default_size(GTK_WINDOW(vol->popup_window), 80, 140);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(vol->popup_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(vol->popup_window), TRUE);
    gtk_window_set_type_hint(GTK_WINDOW(vol->popup_window), GDK_WINDOW_TYPE_HINT_DIALOG);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(vol->popup_window), "focus_out_event", G_CALLBACK(volumealsa_popup_focus_out), vol);
    g_signal_connect(G_OBJECT(vol->popup_window), "map", G_CALLBACK(volumealsa_popup_map), vol);

    /* Create a scrolled window as the child of the top level window. */
    GtkWidget * scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width (GTK_CONTAINER(scrolledwindow), 0);
    gtk_widget_show(scrolledwindow);
    gtk_container_add(GTK_CONTAINER(vol->popup_window), scrolledwindow);
    gtk_widget_set_can_focus(scrolledwindow, FALSE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_SHADOW_NONE);

    /* Create a viewport as the child of the scrolled window. */
    GtkWidget * viewport = gtk_viewport_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolledwindow), viewport);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
    gtk_widget_show(viewport);

    /* Create a frame as the child of the viewport. */
    GtkWidget * frame = gtk_frame_new(_("Volume"));
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(viewport), frame);

    /* Create a vertical box as the child of the frame. */
    GtkWidget * box = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), box);

    /* Create a vertical scale as the child of the vertical box. */
    vol->volume_scale = gtk_vscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(100, 0, 100, 0, 0, 0)));
    gtk_scale_set_draw_value(GTK_SCALE(vol->volume_scale), FALSE);
    gtk_range_set_inverted(GTK_RANGE(vol->volume_scale), TRUE);
    gtk_box_pack_start(GTK_BOX(box), vol->volume_scale, TRUE, TRUE, 0);

    /* Value-changed and scroll-event signals. */
    vol->volume_scale_handler = g_signal_connect(vol->volume_scale, "value_changed", G_CALLBACK(volumealsa_popup_scale_changed), vol);
    g_signal_connect(vol->volume_scale, "scroll-event", G_CALLBACK(volumealsa_popup_scale_scrolled), vol);

    /* Create a check button as the child of the vertical box. */
    vol->mute_check = gtk_check_button_new_with_label(_("Mute"));
    gtk_box_pack_end(GTK_BOX(box), vol->mute_check, FALSE, FALSE, 0);
    vol->mute_check_handler = g_signal_connect(vol->mute_check, "toggled", G_CALLBACK(volumealsa_popup_mute_toggled), vol);

    /* Set background to default. */
    gtk_widget_set_style(viewport, panel_get_default_style(plugin_panel(p)));
}

/* Plugin constructor. */
static int volumealsa_constructor(Plugin * p, char ** fp)
{
    /* Allocate and initialize plugin context and set into Plugin private data pointer. */
    VolumeALSAPlugin * vol = g_new0(VolumeALSAPlugin, 1);
    vol->plugin = p;
    plugin_set_priv(p, vol);

    /* Initialize ALSA.  If that fails, present nothing. */
    if ( ! asound_initialize(vol))
        return 1;

    vol->volume_control_command = NULL;

    /* Load parameters from the configuration file. */
    line s;
    if (fp)
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END)
        {
            if (s.type == LINE_NONE)
            {
                ERR( "volumealsa: illegal token %s\n", s.str);
                return 0;
            }
            if (s.type == LINE_VAR)
            {
                if (g_ascii_strcasecmp(s.t[0], "VolumeControlCommand") == 0)
                    vol->volume_control_command = g_strdup(s.t[1]);
                else if (g_ascii_strcasecmp(s.t[0], "DoubleClickAction") == 0)
                    vol->volume_control_command = g_strdup(s.t[1]);
                else
                    ERR( "volumealsa: unknown var %s\n", s.t[0]);
            }
            else
            {
                ERR( "volumealsa: illegal in this context %s\n", s.str);
                return 0;
            }
        }

    }
/*
    #define DEFAULT_STRING(f, v) \
      if (vol->f == NULL) \
          vol->f = g_strdup(v);

    DEFAULT_STRING(volume_control_command, "pavucontrol");

    #undef DEFAULT_STRING
*/


    /* Allocate top level widget and set into Plugin widget pointer. */
    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(p, pwid);
    gtk_widget_set_has_window(pwid, FALSE);
    gtk_widget_add_events(pwid, GDK_BUTTON_PRESS_MASK);
    gtk_widget_set_tooltip_text(pwid, _("Volume control"));

    /* Allocate icon as a child of top level. */
    vol->tray_icon = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(pwid), vol->tray_icon);

    /* Initialize window to appear when icon clicked. */
    volumealsa_build_popup_window(p);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(pwid), "button-press-event", G_CALLBACK(volumealsa_button_press_event), vol);
    g_signal_connect(G_OBJECT(pwid), "scroll-event", G_CALLBACK(volumealsa_popup_scale_scrolled), vol );

    /* Update the display, show the widget, and return. */
    volumealsa_update_display(vol);
    gtk_widget_show_all(pwid);
    return 1;
}

/* Plugin destructor. */
static void volumealsa_destructor(Plugin * p)
{
    VolumeALSAPlugin * vol = PRIV(p);

    /* Remove the periodic timer. */
    if (vol->mixer_evt_idle != 0)
        g_source_remove(vol->mixer_evt_idle);

    /* If the dialog box is open, dismiss it. */
    if (vol->popup_window != NULL)
        gtk_widget_destroy(vol->popup_window);

    g_free(vol->volume_control_command);

    /* Deallocate all memory. */
    g_free(vol);
}

/* Callback when the configuration dialog has recorded a configuration change. */
static void volumealsa_apply_configuration(Plugin * p)
{
    volumealsa_panel_configuration_changed(p);
}

/* Callback when the configuration dialog is to be shown. */
static void volumealsa_configure(Plugin * p, GtkWindow * parent)
{
    VolumeALSAPlugin * vol = PRIV(p);

    gchar * volume_control_application = get_default_application("volume-control");
    gchar * tooltip = NULL;
    if (volume_control_application)
        tooltip = g_strdup_printf(_("Application to run by double click. \"%s\" by default."), volume_control_application);
    else
        tooltip = g_strdup(_("Application to run by double click"));

    GtkWidget * dlg = create_generic_config_dlg(
        _(plugin_class(p)->name),
        GTK_WIDGET(parent),
        (GSourceFunc) volumealsa_apply_configuration, (gpointer) p,
        _("Volume Control application"), &vol->volume_control_command, (GType)CONF_TYPE_STR,
        "tooltip-text", tooltip, (GType)CONF_TYPE_SET_PROPERTY,
        NULL);

    g_free(tooltip);

    if (dlg)
        gtk_window_present(GTK_WINDOW(dlg));
}

/* Save the configuration to the configuration file. */
static void volumealsa_save_configuration(Plugin * p, FILE * fp)
{
    VolumeALSAPlugin * vol = PRIV(p);
    lxpanel_put_str(fp, "VolumeControlCommand", vol->volume_control_command);
}


/* Callback when panel configuration changes. */
static void volumealsa_panel_configuration_changed(Plugin * p)
{
    /* Do a full redraw. */
    volumealsa_update_display(PRIV(p));
}

static void volumealsa_on_volume_control_activate(GtkMenuItem * item, VolumeALSAPlugin * vol)
{
    lxpanel_launch(volumealsa_get_volume_control_command(vol), NULL);
}

static void volumealsa_popup_menu_hook(struct _Plugin * plugin, GtkMenu * menu)
{
    VolumeALSAPlugin * vol = PRIV(plugin);
    gchar * command = volumealsa_get_volume_control_command(vol);
    if (command)
    {
        GtkWidget * mi = gtk_menu_item_new_with_label(_("Volume Control..."));
        gchar * tooltip = g_strdup_printf(_("Run %s"), command);
        gtk_widget_set_tooltip_text(mi, tooltip);
        g_free(tooltip);
        g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(volumealsa_on_volume_control_activate), vol);
        gtk_widget_show(mi);
        gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), mi);
    }
}

/* Plugin descriptor. */
PluginClass volumealsa_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "volumealsa",
    name : N_("Volume Control (ALSA)"),
    version: "1.0",
    description : "Display and control volume for ALSA",

    constructor : volumealsa_constructor,
    destructor  : volumealsa_destructor,
    config : volumealsa_configure,
    save : volumealsa_save_configuration,
    panel_configuration_changed : volumealsa_panel_configuration_changed,
    popup_menu_hook : volumealsa_popup_menu_hook
};
