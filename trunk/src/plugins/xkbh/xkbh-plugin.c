/**
 * Copyright (c) 2010 LxDE Developers, see the file AUTHORS for details.
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

/* Originally derived from xfce4-xkb-plugin, Copyright 2004 Alexander Iliev,
 * which credits Michael Glickman. */

/* Modified by Giuseppe Penone <giuspen@gmail.com> starting from 2012-07 and lxpanel 0.5.10 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <lxpanelx/gtkcompat.h>
#include "xkbh.h"

enum
{
    COLUMN_ICON,
    COLUMN_LAYOUT,
    COLUMN_VARIANT,
    NUM_COLUMNS
};

enum
{
    COLUMN_ADD_ICON,
    COLUMN_ADD_LAYOUT,
    COLUMN_ADD_DESC,
    NUM_ADD_COLUMNS
};

enum
{
    COLUMN_MODEL_NAME,
    COLUMN_MODEL_DESC,
    NUM_MODEL_COLUMNS
};

enum
{
    COLUMN_CHANGE_NAME,
    COLUMN_CHANGE_DESC,
    NUM_CHANGE_COLUMNS
};

void panel_config_save(Panel * panel);   /* defined in configurator.c */

static int   xkb_constructor(Plugin * plugin, char ** fp);
static void  xkb_destructor(Plugin * plugin);
static void  xkb_configure(Plugin * p, GtkWindow * parent);
static void  xkb_save_configuration(Plugin * p, FILE * fp);
static void  xkb_panel_configuration_changed(Plugin * p);
static void  xkb_settings_fill_layout_tree_model_with_config(XkbPlugin *p_xkb);
static void  xkb_update_layouts_n_variants(XkbPlugin *p_xkb);
static void  xkb_add_layout(XkbPlugin *p_xkb, gchar *layout, gchar*variant);

static gboolean  on_xkb_button_scroll_event(GtkWidget * widget, GdkEventScroll * event, gpointer p_data);
static gboolean  on_xkb_button_press_event(GtkWidget * widget,  GdkEventButton * event, gpointer p_data);
static void      on_xkb_radiobutton_disp_type_image_clicked(GtkButton *p_radiobutton, gpointer p_data);
static void      on_xkb_dialog_config_response(GtkDialog *p_dialog, gint response, gpointer p_data);

static void xkb_read_xkb_configuration(XkbPlugin *p_xkb);
static void xkb_setxkbmap(XkbPlugin *p_xkb);

static unsigned char  user_active = FALSE;

static gchar * get_flag_path_for_layout(const gchar * layout)
{
    gchar * filename = g_strdup_printf("%s.svg", layout);
    gchar * path = get_private_resource_path(RESOURCE_DATA, "images", "xkbh-flags", filename, NULL);
    g_free(filename);
    return path;
}


/* Redraw the graphics. */
void xkb_redraw(XkbPlugin * xkb) 
{
    /* Set the image. */
    gboolean valid_image = FALSE;
    if (xkb->display_type == DISP_TYPE_IMAGE)
    {
        int size = plugin_get_icon_size(xkb->plugin);
        char * group_name = (char *) xkb_get_current_symbol_name_lowercase(xkb);
        if (group_name != NULL)
        {
            gchar * filename = get_flag_path_for_layout(group_name);
            GdkPixbuf * unscaled_pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
            g_free(filename);
            g_free(group_name);


            if (unscaled_pixbuf != NULL)
            {
                /* Loaded successfully. */
                int width = gdk_pixbuf_get_width(unscaled_pixbuf);
                int height = gdk_pixbuf_get_height(unscaled_pixbuf);
                GdkPixbuf * pixbuf = gdk_pixbuf_scale_simple(unscaled_pixbuf, size * width / height, size, GDK_INTERP_BILINEAR);
                if (pixbuf != NULL)
                {
                    gtk_image_set_from_pixbuf(GTK_IMAGE(xkb->image), pixbuf);
                    g_object_unref(G_OBJECT(pixbuf));
                    gtk_widget_hide(xkb->label);
                    gtk_widget_show(xkb->image);
                    gtk_widget_set_tooltip_text(xkb->btn, xkb_get_current_group_name(xkb));
                    valid_image = TRUE;
                }
                g_object_unref(unscaled_pixbuf);
            }
        }
    }

    /* Set the label. */
    if ((xkb->display_type == DISP_TYPE_TEXT) || ( ! valid_image))
    {
        char * group_name = (char *) xkb_get_current_symbol_name(xkb);
        if (group_name != NULL)
        {
            panel_draw_label_text(plugin_panel(xkb->plugin), xkb->label, (char *) group_name, TRUE, FALSE, FALSE, TRUE);
            gtk_widget_hide(xkb->image);
            gtk_widget_show(xkb->label);
            gtk_widget_set_tooltip_text(xkb->btn, xkb_get_current_group_name(xkb));
        }
    }
}

/* Handler for "scroll-event" on drawing area. */
static gboolean on_xkb_button_scroll_event(GtkWidget * widget, GdkEventScroll * event, gpointer data)
{
    XkbPlugin * xkb = (XkbPlugin *)data;

    /* Change to next or previous group. */
    xkb_change_group(xkb,
        (((event->direction == GDK_SCROLL_UP) || (event->direction == GDK_SCROLL_RIGHT)) ? 1 : -1));
    return TRUE;
}

/* Handler for button-press-event on top level widget. */
static gboolean on_xkb_button_press_event(GtkWidget * widget,  GdkEventButton * event, gpointer data) 
{
    XkbPlugin * xkb = (XkbPlugin *)data;

    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, event, xkb->plugin))
        return TRUE;

    /* Change to next group. */
    xkb_change_group(xkb, 1);
    return TRUE;
}

/* Plugin constructor. */
static int xkb_constructor(Plugin * plugin, char ** fp)
{
    /* Allocate plugin context and set into Plugin private data pointer. */
    XkbPlugin * p_xkb = g_new0(XkbPlugin, 1);
    p_xkb->plugin = plugin;
    plugin_set_priv(plugin, p_xkb);

    /* Initialize to defaults. */
    p_xkb->display_type = DISP_TYPE_IMAGE;
    p_xkb->kbd_model = NULL;
    p_xkb->kbd_layouts = NULL;
    p_xkb->kbd_variants = NULL;
    p_xkb->kbd_change_option = NULL;

    /* Load parameters from the configuration file. */
    line s;
    if (fp != NULL)
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END)
        {
            if (s.type == LINE_NONE)
            {
                ERR( "xkb: illegal token %s\n", s.str);
                return 0;
            }
            if (s.type == LINE_VAR)
            {
                if(g_ascii_strcasecmp(s.t[0], "DisplayType") == 0)
                {
                    p_xkb->display_type = atoi(s.t[1]);
                }
                else if(g_ascii_strcasecmp(s.t[0], "Model") == 0)
                {
                    p_xkb->kbd_model = g_strdup(s.t[1]);
                }
                else if(g_ascii_strcasecmp(s.t[0], "LayoutsList") == 0)
                {
                    p_xkb->kbd_layouts = g_strdup(s.t[1]);
                }
                else if(g_ascii_strcasecmp(s.t[0], "VariantsList") == 0)
                {
                    p_xkb->kbd_variants = g_strdup(s.t[1]);
                }
                else if(g_ascii_strcasecmp(s.t[0], "ToggleOpt") == 0)
                {
                    p_xkb->kbd_change_option = g_strdup(s.t[1]);
                }
                else
                    ERR( "xkb: unknown var %s\n", s.t[0]);
            }
            else
            {
                ERR( "xkb: illegal in this context %s\n", s.str);
                return 0;
            }
        }
    }

    if( (p_xkb->kbd_model == NULL) || (p_xkb->kbd_layouts == NULL) ||
        (p_xkb->kbd_variants == NULL) || (p_xkb->kbd_change_option == NULL) )
    {
        xkb_read_xkb_configuration(p_xkb);
    }
    else
        xkb_setxkbmap(p_xkb);

    /* Allocate top level widget and set into Plugin widget pointer. */
    GtkWidget * pwid = gtk_event_box_new();
    plugin_set_widget(plugin, pwid);
    //gtk_widget_set_has_window(pwid, FALSE);
    gtk_widget_add_events(pwid, GDK_BUTTON_PRESS_MASK);

    /* Create a button as the child of the event box. */
    p_xkb->btn = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(pwid), p_xkb->btn);
    gtk_button_set_relief(GTK_BUTTON(p_xkb->btn), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus(p_xkb->btn, FALSE);
    gtk_widget_set_can_default(p_xkb->btn, FALSE);
    gtk_widget_show(p_xkb->btn);

    /* Create a horizontal box as the child of the button. */
    GtkWidget * hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(p_xkb->btn), hbox);
    gtk_widget_show(hbox);

    /* Create a label and an image as children of the horizontal box.
     * Only one of these is visible at a time, controlled by user preference
     * and the successful loading of the image. */
    p_xkb->label = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(hbox), p_xkb->label);
    p_xkb->image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(hbox), p_xkb->image);

    /* Initialize the XKB interface. */
    xkb_mechanism_constructor(p_xkb);

    /* Connect signals. */
    g_signal_connect(p_xkb->btn, "button-press-event", G_CALLBACK(on_xkb_button_press_event), p_xkb);
    g_signal_connect(p_xkb->btn, "scroll-event", G_CALLBACK(on_xkb_button_scroll_event), p_xkb);

    /* Show the widget and return. */
    xkb_redraw(p_xkb);
    gtk_widget_show(pwid);
    return 1;
}

/* Plugin destructor. */
static void xkb_destructor(Plugin * plugin)
{
    XkbPlugin * p_xkb = PRIV(plugin);

    /* Disconnect from the XKB mechanism. */
    g_source_remove(p_xkb->source_id);
    xkb_mechanism_destructor(p_xkb);

    /* Ensure that the configuration dialog is dismissed. */
    if(p_xkb->p_dialog_config != NULL)
        gtk_widget_destroy(p_xkb->p_dialog_config);

    /* Deallocate all memory. */
    g_free(p_xkb->kbd_model);
    g_free(p_xkb->kbd_layouts);
    g_free(p_xkb->kbd_variants);
    g_free(p_xkb->kbd_change_option);
    g_free(p_xkb);
}

/****************************************************************************/

static void xkb_read_xkb_configuration(XkbPlugin *p_xkb)
{
        g_free(p_xkb->kbd_model);
        g_free(p_xkb->kbd_layouts);
        g_free(p_xkb->kbd_variants);
        g_free(p_xkb->kbd_change_option);

        p_xkb->kbd_model = NULL;
        p_xkb->kbd_layouts = NULL;
        p_xkb->kbd_variants = NULL;
        p_xkb->kbd_change_option = NULL;


        FILE *fp;
        char  buf[MAX_ROW_LEN];
        fp = popen("setxkbmap -query", "r");
        if(fp != NULL)
        {
            GRegex *p_regex_model = g_regex_new("(?<=model:).*", 0, 0, NULL);
            GRegex *p_regex_layouts = g_regex_new("(?<=layout:).*", 0, 0, NULL);
            GRegex *p_regex_variants = g_regex_new("(?<=variant:).*", 0, 0, NULL);
            GRegex *p_regex_grp = g_regex_new("(?<=grp:)[^,]*", 0, 0, NULL);
            GMatchInfo *p_match_info;
            while(fgets(buf, MAX_ROW_LEN, fp) != NULL)
            {
                // model
                g_regex_match(p_regex_model, buf, 0, &p_match_info);
                if(g_match_info_matches(p_match_info))
                {
                    p_xkb->kbd_model = g_strchug(g_match_info_fetch(p_match_info, 0));
                    g_match_info_free(p_match_info);
                    continue;
                }
                g_match_info_free(p_match_info);

                // layouts
                g_regex_match(p_regex_layouts, buf, 0, &p_match_info);
                if(g_match_info_matches(p_match_info))
                {
                    p_xkb->kbd_layouts = g_strchug(g_match_info_fetch(p_match_info, 0));
                    g_match_info_free(p_match_info);
                    continue;
                }
                g_match_info_free(p_match_info);

                // variants
                g_regex_match(p_regex_variants, buf, 0, &p_match_info);
                if(g_match_info_matches(p_match_info))
                {
                    p_xkb->kbd_variants = g_strchug(g_match_info_fetch(p_match_info, 0));
                    g_match_info_free(p_match_info);
                    continue;
                }
                g_match_info_free(p_match_info);

                // grp
                g_regex_match(p_regex_grp, buf, 0, &p_match_info);
                if(g_match_info_matches(p_match_info))
                {
                    p_xkb->kbd_change_option = g_strchug(g_match_info_fetch(p_match_info, 0));
                    g_match_info_free(p_match_info);
                    continue;
                }
                g_match_info_free(p_match_info);
            }

            g_regex_unref(p_regex_model);
            g_regex_unref(p_regex_layouts);
            g_regex_unref(p_regex_variants);

            /* close */
            pclose(fp);
        }
        if(p_xkb->kbd_model == NULL) p_xkb->kbd_model = g_strdup("pc105");
        if(p_xkb->kbd_layouts == NULL) p_xkb->kbd_layouts = g_strdup("us");
        if(p_xkb->kbd_variants == NULL) p_xkb->kbd_variants = g_strdup(",");
        if(p_xkb->kbd_change_option == NULL) p_xkb->kbd_change_option = g_strdup("shift_caps_toggle");

#if 1
        /* extract variants from layouts */

        gchar ** layout_list = g_strsplit_set(p_xkb->kbd_layouts, ",", 0);
        gchar ** variant_list = g_strsplit_set(p_xkb->kbd_variants, ",", 0);
        gchar * new_variants = NULL;

        gchar ** layout;
        gchar ** old_variant = variant_list;
        for (layout = layout_list; *layout; layout++)
        {
            gchar * s = *layout;

            int len = strlen(s);
            if (len < 3)
                continue;

            /* fix layout:n notation */
            if (isdigit(s[len - 1]) && s[len - 2] == ':')
            {
                len -= 2;
                s[len] = 0;
            }

            if (len < 3)
                continue;

            /* search for layout(variant) notation */
            gchar * v = NULL;

            if (s[len - 1] == ')')
            {
                char * i = strrchr(s, '(');
                if (i && i != s && i != s + len - 2)
                {
                    s[len - 1] = 0;
                    *i = 0;
                    v = i + 1;
                }
            }

            /* if layout(variant) notation not found, use valus from variant list */
            if (!v && *old_variant)
                v = *old_variant;
            if (!v)
                v = "";
g_print("v = '%s'\n", v);
            /* concat new_variants */
            if (new_variants)
            {
                v = g_strdup_printf("%s,%s", new_variants, v);
                g_free(new_variants);
                new_variants = v;
            }
            else
            {
                new_variants = g_strdup(v);
            }
g_print("new_variants = '%s'\n", new_variants);

            if (*old_variant)
               old_variant++;
        }

        g_free(p_xkb->kbd_layouts);
        g_free(p_xkb->kbd_variants);

        p_xkb->kbd_layouts = g_strjoinv(",", layout_list);
        p_xkb->kbd_variants = new_variants;

        g_strfreev(layout_list);
        g_strfreev(variant_list);
#endif
}

static void xkb_setxkbmap(XkbPlugin *p_xkb)
{
    GString *p_gstring_syscmd = g_string_new("");
    g_string_printf(p_gstring_syscmd,
                    "setxkbmap -model \"%s\" -layout \"%s\" -variant \"%s\" -option grp:\"%s\"",
                    p_xkb->kbd_model, p_xkb->kbd_layouts, p_xkb->kbd_variants, p_xkb->kbd_change_option);
    if(system(p_gstring_syscmd->str)) {};
    g_printf("\n%s\n", p_gstring_syscmd->str);
    g_string_free(p_gstring_syscmd, TRUE/*free also gstring->str*/);
}

/****************************************************************************/

/* Handler for "clicked" event on display type image radiobutton of configuration dialog. */
static void on_xkb_radiobutton_disp_type_image_clicked(GtkButton *p_button, gpointer p_data)
{
    if(user_active == TRUE)
    {
        /* Fetch the new value and redraw. */
        XkbPlugin * p_xkb = (XkbPlugin *)p_data;
        p_xkb->display_type = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p_button)) ? DISP_TYPE_IMAGE : DISP_TYPE_TEXT;
        xkb_redraw(p_xkb);
    }
}

/* Handler for "response" event on configuration dialog. */
static void on_xkb_dialog_config_response(GtkDialog *p_dialog, gint response, gpointer data)
{
    XkbPlugin * p_xkb = (XkbPlugin *)data;

    /* Save the new configuration and redraw the plugin. */
    panel_config_save(plugin_panel(p_xkb->plugin));
    xkb_redraw(p_xkb);

    /* Destroy the dialog. */
    gtk_widget_destroy(p_xkb->p_dialog_config);
    p_xkb->p_dialog_config = NULL;
}

// callback for double click
static gboolean  on_treeview_kbd_model_button_press_event(GtkWidget *p_widget,
                                                       GdkEventButton *p_event,
                                                       gpointer p_dialog)
{
    if(p_event->button == 1)
    {
        if(p_event->type == GDK_2BUTTON_PRESS)
        {
            GtkButton *p_button_ok = (GtkButton *)gtk_dialog_get_widget_for_response(GTK_DIALOG(p_dialog), GTK_RESPONSE_OK);
            gtk_button_clicked(p_button_ok);
        }
    }
    return FALSE;
}

// callback for double click
static gboolean  on_treeview_kbd_change_button_press_event(GtkWidget *p_widget,
                                                        GdkEventButton *p_event,
                                                        gpointer p_dialog)
{
    if(p_event->button == 1)
    {
        if(p_event->type == GDK_2BUTTON_PRESS)
        {
            GtkButton *p_button_ok = (GtkButton *)gtk_dialog_get_widget_for_response(GTK_DIALOG(p_dialog), GTK_RESPONSE_OK);
            gtk_button_clicked(p_button_ok);
        }
    }
    return FALSE;
}


// callback for double click
static gboolean  on_treeview_add_layout_button_press_event(GtkWidget *p_widget,
                                                        GdkEventButton *p_event,
                                                        gpointer p_dialog)
{
    if(p_event->button == 1)
    {
        if(p_event->type == GDK_2BUTTON_PRESS)
        {
            GtkButton *p_button_ok = (GtkButton *)gtk_dialog_get_widget_for_response(GTK_DIALOG(p_dialog), GTK_RESPONSE_OK);
            gtk_button_clicked(p_button_ok);
        }
    }
    return FALSE;
}


static void on_button_kbd_model_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;

    // dialog
    GtkWidget *p_dialog = gtk_dialog_new_with_buttons(_("Select Keyboard Model"),
                            GTK_WINDOW(p_xkb->p_dialog_config),
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

    // scrolledwindow
    GtkWidget * p_scrolledwindow_kbd_model = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p_scrolledwindow_kbd_model),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(p_dialog)->vbox), p_scrolledwindow_kbd_model, TRUE, TRUE, 2);

    // liststore
    GtkListStore *p_liststore_kbd_model = gtk_list_store_new(NUM_MODEL_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *p_treeview_kbd_model = gtk_tree_view_new_with_model(GTK_TREE_MODEL(p_liststore_kbd_model));
    g_object_unref(G_OBJECT(p_liststore_kbd_model));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(p_treeview_kbd_model), TRUE);
    gtk_container_add(GTK_CONTAINER(p_scrolledwindow_kbd_model), p_treeview_kbd_model);
    GtkCellRenderer *p_renderer;
    GtkTreeViewColumn *p_column;
    // model
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Model"), p_renderer, "text", COLUMN_CHANGE_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_kbd_model), p_column);
    // desc
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Description"), p_renderer, "text", COLUMN_CHANGE_DESC, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_kbd_model), p_column);

    // populate model
    GKeyFile *p_keyfile = g_key_file_new();
    gchar *xkbcfg_filepath = get_private_resource_path(RESOURCE_DATA, "xkeyboardconfig", "models.cfg", NULL);

    if(g_key_file_load_from_file(p_keyfile, xkbcfg_filepath, 0, NULL))
    {
        gchar **keys_models = g_key_file_get_keys(p_keyfile, "MODELS", NULL, NULL);
        guint   model_idx = 0;
        GtkTreeIter  tree_iter;
        gchar *p_model_desc;
        while(keys_models[model_idx] != NULL)
        {
            p_model_desc = g_key_file_get_string(p_keyfile, "MODELS", keys_models[model_idx], NULL);
            gtk_list_store_append(p_liststore_kbd_model, &tree_iter);
            gtk_list_store_set(p_liststore_kbd_model, &tree_iter,
                                COLUMN_MODEL_NAME, keys_models[model_idx],
                                COLUMN_MODEL_DESC, p_model_desc,
                                -1);
            g_free(p_model_desc);
            model_idx++;
        }
        g_strfreev(keys_models);
        g_key_file_free(p_keyfile);
    }
    g_free(xkbcfg_filepath);

    g_signal_connect(p_treeview_kbd_model, "button-press-event",
                     G_CALLBACK(on_treeview_kbd_model_button_press_event), p_dialog);
    gtk_widget_set_size_request(p_dialog, 600, 500);
    gtk_widget_show_all(GTK_WIDGET(p_scrolledwindow_kbd_model));
    gint  response = gtk_dialog_run(GTK_DIALOG(p_dialog));
    if(response == GTK_RESPONSE_OK)
    {
        GtkTreeIter  tree_iter_sel;
        GtkTreeSelection *p_treeselection_kbd_model = gtk_tree_view_get_selection(GTK_TREE_VIEW(p_treeview_kbd_model));
        if(gtk_tree_selection_get_selected(p_treeselection_kbd_model,
                                           (GtkTreeModel **)(&p_liststore_kbd_model),
                                           &tree_iter_sel))
        {
            gchar *kbd_model_new;
            gtk_tree_model_get(GTK_TREE_MODEL(p_liststore_kbd_model),
                               &tree_iter_sel, COLUMN_MODEL_NAME, &kbd_model_new, -1);
            g_free(p_xkb->kbd_model);
            p_xkb->kbd_model = g_strdup(kbd_model_new);
            gtk_button_set_label(GTK_BUTTON(p_xkb->p_button_kbd_model), p_xkb->kbd_model);
            g_free(kbd_model_new);
            xkb_setxkbmap(p_xkb);
            xkb_redraw(p_xkb);
        }
    }
    gtk_widget_destroy(p_dialog);
}

static void on_button_kbd_change_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;

    // dialog
    GtkWidget *p_dialog = gtk_dialog_new_with_buttons(_("Select Layout Change Type"),
                            GTK_WINDOW(p_xkb->p_dialog_config),
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

    // scrolledwindow
    GtkWidget * p_scrolledwindow_kbd_change = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p_scrolledwindow_kbd_change),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(p_dialog)->vbox), p_scrolledwindow_kbd_change, TRUE, TRUE, 2);

    // liststore
    GtkListStore *p_liststore_kbd_change = gtk_list_store_new(NUM_CHANGE_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *p_treeview_kbd_change = gtk_tree_view_new_with_model(GTK_TREE_MODEL(p_liststore_kbd_change));
    g_object_unref(G_OBJECT(p_liststore_kbd_change));
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(p_treeview_kbd_change), TRUE);
    gtk_container_add(GTK_CONTAINER(p_scrolledwindow_kbd_change), p_treeview_kbd_change);
    GtkCellRenderer *p_renderer;
    GtkTreeViewColumn *p_column;
    // model
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Change Type"), p_renderer, "text", COLUMN_CHANGE_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_kbd_change), p_column);
    // desc
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Description"), p_renderer, "text", COLUMN_CHANGE_DESC, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_kbd_change), p_column);

    // populate model
    GKeyFile *p_keyfile = g_key_file_new();
    gchar *xkbcfg_filepath = get_private_resource_path(RESOURCE_DATA, "xkeyboardconfig", "toggle.cfg", NULL);

    if(g_key_file_load_from_file(p_keyfile, xkbcfg_filepath, 0, NULL))
    {
        gchar **keys_changes = g_key_file_get_keys(p_keyfile, "TOGGLE", NULL, NULL);
        guint   change_idx = 0;
        GtkTreeIter  tree_iter;
        gchar *p_change_desc;
        while(keys_changes[change_idx] != NULL)
        {
            p_change_desc = g_key_file_get_string(p_keyfile, "TOGGLE", keys_changes[change_idx], NULL);
            gtk_list_store_append(p_liststore_kbd_change, &tree_iter);
            gtk_list_store_set(p_liststore_kbd_change, &tree_iter,
                                COLUMN_CHANGE_NAME, keys_changes[change_idx],
                                COLUMN_CHANGE_DESC, p_change_desc,
                                -1);
            g_free(p_change_desc);
            change_idx++;
        }
        g_strfreev(keys_changes);
        g_key_file_free(p_keyfile);
    }
    g_free(xkbcfg_filepath);

    g_signal_connect(p_treeview_kbd_change, "button-press-event",
                     G_CALLBACK(on_treeview_kbd_change_button_press_event), p_dialog);
    gtk_widget_set_size_request(p_dialog, 600, 500);
    gtk_widget_show_all(GTK_WIDGET(p_scrolledwindow_kbd_change));
    gint  response = gtk_dialog_run(GTK_DIALOG(p_dialog));
    if(response == GTK_RESPONSE_OK)
    {
        GtkTreeIter  tree_iter_sel;
        GtkTreeSelection *p_treeselection_kbd_change = gtk_tree_view_get_selection(GTK_TREE_VIEW(p_treeview_kbd_change));
        if(gtk_tree_selection_get_selected(p_treeselection_kbd_change,
                                           (GtkTreeModel **)(&p_liststore_kbd_change),
                                           &tree_iter_sel))
        {
            gchar *kbd_change_new;
            gtk_tree_model_get(GTK_TREE_MODEL(p_liststore_kbd_change),
                               &tree_iter_sel, COLUMN_CHANGE_NAME, &kbd_change_new, -1);
            g_free(p_xkb->kbd_change_option);
            p_xkb->kbd_change_option = g_strdup(kbd_change_new);
            gtk_button_set_label(GTK_BUTTON(p_xkb->p_button_change_layout), p_xkb->kbd_change_option);
            g_free(kbd_change_new);
            xkb_setxkbmap(p_xkb);
            xkb_redraw(p_xkb);
        }
    }
    gtk_widget_destroy(p_dialog);
}

static void on_button_up_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;
    GtkTreeIter  tree_iter_sel;
    GtkTreeIter  tree_iter_prev;
    if(gtk_tree_selection_get_selected(p_xkb->p_treeselection_layout,
                                       (GtkTreeModel **)(&p_xkb->p_liststore_layout),
                                       &tree_iter_sel))
    {
        GtkTreePath *p_tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(p_xkb->p_liststore_layout),
                                                           &tree_iter_sel);
        if( (gtk_tree_path_prev(p_tree_path)) &&
            (gtk_tree_model_get_iter(GTK_TREE_MODEL(p_xkb->p_liststore_layout),
                                     &tree_iter_prev,
                                     p_tree_path)))
        {
            gtk_list_store_swap(p_xkb->p_liststore_layout, &tree_iter_sel, &tree_iter_prev);
            xkb_update_layouts_n_variants(p_xkb);
        }
        gtk_tree_path_free(p_tree_path);
    }
}

static void on_button_down_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;
    GtkTreeIter  tree_iter_sel;
    GtkTreeIter  tree_iter_next;
    if(gtk_tree_selection_get_selected(p_xkb->p_treeselection_layout,
                                       (GtkTreeModel **)(&p_xkb->p_liststore_layout),
                                       &tree_iter_sel))
    {
        tree_iter_next = tree_iter_sel;
        if(gtk_tree_model_iter_next(GTK_TREE_MODEL(p_xkb->p_liststore_layout),
                                    &tree_iter_next))
        {
            gtk_list_store_swap(p_xkb->p_liststore_layout, &tree_iter_sel, &tree_iter_next);
            xkb_update_layouts_n_variants(p_xkb);
        }
    }
}

static void on_button_rm_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;
    GtkTreeIter  tree_iter_sel;
    if(gtk_tree_selection_get_selected(p_xkb->p_treeselection_layout,
                                       (GtkTreeModel **)(&p_xkb->p_liststore_layout),
                                       &tree_iter_sel))
    {
        gtk_list_store_remove(p_xkb->p_liststore_layout, &tree_iter_sel);
        xkb_update_layouts_n_variants(p_xkb);
        gtk_widget_set_sensitive(p_xkb->p_button_rm_layout, p_xkb->num_layouts > 1);
    }
}

static void on_button_add_layout_clicked(GtkButton *p_button, gpointer *p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;

    // dialog
    GtkWidget *p_dialog = gtk_dialog_new_with_buttons(_("Add Keyboard Layout"),
                            GTK_WINDOW(p_xkb->p_dialog_config),
                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK, GTK_RESPONSE_OK,
                            NULL);

    // scrolledwindow
    GtkWidget * p_scrolledwindow_add_layout = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p_scrolledwindow_add_layout),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(p_dialog)->vbox), p_scrolledwindow_add_layout, TRUE, TRUE, 2);

    // treestore
    GtkTreeStore *p_treestore_add_layout = gtk_tree_store_new(NUM_ADD_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *p_treeview_add_layout = gtk_tree_view_new_with_model(GTK_TREE_MODEL(p_treestore_add_layout));
    g_object_unref(G_OBJECT(p_treestore_add_layout));
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(p_treeview_add_layout), TRUE);
    gtk_container_add(GTK_CONTAINER(p_scrolledwindow_add_layout), p_treeview_add_layout);
    GtkCellRenderer *p_renderer;
    GtkTreeViewColumn *p_column;
    // icon
    p_renderer = gtk_cell_renderer_pixbuf_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Flag"), p_renderer, "pixbuf", COLUMN_ICON, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_add_layout), p_column);
    // layout
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Layout"), p_renderer, "text", COLUMN_LAYOUT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_add_layout), p_column);
    // desc
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Description"), p_renderer, "text", COLUMN_VARIANT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_treeview_add_layout), p_column);

    // populate model
    GKeyFile *p_keyfile = g_key_file_new();
    gchar *xkbcfg_filepath = get_private_resource_path(RESOURCE_DATA, "xkeyboardconfig", "layouts.cfg", NULL);

    if(g_key_file_load_from_file(p_keyfile, xkbcfg_filepath, 0, NULL))
    {
        gchar **keys_layouts = g_key_file_get_keys(p_keyfile, "LAYOUTS", NULL, NULL);
        guint   layout_idx = 0;
        GtkTreeIter  tree_top, tree_child;
        gchar *p_layout_desc;
        while(keys_layouts[layout_idx] != NULL)
        {
            p_layout_desc = g_key_file_get_string(p_keyfile, "LAYOUTS", keys_layouts[layout_idx], NULL);
            if(strchr(keys_layouts[layout_idx], '(') == NULL)
            {
                gchar *flag_filepath = get_flag_path_for_layout(keys_layouts[layout_idx]);

                GdkPixbuf *p_pixbuf = gdk_pixbuf_new_from_file_at_size(flag_filepath, -1, 22, NULL);
                gtk_tree_store_append(p_treestore_add_layout, &tree_top, NULL);
                if(p_pixbuf != NULL)
                {
                    gtk_tree_store_set(p_treestore_add_layout, &tree_top,
                                        COLUMN_ADD_ICON, p_pixbuf,
                                        COLUMN_ADD_LAYOUT, keys_layouts[layout_idx],
                                        COLUMN_ADD_DESC, p_layout_desc,
                                        -1);
                    g_object_unref(G_OBJECT(p_pixbuf));
                }
                else
                {
                    gtk_tree_store_set(p_treestore_add_layout, &tree_top,
                                        COLUMN_ADD_LAYOUT, keys_layouts[layout_idx],
                                        COLUMN_ADD_DESC, p_layout_desc,
                                        -1);
                }
                g_free(flag_filepath);
            }
            else
            {

                gtk_tree_store_append(p_treestore_add_layout, &tree_child, &tree_top);
                gtk_tree_store_set(p_treestore_add_layout, &tree_child,
                                    COLUMN_ADD_LAYOUT, keys_layouts[layout_idx],
                                    COLUMN_ADD_DESC, p_layout_desc,
                                    -1);
            }
            g_free(p_layout_desc);
            layout_idx++;
        }
        g_strfreev(keys_layouts);
        g_key_file_free(p_keyfile);
    }
    g_free(xkbcfg_filepath);

    g_signal_connect(p_treeview_add_layout, "button-press-event",
                     G_CALLBACK(on_treeview_add_layout_button_press_event), p_dialog);
    gtk_widget_set_size_request(p_dialog, 600, 500);
    gtk_widget_show_all(GTK_WIDGET(p_scrolledwindow_add_layout));
    gint  response = gtk_dialog_run(GTK_DIALOG(p_dialog));
    if(response == GTK_RESPONSE_OK)
    {
        GtkTreeIter  tree_iter_sel;
        GtkTreeSelection *p_treeselection_add_layout = gtk_tree_view_get_selection(GTK_TREE_VIEW(p_treeview_add_layout));
        if(gtk_tree_selection_get_selected(p_treeselection_add_layout,
                                           (GtkTreeModel **)(&p_treestore_add_layout),
                                           &tree_iter_sel))
        {
            gchar *layout_add;
            GString *p_gstring_new_layout = g_string_new("");
            GString *p_gstring_new_variant = g_string_new("");
            gtk_tree_model_get(GTK_TREE_MODEL(p_treestore_add_layout),
                               &tree_iter_sel, COLUMN_ADD_LAYOUT, &layout_add, -1);

            if(strchr(layout_add, '(') == NULL)
            {
                g_string_append(p_gstring_new_layout, layout_add);
            }
            else
            {
                gboolean  parsing_variant = FALSE;
                guchar  i;
                for(i=0; layout_add[i]; i++)
                {
                    if(!parsing_variant)
                    {
                        if(layout_add[i] == '(')
                            parsing_variant = TRUE;
                        else
                            g_string_append_c(p_gstring_new_layout, layout_add[i]);
                    }
                    else
                    {
                        if(layout_add[i] == ')')
                            break;
                        else
                            g_string_append_c(p_gstring_new_variant, layout_add[i]);
                    }
                }
            }
            xkb_add_layout(p_xkb, p_gstring_new_layout->str, p_gstring_new_variant->str);
            xkb_update_layouts_n_variants(p_xkb);

            gtk_widget_set_sensitive(p_xkb->p_button_rm_layout, p_xkb->num_layouts > 1);

            g_free(layout_add);
            g_string_free(p_gstring_new_layout, TRUE/*free also gstring->str*/);
            g_string_free(p_gstring_new_variant, TRUE/*free also gstring->str*/);
        }
    }
    gtk_widget_destroy(p_dialog);
}

static gboolean layouts_tree_model_foreach(GtkTreeModel *p_model,
                                           GtkTreePath *p_path,
                                           GtkTreeIter *p_iter,
                                           gpointer p_data)
{
    XkbPlugin *p_xkb = (XkbPlugin *)p_data;
    gchar *layout_val;
    gchar *variant_val;

    gtk_tree_model_get(p_model, p_iter, COLUMN_LAYOUT, &layout_val,  -1);
    gtk_tree_model_get(p_model, p_iter, COLUMN_VARIANT, &variant_val,  -1);

    if(strlen(p_xkb->p_gstring_layouts_partial->str))
    {
        g_string_append_c(p_xkb->p_gstring_layouts_partial, ',');
        g_string_append_c(p_xkb->p_gstring_variants_partial, ',');
    }
    g_string_append(p_xkb->p_gstring_layouts_partial, layout_val);
    g_string_append(p_xkb->p_gstring_variants_partial, variant_val);

    //printf("\npartial layouts = '%s'\n", p_xkb->p_gstring_layouts_partial->str);
    //printf("partial variants = '%s'\n", p_xkb->p_gstring_variants_partial->str);

    g_free(layout_val);
    g_free(variant_val);
    p_xkb->num_layouts++;
    return FALSE;
}

static void xkb_update_layouts_n_variants(XkbPlugin *p_xkb)
{
    p_xkb->p_gstring_layouts_partial = g_string_new("");
    p_xkb->p_gstring_variants_partial = g_string_new("");
    p_xkb->num_layouts = 0;
    gtk_tree_model_foreach(GTK_TREE_MODEL(p_xkb->p_liststore_layout),
                           layouts_tree_model_foreach,
                           p_xkb);
    if(!strlen(p_xkb->p_gstring_variants_partial->str))
    {
        g_string_append_c(p_xkb->p_gstring_variants_partial, ',');
    }
    g_free(p_xkb->kbd_layouts);
    g_free(p_xkb->kbd_variants);
    p_xkb->kbd_layouts = g_strdup(p_xkb->p_gstring_layouts_partial->str);
    p_xkb->kbd_variants = g_strdup(p_xkb->p_gstring_variants_partial->str);
    g_string_free(p_xkb->p_gstring_layouts_partial, TRUE/*free also gstring->str*/);
    g_string_free(p_xkb->p_gstring_variants_partial, TRUE/*free also gstring->str*/);
    xkb_setxkbmap(p_xkb);
    xkb_mechanism_destructor(p_xkb);
    xkb_mechanism_constructor(p_xkb);
    xkb_redraw(p_xkb);
}

static void xkb_add_layout(XkbPlugin *p_xkb, gchar *layout, gchar*variant)
{
    GtkTreeIter  tree_iter;
    gtk_list_store_append(p_xkb->p_liststore_layout, &tree_iter);

    gchar *flag_filepath = get_flag_path_for_layout(layout);

    GdkPixbuf *p_pixbuf = gdk_pixbuf_new_from_file_at_size(flag_filepath, -1, 22, NULL);
    if(p_pixbuf != NULL)
    {
        gtk_list_store_set(p_xkb->p_liststore_layout, &tree_iter,
                           COLUMN_ICON, p_pixbuf,
                           COLUMN_LAYOUT, layout,
                           COLUMN_VARIANT, variant,
                           -1);
        g_object_unref(G_OBJECT(p_pixbuf));
    }
    else
    {
        gtk_list_store_set(p_xkb->p_liststore_layout, &tree_iter,
                           COLUMN_LAYOUT, layout,
                           COLUMN_VARIANT, variant,
                           -1);
    }
    g_free(flag_filepath);
}

static void xkb_settings_fill_layout_tree_model_with_config(XkbPlugin *p_xkb)
{
    p_xkb->num_layouts = 0;
    if(strlen(p_xkb->kbd_layouts) && strlen(p_xkb->kbd_variants))
    {
        char **layouts = g_strsplit_set(p_xkb->kbd_layouts, ",", 0);
        char **variants = g_strsplit_set(p_xkb->kbd_variants, ",", 0);

        while(layouts[p_xkb->num_layouts] != NULL)
        {
            xkb_add_layout(p_xkb, layouts[p_xkb->num_layouts], variants[p_xkb->num_layouts]);
            p_xkb->num_layouts++;
        }
        gtk_widget_set_sensitive(p_xkb->p_button_rm_layout, p_xkb->num_layouts > 1);

        g_strfreev(layouts);
        g_strfreev(variants);
    }
}

/* Callback when the configuration dialog is to be shown. */
static void xkb_configure(Plugin * p, GtkWindow * parent)
{
    XkbPlugin * p_xkb = PRIV(p);
    gchar       markup_str[MAX_MARKUP_LEN];

    user_active = FALSE;

    // configuration dialog
    GtkWidget * dlg = gtk_dialog_new_with_buttons(
        _("Keyboard Layout Handler"),
        NULL,
        GTK_DIALOG_NO_SEPARATOR,
        GTK_STOCK_CLOSE,
        GTK_RESPONSE_OK,
        NULL);
    p_xkb->p_dialog_config = dlg;
    panel_apply_icon(GTK_WINDOW(dlg));

    // main vbox of the config dialog
    GtkWidget * p_vbox_main = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox), p_vbox_main);


    // 'KEYBOARD MODEL' frame
    GtkWidget * p_frame_kbd_model = gtk_frame_new(NULL);
    GtkWidget * p_label_kbd_model = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Keyboard Model"));
    gtk_label_set_markup(GTK_LABEL(p_label_kbd_model), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_kbd_model), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_frame_kbd_model), p_label_kbd_model);
    gtk_box_pack_start(GTK_BOX(p_vbox_main), p_frame_kbd_model, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_frame_kbd_model), 3);

    // frame alignment
    GtkWidget * p_alignment_kbd_model = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_frame_kbd_model), p_alignment_kbd_model);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_kbd_model), 4, 4, 10, 10);
    p_xkb->p_button_kbd_model = gtk_button_new_with_label(p_xkb->kbd_model);
    g_signal_connect(p_xkb->p_button_kbd_model, "clicked", G_CALLBACK(on_button_kbd_model_clicked), p_xkb);
    gtk_container_add(GTK_CONTAINER(p_alignment_kbd_model), p_xkb->p_button_kbd_model);


    // 'KEYBOARD LAYOUTS' frame
    GtkWidget * p_frame_kbd_layouts = gtk_frame_new(NULL);
    GtkWidget * p_label_kbd_layouts = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Keyboard Layouts"));
    gtk_label_set_markup(GTK_LABEL(p_label_kbd_layouts), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_kbd_layouts), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_frame_kbd_layouts), p_label_kbd_layouts);
    gtk_box_pack_start(GTK_BOX(p_vbox_main), p_frame_kbd_layouts, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_frame_kbd_layouts), 3);
    gtk_widget_set_size_request(GTK_WIDGET(p_frame_kbd_layouts), -1, 180);

    // frame alignment
    GtkWidget * p_alignment_kbd_layouts = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_frame_kbd_layouts), p_alignment_kbd_layouts);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_kbd_layouts), 4, 4, 10, 10);
    GtkWidget * p_hbox_kbd_layouts = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(p_alignment_kbd_layouts), p_hbox_kbd_layouts);

    // scrolledwindow and buttons
    GtkWidget * p_scrolledwindow_kbd_layouts = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p_scrolledwindow_kbd_layouts),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(p_hbox_kbd_layouts), p_scrolledwindow_kbd_layouts, TRUE, TRUE, 2);
    GtkWidget * p_vbox_kbd_layouts = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p_hbox_kbd_layouts), p_vbox_kbd_layouts, FALSE, TRUE, 2);
    GtkWidget * p_button_add_layout = gtk_button_new_from_stock(GTK_STOCK_ADD);
    GtkWidget * p_button_up_layout = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
    GtkWidget * p_button_down_layout = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
    p_xkb->p_button_rm_layout = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    g_signal_connect(p_button_add_layout, "clicked", G_CALLBACK(on_button_add_layout_clicked), p_xkb);
    g_signal_connect(p_xkb->p_button_rm_layout, "clicked", G_CALLBACK(on_button_rm_layout_clicked), p_xkb);
    g_signal_connect(p_button_up_layout, "clicked", G_CALLBACK(on_button_up_layout_clicked), p_xkb);
    g_signal_connect(p_button_down_layout, "clicked", G_CALLBACK(on_button_down_layout_clicked), p_xkb);
    gtk_box_pack_start(GTK_BOX(p_vbox_kbd_layouts), p_button_add_layout, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_vbox_kbd_layouts), p_button_up_layout, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_vbox_kbd_layouts), p_button_down_layout, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_vbox_kbd_layouts), p_xkb->p_button_rm_layout, TRUE, TRUE, 0);

    // liststore
    p_xkb->p_liststore_layout = gtk_list_store_new(NUM_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
    p_xkb->p_treeview_layout = gtk_tree_view_new_with_model(GTK_TREE_MODEL(p_xkb->p_liststore_layout));
    g_object_unref(G_OBJECT(p_xkb->p_liststore_layout));
    p_xkb->p_treeselection_layout = gtk_tree_view_get_selection(GTK_TREE_VIEW(p_xkb->p_treeview_layout));
    gtk_container_add(GTK_CONTAINER(p_scrolledwindow_kbd_layouts), p_xkb->p_treeview_layout);
    GtkCellRenderer *p_renderer;
    GtkTreeViewColumn *p_column;
    // icon
    p_renderer = gtk_cell_renderer_pixbuf_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Flag"), p_renderer, "pixbuf", COLUMN_ICON, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_xkb->p_treeview_layout), p_column);
    // layout
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Layout"), p_renderer, "text", COLUMN_LAYOUT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_xkb->p_treeview_layout), p_column);
    // variant
    p_renderer = gtk_cell_renderer_text_new();
    p_column = gtk_tree_view_column_new_with_attributes(_("Variant"), p_renderer, "text", COLUMN_VARIANT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(p_xkb->p_treeview_layout), p_column);
    xkb_settings_fill_layout_tree_model_with_config(p_xkb);


    // 'CHANGE LAYOUT OPTION' frame
    GtkWidget * p_frame_change_layout = gtk_frame_new(NULL);
    GtkWidget * p_label_change_layout = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Change Layout Option"));
    gtk_label_set_markup(GTK_LABEL(p_label_change_layout), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_change_layout), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_frame_change_layout), p_label_change_layout);
    gtk_box_pack_start(GTK_BOX(p_vbox_main), p_frame_change_layout, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_frame_change_layout), 3);

    // frame alignment
    GtkWidget * p_alignment_change_layout = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_frame_change_layout), p_alignment_change_layout);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_change_layout), 4, 4, 10, 10);
    p_xkb->p_button_change_layout = gtk_button_new_with_label(p_xkb->kbd_change_option);
    g_signal_connect(p_xkb->p_button_change_layout, "clicked", G_CALLBACK(on_button_kbd_change_layout_clicked), p_xkb);
    gtk_container_add(GTK_CONTAINER(p_alignment_change_layout), p_xkb->p_button_change_layout);


    // 'SHOW LAYOUT AS' frame
    GtkWidget * p_frame_display_type = gtk_frame_new(NULL);
    GtkWidget * p_label_show_layout_as = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<b>%s</b>", _("Show Layout as"));
    gtk_label_set_markup(GTK_LABEL(p_label_show_layout_as), markup_str);
    gtk_misc_set_padding(GTK_MISC(p_label_show_layout_as), 1, 0);
    gtk_frame_set_label_widget(GTK_FRAME(p_frame_display_type), p_label_show_layout_as);
    gtk_box_pack_start(GTK_BOX(p_vbox_main), p_frame_display_type, TRUE, TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(p_frame_display_type), 3);

    // frame alignment
    GtkWidget * p_alignment_display_type = gtk_alignment_new(0.5, 0.5, 1, 1);
    gtk_container_add(GTK_CONTAINER(p_frame_display_type), p_alignment_display_type);
    gtk_alignment_set_padding(GTK_ALIGNMENT(p_alignment_display_type), 4, 4, 10, 10);
    GtkWidget * p_hbox_disp_type = gtk_hbox_new(TRUE, 10);
    gtk_container_add(GTK_CONTAINER(p_alignment_display_type), p_hbox_disp_type);

    // radiobuttons
    #define FLAG_ICON_SIZE  32
    GtkWidget * p_hbox_disp_type_image = gtk_hbox_new(FALSE, 3);
    GtkWidget * p_hbox_disp_type_text = gtk_hbox_new(FALSE, 3);
    GtkWidget * p_image_disp_type_image = gtk_image_new();
    char * symbol_name_lowercase = (char *)xkb_get_current_symbol_name_lowercase(p_xkb);

    gchar *flag_filepath = get_flag_path_for_layout(symbol_name_lowercase);

    GdkPixbuf * unscaled_pixbuf = gdk_pixbuf_new_from_file(flag_filepath, NULL);
    g_free(flag_filepath);
    g_free(symbol_name_lowercase);
    if(unscaled_pixbuf != NULL)
    {
        /* Loaded successfully. */
        int width = gdk_pixbuf_get_width(unscaled_pixbuf);
        int height = gdk_pixbuf_get_height(unscaled_pixbuf);
        GdkPixbuf * pixbuf = gdk_pixbuf_scale_simple(unscaled_pixbuf, FLAG_ICON_SIZE * width / height, FLAG_ICON_SIZE, GDK_INTERP_BILINEAR);
        if(pixbuf != NULL)
        {
            gtk_image_set_from_pixbuf(GTK_IMAGE(p_image_disp_type_image), pixbuf);
            g_object_unref(G_OBJECT(pixbuf));
        }
        g_object_unref(unscaled_pixbuf);
    }
    GtkWidget * p_label_disp_type_text = gtk_label_new(NULL);
    snprintf(markup_str, MAX_MARKUP_LEN, "<span font='%d' font_weight='heavy'>%s</span>", FLAG_ICON_SIZE/2, xkb_get_current_symbol_name(p_xkb));
    gtk_label_set_markup(GTK_LABEL(p_label_disp_type_text), markup_str);
    GtkWidget * p_radiobutton_disp_type_image = gtk_radio_button_new_with_label(NULL, (const gchar *)_("image"));
    GtkWidget * p_radiobutton_disp_type_text = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(p_radiobutton_disp_type_image), (const gchar *)_("text"));
    gtk_box_pack_start(GTK_BOX(p_hbox_disp_type), p_hbox_disp_type_image, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_hbox_disp_type), p_hbox_disp_type_text, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_hbox_disp_type_image), p_image_disp_type_image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p_hbox_disp_type_image), p_radiobutton_disp_type_image, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(p_hbox_disp_type_text), p_label_disp_type_text, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p_hbox_disp_type_text), p_radiobutton_disp_type_text, TRUE, TRUE, 0);
    g_signal_connect(p_radiobutton_disp_type_image, "clicked", G_CALLBACK(on_xkb_radiobutton_disp_type_image_clicked), p_xkb);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_disp_type_image), p_xkb->display_type == DISP_TYPE_IMAGE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(p_radiobutton_disp_type_text), p_xkb->display_type == DISP_TYPE_TEXT);

    /* Connect signals. */
    g_signal_connect(p_xkb->p_dialog_config, "response", G_CALLBACK(on_xkb_dialog_config_response), p_xkb);

    /* Display the dialog. */
    gtk_widget_set_size_request(p_xkb->p_dialog_config, 350, -1);
    gtk_widget_show_all(p_xkb->p_dialog_config);
    gtk_window_present(GTK_WINDOW(p_xkb->p_dialog_config));

    user_active = TRUE;
}

/* Callback when the configuration is to be saved. */
static void xkb_save_configuration(Plugin * p, FILE * fp)
{
    XkbPlugin * p_xkb = PRIV(p);
    lxpanel_put_int(fp, "DisplayType", p_xkb->display_type);
    lxpanel_put_str(fp, "Model", p_xkb->kbd_model);
    lxpanel_put_str(fp, "LayoutsList", p_xkb->kbd_layouts);
    lxpanel_put_str(fp, "VariantsList", p_xkb->kbd_variants);
    lxpanel_put_str(fp, "ToggleOpt", p_xkb->kbd_change_option);
}

/* Callback when panel configuration changes. */
static void xkb_panel_configuration_changed(Plugin * p)
{
    /* Do a full redraw. */
    XkbPlugin * p_xkb = PRIV(p);
    xkb_redraw(p_xkb);
}

/* Plugin descriptor. */
PluginClass xkbh_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "xkbh",
    name : N_("Keyboard Layouts Handler"),
    version: "2.0",
    description : N_("Handle keyboard layouts"),

    constructor : xkb_constructor,
    destructor  : xkb_destructor,
    config : xkb_configure,
    save : xkb_save_configuration,
    panel_configuration_changed : xkb_panel_configuration_changed

};
