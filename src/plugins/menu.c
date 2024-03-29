/**
 * Copyright (c) 2011-2012 Vadim Ushakov
 * Copyright (c) 2006 LxDE Developers, see the file AUTHORS for details.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <glib/gi18n.h>

#include <lxpanelx/menu-cache-compat.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define PLUGIN_PRIV_TYPE menup

#include <lxpanelx/gtkcompat.h>
#include <lxpanelx/global.h>
#include <lxpanelx/panel.h>
#include <lxpanelx/misc.h>
#include <lxpanelx/plugin.h>
#include <lxpanelx/fb_button.h>
#include "bg.h"
#include "menu-policy.h"
#include "commands.h"

#include <lxpanelx/dbg.h>

extern void gtk_run(void); /* FIXME! */

//#define DEFAULT_MENU_ICON PACKAGE_DATA_DIR "/lxpanelx/images/my-computer.png"
#define DEFAULT_MENU_ICON "start-here"



/* Test of haystack has the needle prefix, comparing case
 * insensitive. haystack may be UTF-8, but needle must
 * contain only lowercase ascii. */
static gboolean
has_case_prefix (const gchar *haystack,
                 const gchar *needle)
{
  const gchar *h, *n;

  /* Eat one character at a time. */
  h = haystack;
  n = needle;

  while (*n && *h && *n == g_ascii_tolower (*h))
    {
      n++;
      h++;
    }

  return *n == '\0';
}


typedef struct {
    GtkWidget *menu, *box, *img, *label;
    char *fname, *caption;
    gulong handler_id;
    int iconsize, paneliconsize;
    GSList *files;
    char* config_data;
    int sysmenu_pos;
    char *config_start, *config_end;

    MenuCache* menu_cache;
    guint visibility_flags;
    gpointer reload_notify;

    gboolean has_run_command;
} menup;

static guint idle_loader = 0;

GQuark SYS_MENU_ITEM_ID = 0;

static void
menu_destructor(Plugin *p)
{
    menup *m = PRIV(p);

    if( G_UNLIKELY( idle_loader ) )
    {
        g_source_remove( idle_loader );
        idle_loader = 0;
    }

    g_signal_handler_disconnect(G_OBJECT(m->img), m->handler_id);
    gtk_widget_destroy(m->menu);

    if( m->menu_cache )
    {
        menu_cache_remove_reload_notify(m->menu_cache, m->reload_notify);
        menu_cache_unref( m->menu_cache );
    }

    g_free(m->fname);
    g_free(m->caption);
    g_free(m);
    RET();
}

static void
spawn_app(GtkWidget *widget, gpointer data)
{
    GError *error = NULL;

    ENTER;
    if (data) {
        if (! g_spawn_command_line_async(data, &error) ) {
            ERR("can't spawn %s\nError is %s\n", (char *)data, error->message);
            g_error_free (error);
        }
    }
    RET();
}


static void
run_command(GtkWidget *widget, void (*cmd)(void))
{
    ENTER;
    cmd();
    RET();
}

static void
menu_pos(GtkWidget *menu, gint *px, gint *py, gboolean *push_in, Plugin * p)
{
    /* Get the allocation of the popup menu. */
    GtkRequisition popup_req;
    gtk_widget_size_request(menu, &popup_req);

    /* Determine the coordinates. */
    plugin_popup_set_position_helper(p, plugin_widget(p), menu, &popup_req, px, py);
    *push_in = TRUE;
}

static void on_menu_item( GtkMenuItem* mi, MenuCacheItem* item )
{
    lxpanel_launch_app( menu_cache_app_get_exec(MENU_CACHE_APP(item)),
            NULL, menu_cache_app_get_use_terminal(MENU_CACHE_APP(item)));
}

/* load icon when mapping the menu item to speed up */
static void on_menu_item_map(GtkWidget* mi, MenuCacheItem* item)
{
    GtkImage* img = GTK_IMAGE(gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(mi)));
    if( img )
    {
        if( gtk_image_get_storage_type(img) == GTK_IMAGE_EMPTY )
        {
            GdkPixbuf* icon;
            int w, h;
            /* FIXME: this is inefficient */
            gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &w, &h);
            item = g_object_get_qdata(G_OBJECT(mi), SYS_MENU_ITEM_ID);
            icon = lxpanel_load_icon(menu_cache_item_get_icon(item), w, h, TRUE);
            if (icon)
            {
                gtk_image_set_from_pixbuf(img, icon);
                g_object_unref(icon);
            }
        }
    }
}

static void on_menu_item_style_set(GtkWidget* mi, GtkStyle* prev, MenuCacheItem* item)
{
    /* reload icon */
    on_menu_item_map(mi, item);
}

static void on_add_menu_item_to_desktop(GtkMenuItem* item, MenuCacheApp* app)
{
    char* dest;
    char* src;
    const char* desktop = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
    int dir_len = strlen(desktop);
    int basename_len = strlen(menu_cache_item_get_id(MENU_CACHE_ITEM(app)));
    int dest_fd;

    dest = g_malloc( dir_len + basename_len + 6 + 1 + 1 );
    memcpy(dest, desktop, dir_len);
    dest[dir_len] = '/';
    memcpy(dest + dir_len + 1, menu_cache_item_get_id(MENU_CACHE_ITEM(app)), basename_len + 1);

    /* if the destination file already exists, make a unique name. */
    if( g_file_test( dest, G_FILE_TEST_EXISTS ) )
    {
        memcpy( dest + dir_len + 1 + basename_len - 8 /* .desktop */, "XXXXXX.desktop", 15 );
        dest_fd = g_mkstemp(dest);
        if( dest_fd >= 0 )
            chmod(dest, 0600);
    }
    else
    {
        dest_fd = creat(dest, 0600);
    }

    if( dest_fd >=0 )
    {
        char* data;
        gsize len;
        src = menu_cache_item_get_file_path(MENU_CACHE_ITEM(app));
        if( g_file_get_contents(src, &data, &len, NULL) )
        {
            write( dest_fd, data, len );
            g_free(data);
        }
        close(dest_fd);
        g_free(src);
    }
    g_free(dest);
}


static Plugin * get_launchbar_plugin(void)
{
    /* Find a penel containing launchbar applet.
     * The launchbar with most buttons will be choosen if
     * there are several launchbar applets loaded.
     */

    GSList * l;
    Plugin * lb = NULL;
    int prio = -1;

    for (l = get_all_panels(); !lb && l; l = l->next)
    {
        Panel * panel = (Panel *) l->data;
        GList * pl;
        for (pl = panel_get_plugins(panel); pl; pl = pl->next)
        {
            Plugin* plugin = (Plugin *) pl->data;
            if (plugin_class(plugin)->add_launch_item && plugin_class(plugin)->get_priority_of_launch_item_adding)
            {
                int n = plugin_class(plugin)->get_priority_of_launch_item_adding(plugin);
                if( n > prio )
                {
                    lb = plugin;
                    prio = n;
                }
            }
        }
    }

    return lb;
}


static void on_add_menu_item_to_panel(GtkMenuItem* item, MenuCacheApp* app)
{
    /*
    FIXME: let user choose launchbar
    */

    Plugin * lb = get_launchbar_plugin();
    if (lb)
    {
        plugin_class(lb)->add_launch_item(lb, menu_cache_item_get_file_basename(MENU_CACHE_ITEM(app)));
    }
}

static void on_menu_item_properties(GtkMenuItem* item, MenuCacheApp* app)
{
    /* FIXME: if the source desktop is in AppDir other then default
     * applications dirs, where should we store the user-specific file?
    */
    char* ifile = menu_cache_item_get_file_path(MENU_CACHE_ITEM(app));
    char* ofile = g_build_filename(g_get_user_data_dir(), "applications",
				   menu_cache_item_get_file_basename(MENU_CACHE_ITEM(app)), NULL);
    char* argv[] = {
        "lxshortcut",
        "-i",
        NULL,
        "-o",
        NULL,
        NULL};
    argv[2] = ifile;
    argv[4] = ofile;
    g_spawn_async( NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL );
    g_free( ifile );
    g_free( ofile );
}

static gboolean on_menu_button_press(GtkWidget* mi, GdkEventButton* evt, MenuCacheItem* data)
{
    if( evt->button == 3)  /* right */
    {
        if (lxpanel_is_in_kiosk_mode())
            return TRUE;

        char* tmp;
        GtkWidget* item;
        GtkMenu* p = GTK_MENU(gtk_menu_new());

        item = gtk_menu_item_new_with_label(_("Add to desktop"));
        g_signal_connect(item, "activate", G_CALLBACK(on_add_menu_item_to_desktop), data);
        gtk_menu_shell_append(GTK_MENU_SHELL(p), item);

        if (get_launchbar_plugin())
        {
            item = gtk_menu_item_new_with_label(_("Add to launch bar"));
            g_signal_connect(item, "activate", G_CALLBACK(on_add_menu_item_to_panel), data);
            gtk_menu_shell_append(GTK_MENU_SHELL(p), item);
        }

        tmp = g_find_program_in_path("lxshortcut");
        if( tmp )
        {
            item = gtk_separator_menu_item_new();
            gtk_menu_shell_append(GTK_MENU_SHELL(p), item);

            item = gtk_menu_item_new_with_label(_("Properties"));
            g_signal_connect(item, "activate", G_CALLBACK(on_menu_item_properties), data);
            gtk_menu_shell_append(GTK_MENU_SHELL(p), item);
            g_free(tmp);
        }
        g_signal_connect(p, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);
        g_signal_connect(p, "deactivate", G_CALLBACK(restore_grabs), mi);

        gtk_widget_show_all(GTK_WIDGET(p));
        gtk_menu_popup(p, NULL, NULL, NULL, NULL, 0, evt->time);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_menu_button_release(GtkWidget* mi, GdkEventButton* evt, MenuCacheItem* data)
{
    if( evt->button == 3)
    {
        return TRUE;
    }

    return FALSE;
}

static GtkWidget* create_item( MenuCacheItem* item )
{
    GtkWidget* mi;
    if( menu_cache_item_get_type(item) == MENU_CACHE_TYPE_SEP )
        mi = gtk_separator_menu_item_new();
    else
    {
        GtkWidget* img;
        mi = gtk_image_menu_item_new_with_label( menu_cache_item_get_name(item) );
        img = gtk_image_new();
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(mi), img );
        if( menu_cache_item_get_type(item) == MENU_CACHE_TYPE_APP )
        {
            const gchar * tooltip = menu_cache_item_get_comment(item);
/*
            FIXME: to be implemented in menu-cache
            if (strempty(tooltip))
                tooltip = menu_cache_item_get_generic_name(item);
*/
            gchar * additional_tooltip = NULL;

            const gchar * commandline = menu_cache_app_get_exec(MENU_CACHE_APP(item));

            gchar ** commandline_list = NULL;
            if (commandline)
                commandline_list = g_strsplit_set(commandline, " \t", 0);

            const gchar * executable = NULL;

            if (commandline_list && *commandline_list)
            {
                executable = *commandline_list;
                gchar ** p;
                for (p = commandline_list + 1; *p; p++)
                {
                     if (**p != '%')
                     {
                         executable = NULL;
                         break;
                     }
                }
            }

            if (executable)
            {
                if (strempty(tooltip))
                {
                    additional_tooltip = g_strdup(executable);
                }
                else
                {
                    gchar * s0 = g_ascii_strdown(executable, -1);
                    gchar * s1 = g_ascii_strdown(tooltip, -1);
                    gchar * s2 = g_ascii_strdown(menu_cache_item_get_name(item), -1);
                    if (!strstr(s1, s0) && !strstr(s2, s0))
                    {
                        additional_tooltip = g_strdup_printf(_("%s\n[%s]"), tooltip, executable);
                    }
                }
            }

            g_strfreev(commandline_list);

            if (additional_tooltip)
            {
                gtk_widget_set_tooltip_text(mi, additional_tooltip);
                g_free(additional_tooltip);
            }
            else
            {
               gtk_widget_set_tooltip_text(mi, tooltip);
            }

            g_signal_connect( mi, "activate", G_CALLBACK(on_menu_item), item );
        }
        g_signal_connect(mi, "map", G_CALLBACK(on_menu_item_map), item);
        g_signal_connect(mi, "style-set", G_CALLBACK(on_menu_item_style_set), item);
        g_signal_connect(mi, "button-press-event", G_CALLBACK(on_menu_button_press), item);
        g_signal_connect(mi, "button-release-event", G_CALLBACK(on_menu_button_release), item);
    }
    gtk_widget_show( mi );
    g_object_set_qdata_full( G_OBJECT(mi), SYS_MENU_ITEM_ID, menu_cache_item_ref(item), (GDestroyNotify) menu_cache_item_unref );
    return mi;
}

static int load_menu(menup* m, MenuCacheDir* dir, GtkWidget* menu, int pos )
{
    GSList * l;
    /* number of visible entries */
    gint count = 0;		
    for( l = menu_cache_dir_get_children(dir); l; l = l->next )
    {
        MenuCacheItem* item = MENU_CACHE_ITEM(l->data);
	
        gboolean is_visible = ((menu_cache_item_get_type(item) != MENU_CACHE_TYPE_APP) || 
			       (panel_menu_item_evaluate_visibility(item, m->visibility_flags)));
	
	if (is_visible) 
	{
            GtkWidget * mi = create_item(item);
	    count++;
            if (mi != NULL)
                gtk_menu_shell_insert( (GtkMenuShell*)menu, mi, pos );
                if( pos >= 0 )
                    ++pos;
		/* process subentries */
		if (menu_cache_item_get_type(item) == MENU_CACHE_TYPE_DIR) 
		{
                    GtkWidget* sub = gtk_menu_new();
		    /*  always pass -1 for position */
		    gint s_count = load_menu( m, MENU_CACHE_DIR(item), sub, -1 );    
                    if (s_count) 
			gtk_menu_item_set_submenu( GTK_MENU_ITEM(mi), sub );	    
		    else 
		    {
			/* don't keep empty submenus */
			gtk_widget_destroy( sub );
			gtk_widget_destroy( mi );
			if (pos > 0)
			    pos--;
		    }
		}
	}
    }
    return count;
}



static gboolean sys_menu_item_has_data( GtkMenuItem* item )
{
   return (g_object_get_qdata( G_OBJECT(item), SYS_MENU_ITEM_ID ) != NULL);
}

static void unload_old_icons(GtkMenu* menu, GtkIconTheme* theme)
{
    GList *children, *child;
    GtkMenuItem* item;
    GtkWidget* sub_menu=NULL;

    children = gtk_container_get_children( GTK_CONTAINER(menu) );
    for( child = children; child; child = child->next )
    {
        item = GTK_MENU_ITEM( child->data );
        if( sys_menu_item_has_data( item ) )
        {
            GtkImage* img;
            item = GTK_MENU_ITEM( child->data );
            if( GTK_IS_IMAGE_MENU_ITEM(item) )
            {
	        img = GTK_IMAGE(gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(item)));
                gtk_image_clear(img);
                if( gtk_widget_get_mapped(GTK_WIDGET(img)) )
		    on_menu_item_map(GTK_WIDGET(item),
			(MenuCacheItem*)g_object_get_qdata(G_OBJECT(item), SYS_MENU_ITEM_ID) );
            }
        }
        else if( ( sub_menu = gtk_menu_item_get_submenu( item ) ) )
        {
	    unload_old_icons( GTK_MENU(sub_menu), theme );
        }
    }
    g_list_free( children );
}

static void remove_change_handler(gpointer id, GObject* menu)
{
    g_signal_handler_disconnect(gtk_icon_theme_get_default(), GPOINTER_TO_INT(id));
}

/*
 * Insert application menus into specified menu
 * menu: The parent menu to which the items should be inserted
 * pisition: Position to insert items.
             Passing -1 in this parameter means append all items
             at the end of menu.
 */
static void sys_menu_insert_items( menup* m, GtkMenu* menu, int position )
{
    MenuCacheDir* dir;
    guint change_handler;

    if( G_UNLIKELY( SYS_MENU_ITEM_ID == 0 ) )
        SYS_MENU_ITEM_ID = g_quark_from_static_string( "SysMenuItem" );

    dir = menu_cache_get_root_dir( m->menu_cache );
    if(dir)
        load_menu( m, dir, GTK_WIDGET(menu), position );
    else /* menu content is empty */
    {
        /* add a place holder */
        GtkWidget* mi = gtk_menu_item_new();
        g_object_set_qdata( G_OBJECT(mi), SYS_MENU_ITEM_ID, GINT_TO_POINTER(1) );
        gtk_menu_shell_insert(GTK_MENU_SHELL(menu), mi, position);
    }

    change_handler = g_signal_connect_swapped( gtk_icon_theme_get_default(), "changed", G_CALLBACK(unload_old_icons), menu );
    g_object_weak_ref( G_OBJECT(menu), remove_change_handler, GINT_TO_POINTER(change_handler) );
}


static void
reload_system_menu( menup* m, GtkMenu* menu )
{
    GList *children, *child;
    GtkMenuItem* item;
    GtkWidget* sub_menu;
    gint idx;
    //gboolean found = FALSE;

    children = gtk_container_get_children( GTK_CONTAINER(menu) );
    for( child = children, idx = 0; child; child = child->next, ++idx )
    {
        item = GTK_MENU_ITEM( child->data );
        if( sys_menu_item_has_data( item ) )
        {
            do
            {
                item = GTK_MENU_ITEM( child->data );
                child = child->next;
                gtk_widget_destroy( GTK_WIDGET(item) );
            }while( child && sys_menu_item_has_data( child->data ) );
            sys_menu_insert_items( m, menu, idx );
            if( ! child )
                break;
            //found = TRUE;
        }
        else if( ( sub_menu = gtk_menu_item_get_submenu( item ) ) )
        {
            reload_system_menu( m, GTK_MENU(sub_menu) );
        }
    }
    g_list_free( children );
}

static void show_menu( GtkWidget* widget, Plugin* p, int btn, guint32 time )
{
    menup* m = PRIV(p);
    gtk_menu_popup(GTK_MENU(m->menu),
                   NULL, NULL,
                   (GtkMenuPositionFunc)menu_pos, p,
                   btn, time);
}

static gboolean
my_button_pressed(GtkWidget *widget, GdkEventButton *event, Plugin* plugin)
{
    ENTER;

    menup *m = PRIV(plugin);

    /* Standard right-click handling. */
    if (plugin_button_press_event(widget, event, plugin))
        return TRUE;

    if ((event->type == GDK_BUTTON_PRESS)
          && (event->x >=0 && event->x < widget->allocation.width)
          && (event->y >=0 && event->y < widget->allocation.height))
    {
        if (m->has_run_command && event->button == 2)
        {
            gtk_run();
        }
        else
        {
            show_menu( widget, plugin, event->button, event->time );
        }
    }
    RET(TRUE);
}

static
void menu_open_system_menu(struct _Plugin * p)
{
    menup* m = PRIV(p);
    show_menu( m->img, p, 0, GDK_CURRENT_TIME );
}

static GtkWidget *
make_button(Plugin *p, gchar *fname, gchar *name, GdkColor* tint, GtkWidget *menu)
{
    char* title = NULL;
    menup *m;

    ENTER;
    m = PRIV(p);
    m->menu = menu;

    if( name )
    {
        title = panel_translate_directory_name(name);
        m->img = fb_button_new_from_file_with_text(fname, -1, plugin_get_icon_size(p), p, title);
        g_free(title);
    }
    else
    {
        m->img = fb_button_new_from_file(fname, -1, plugin_get_icon_size(p), p);
    }

    fb_button_set_orientation(m->img, plugin_get_orientation(p));

    gtk_widget_show(m->img);
    gtk_box_pack_start(GTK_BOX(m->box), m->img, TRUE, TRUE, 0);

    m->handler_id = g_signal_connect (G_OBJECT (m->img), "button-press-event",
          G_CALLBACK (my_button_pressed), p);
    g_object_set_data(G_OBJECT(m->img), "plugin", p);

    RET(m->img);
}


static GtkWidget *
read_item(Plugin *p, char** fp)
{
    ENTER;

    menup* m = PRIV(p);

    line s;
    gchar *name, *fname, *action;
    GtkWidget *item;
    Command *cmd_entry = NULL;

    name = fname = action = NULL;

    if( fp )
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
            if (s.type == LINE_VAR) {
                if (!g_ascii_strcasecmp(s.t[0], "image"))
                    fname = expand_tilda(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "name"))
                    name = g_strdup(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "action"))
                    action = g_strdup(s.t[1]);
                else if (!g_ascii_strcasecmp(s.t[0], "command")) {

                    if (!g_ascii_strcasecmp(s.t[1], "run"))
                    {
                        m->has_run_command = TRUE;
                    }

                    Command *tmp;

                    for (tmp = commands; tmp->name; tmp++) {
                        if (!g_ascii_strcasecmp(s.t[1], tmp->name)) {
                            cmd_entry = tmp;
                            break;
                        }
                    }
                } else {
                    ERR( "menu/item: unknown var %s\n", s.t[0]);
                    goto error;
                }
            }
        }
    }
    /* menu button */
    if( cmd_entry ) /* built-in commands */
    {
        item = gtk_image_menu_item_new_with_label( _(cmd_entry->disp_name) );
        g_signal_connect(G_OBJECT(item), "activate", (GCallback)run_command, cmd_entry->cmd);
    }
    else
    {
        item = gtk_image_menu_item_new_with_label(name ? name : "");
        if (action) {
            g_signal_connect(G_OBJECT(item), "activate", (GCallback)spawn_app, action);
        }
    }
    gtk_container_set_border_width(GTK_CONTAINER(item), 0);
    g_free(name);
    if (fname) {
        GtkWidget *img;

        img = _gtk_image_new_from_file_scaled(fname, m->iconsize, m->iconsize, TRUE, TRUE);
        gtk_widget_show(img);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
        g_free(fname);
    }
    RET(item);

 error:
    g_free(fname);
    g_free(name);
    g_free(action);
    RET(NULL);
}

static GtkWidget *
read_separator(Plugin *p, char **fp)
{
    line s;

    ENTER;
    if( fp )
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
            ERR("menu: error - separator can not have paramteres\n");
            RET(NULL);
        }
    }
    RET(gtk_separator_menu_item_new());
}

static void on_reload_menu( MenuCache* cache, menup* m )
{
    /* g_debug("reload system menu!!"); */
    reload_system_menu( m, GTK_MENU(m->menu) );
}

#if 0
static void ru_menuitem_open(GtkWidget * item, Plugin * p)
{
    open_in_file_manager(p, g_object_get_data(G_OBJECT(item), "uri"));
}

static void
read_recently_used_menu(GtkMenu* menu, Plugin *p, char** fp)
{
    menup *m = PRIV(p);

    line s;
    if( fp )
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
            ERR("menu: error - system can not have paramteres\n");
            return;
        }
    }

    GtkRecentManager * rm = gtk_recent_manager_get_default();
    GList * rl = gtk_recent_manager_get_items(rm);
    GList *l;
    for (l = rl; l; l = l->next)
    {
        GtkRecentInfo *info = l->data;
        const char * uri = gtk_recent_info_get_uri(info);

        //GFile * file = g_file_new_for_uri(uri);
        //if (g_file_query_exists(file, NULL) || gtk_recent_info_get_private_hint(info))
        if (!gtk_recent_info_get_private_hint(info) && gtk_recent_info_exists(info))
        {
	    const char * display_name = gtk_recent_info_get_display_name(info);

	    GdkPixbuf * pixbuf = gtk_recent_info_get_icon(info, m->iconsize);
	    GtkWidget* img = gtk_image_new_from_pixbuf(pixbuf);
	    g_object_unref(pixbuf);

	    GtkWidget* mi = gtk_image_menu_item_new_with_label(display_name);
	    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(mi), img);
	    g_object_set_data_full(G_OBJECT(mi), "uri", g_strdup(uri), g_free);

            gchar * tooltip = g_strdup_printf("%d", gtk_recent_info_get_age(info));
            gtk_widget_set_tooltip_text(mi, tooltip);
            g_free(tooltip);

	    g_signal_connect(G_OBJECT(mi), "activate", G_CALLBACK(ru_menuitem_open), p);

	    gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
	    gtk_widget_show(mi);
        }

        //g_object_unref(file);
        gtk_recent_info_unref(info);
    }
    g_list_free(rl);

    //GtkWidget* mi = gtk_menu_item_new();
    //gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
}
#endif

static gboolean
recent_documents_filter (const GtkRecentFilterInfo *filter_info, gpointer user_data)
{
    if (!filter_info)
         return FALSE;

    if (!filter_info->uri)
         return FALSE;
/*
g_print("%s\n", filter_info->uri);
int i;
for (i = 0; filter_info->applications[i]; i++)
{
    g_print("    %s\n", filter_info->applications[i]);
}
*/
    if (!has_case_prefix(filter_info->uri, "file:/"))
        return TRUE;

    gchar * filename = g_filename_from_uri(filter_info->uri, NULL, NULL);

    if (!filename)
        return FALSE;

    gboolean result = FALSE;

    struct stat stat_buf;
    if (stat(filename, &stat_buf) == 0)
        result = TRUE;

    g_free(filename);

    return result;
}

static void
recent_documents_activate_cb (GtkRecentChooser *chooser, Plugin * p)
{
    GtkRecentInfo * recent_info = gtk_recent_chooser_get_current_item (chooser);
    const char    * uri = gtk_recent_info_get_uri (recent_info);

    lxpanel_open_in_file_manager(uri);

    gtk_recent_info_unref (recent_info);
}

static void
read_recent_documents_menu(GtkMenu* menu, Plugin *p, char** fp)
{
    menup *m = PRIV(p);

    int limit = 20;
    gboolean show_private = FALSE;
    gboolean local_only = FALSE;
    gboolean show_tips = TRUE;

    line s;
    if( fp )
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
            if (s.type == LINE_VAR) {
		m->config_start = *fp;
		if (!g_ascii_strcasecmp(s.t[0], "limit"))
		    limit = atoi(s.t[1]);
		else if (!g_ascii_strcasecmp(s.t[0], "showprivate"))
		    show_private = str2num(bool_pair, s.t[1], show_private);
		else if (!g_ascii_strcasecmp(s.t[0], "localonly"))
		    local_only = str2num(bool_pair, s.t[1], local_only);
		else if (!g_ascii_strcasecmp(s.t[0], "showtips"))
		    show_tips = str2num(bool_pair, s.t[1], show_tips);
		else {
		    ERR("menu: unknown var %s\n", s.t[0]);
		}
	    } else {
		ERR("menu: illegal in this context %s\n", s.str);
		goto error;
	    }
        }
    }

    GtkRecentManager * rm = gtk_recent_manager_get_default();

    GtkWidget      *recent_menu;
    GtkWidget      *menu_item;

    menu_item = gtk_image_menu_item_new_with_label(_("Recent Documents"));
    recent_menu = gtk_recent_chooser_menu_new_for_manager(rm);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), recent_menu);

    GdkPixbuf * pixbuf = lxpanel_load_icon("document-open-recent", m->iconsize, m->iconsize, FALSE);
    if (pixbuf)
    {
        GtkWidget* img = gtk_image_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM(menu_item), img);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_widget_show_all(menu_item);

    /*
        Ok, Gtk+ has a couple of bugs here.

        First, gtk_recent_chooser_set_show_tips() has no effect
        with GtkRecentChooserMenu. So we use a custom filter to
        do the things.

        Second, get_is_recent_filtered() in gtkrecentchooserutils.c
        nulls filter_info.uri field,
        if GTK_RECENT_FILTER_DISPLAY_NAME flag not supplied.
    */
    GtkRecentFilter * filter = gtk_recent_filter_new();
    gtk_recent_filter_add_custom(filter, GTK_RECENT_FILTER_URI | GTK_RECENT_FILTER_DISPLAY_NAME/* | GTK_RECENT_FILTER_APPLICATION*/,
        recent_documents_filter, NULL, NULL);
    gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER(recent_menu), filter);

    gtk_recent_chooser_set_show_private(GTK_RECENT_CHOOSER(recent_menu), show_private);
    gtk_recent_chooser_set_local_only(GTK_RECENT_CHOOSER(recent_menu), local_only);
    gtk_recent_chooser_set_show_not_found(GTK_RECENT_CHOOSER(recent_menu), FALSE); // XXX: Seems not working.
    gtk_recent_chooser_set_show_tips(GTK_RECENT_CHOOSER(recent_menu), show_tips);
    gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER(recent_menu), GTK_RECENT_SORT_MRU);
    gtk_recent_chooser_set_limit(GTK_RECENT_CHOOSER(recent_menu), limit);

    g_signal_connect(G_OBJECT(recent_menu), "item-activated", G_CALLBACK(recent_documents_activate_cb), p);
    
    error: ;
}

static void
read_system_menu(GtkMenu* menu, Plugin *p, char** fp)
{
    line s;
    menup *m = PRIV(p);

    if (m->menu_cache == NULL)
    {
        guint32 flags;
        m->menu_cache = panel_menu_cache_new(&flags);
        if (m->menu_cache == NULL)
        {
            ERR("error loading applications menu");
            return;
        }
        m->visibility_flags = flags;
        m->reload_notify = menu_cache_add_reload_notify(m->menu_cache, (MenuCacheReloadNotify) on_reload_menu, m);
    }

    if( fp )
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
            ERR("menu: error - system can not have parameters\n");
            return;
        }
    }

    sys_menu_insert_items( m, menu, -1 );
    plugin_set_has_system_menu(p, TRUE);
}

static void
read_include(Plugin *p, char **fp)
{
    ENTER;
#if 0
    gchar *name;
    line s;
    menup *m = PRIV(p);
    /* FIXME: this is disabled */
    ENTER;
    name = NULL;
    if( fp )
    {
        while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
            if (s.type == LINE_VAR) {
                if (!g_ascii_strcasecmp(s.t[0], "name"))
                    name = expand_tilda(s.t[1]);
                else  {
                    ERR( "menu/include: unknown var %s\n", s.t[0]);
                    RET();
                }
            }
        }
    }
    if ((fp = fopen(name, "r"))) {
        LOG(LOG_INFO, "Including %s\n", name);
        m->files = g_slist_prepend(m->files, fp);
        p->fp = fp;
    } else {
        ERR("Can't include %s\n", name);
    }
    if (name) g_free(name);
#endif
    RET();
}

static GtkWidget *
read_submenu(Plugin *p, char** fp, gboolean as_item)
{
    line s;
    GtkWidget *mi, *menu;
    gchar *name, *fname;
    menup *m = PRIV(p);
    GdkColor color={0, 0, 36 * 0xffff / 0xff, 96 * 0xffff / 0xff};

    ENTER;

    menu = gtk_menu_new ();
    gtk_container_set_border_width(GTK_CONTAINER(menu), 0);

    fname = NULL;
    name = NULL;
    while (lxpanel_get_line(fp, &s) != LINE_BLOCK_END) {
        if (s.type == LINE_BLOCK_START) {
            mi = NULL;
            if (!g_ascii_strcasecmp(s.t[0], "item")) {
                mi = read_item(p, fp);
            } else if (!g_ascii_strcasecmp(s.t[0], "separator")) {
                mi = read_separator(p, fp);
            } else if (!g_ascii_strcasecmp(s.t[0], "system")) {
                read_system_menu(GTK_MENU(menu), p, fp); /* add system menu items */
                continue;
            } else if (!g_ascii_strcasecmp(s.t[0], "recentdocuments")) {
                read_recent_documents_menu(GTK_MENU(menu), p, fp);
                continue;
            } else if (!g_ascii_strcasecmp(s.t[0], "menu")) {
                mi = read_submenu(p, fp, TRUE);
            } else if (!g_ascii_strcasecmp(s.t[0], "include")) {
                read_include(p, fp);
                continue;
            } else {
                ERR("menu: unknown block %s\n", s.t[0]);
                goto error;
            }
            if (!mi) {
                ERR("menu: can't create menu item\n");
                goto error;
            }
            gtk_widget_show(mi);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), mi);
        } else if (s.type == LINE_VAR) {
            m->config_start = *fp;
            if (!g_ascii_strcasecmp(s.t[0], "image"))
                fname = expand_tilda(s.t[1]);
            else if (!g_ascii_strcasecmp(s.t[0], "name"))
                name = g_strdup(s.t[1]);
        /* FIXME: tintcolor will not be saved.  */
            else if (!g_ascii_strcasecmp(s.t[0], "tintcolor"))
                gdk_color_parse( s.t[1], &color);
            else {
                ERR("menu: unknown var %s\n", s.t[0]);
            }
        } else if (s.type == LINE_NONE) {
            if (m->files) {
                /*
                  fclose(p->fp);
                  p->fp = m->files->data;
                */
                m->files = g_slist_delete_link(m->files, m->files);
            }
        }  else {
            ERR("menu: illegal in this context %s\n", s.str);
            goto error;
        }
    }
    if (as_item) {
        mi = gtk_image_menu_item_new_with_label(name);
        if (fname) {
            GtkWidget *img;
            img = _gtk_image_new_from_file_scaled(fname, m->iconsize, m->iconsize, TRUE, TRUE);
            gtk_widget_show(img);
            gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi), img);
        }
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (mi), menu);
    } else {
        m->fname = fname ? g_strdup(fname) : g_strdup( DEFAULT_MENU_ICON );
        m->caption = name ? g_strdup(name) : NULL;
        mi = make_button(p, fname, name, &color, menu);
        RET(mi);
    }

    g_free(fname);
    g_free(name);
    RET(mi);

 error:
    // FIXME: we need to recursivly destroy all child menus and their items
    gtk_widget_destroy(menu);
    g_free(fname);
    g_free(name);
    RET(NULL);
}

static int
menu_constructor(Plugin *p, char **fp)
{
    char *start;
    menup *m;
    static char default_config[] =
        "system {\n"
        "}\n"
        "separator {\n"
        "}\n"
        "recentdocuments {\n"
            "showprivate=0\n"
            "limit=20\n"
            "localonly=0\n"
            "showtips=1\n"
        "}\n"
        "separator {\n"
        "}\n"
        "item {\n"
            "command=run\n"
        "}\n"
        "separator {\n"
        "}\n"
        "item {\n"
            "image=gnome-logout\n"
            "command=logout\n"
        "}\n"
        "image=" DEFAULT_MENU_ICON "\n"
        "}\n";
    char *config_default = default_config;
    int iw, ih;

    m = g_new0(menup, 1);
    g_return_val_if_fail(m != NULL, 0);
    m->fname = NULL;
    m->caption = NULL;

    plugin_set_priv(p, m);

    gtk_icon_size_lookup( GTK_ICON_SIZE_MENU, &iw, &ih );
    m->iconsize = MAX(iw, ih);

    m->box = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(m->box), 0);

    if( ! fp )
        fp = &config_default;

    m->config_start = start = *fp;
    if (!read_submenu(p, fp, FALSE)) {
        ERR("menu: plugin init failed\n");
        return 0;
    }
    m->config_end = *fp - 1;
    while( *m->config_end != '}' && m->config_end > m->config_start ) {
        --m->config_end;
    }
    if( *m->config_end == '}' )
        --m->config_end;

    m->config_data = g_strndup( start, (m->config_end - start) );

    plugin_set_widget(p, m->box);

    RET(1);

}

static void save_config( Plugin* p, FILE* fp )
{
    menup* menu = PRIV(p);
    int level = 0;
    lxpanel_put_str( fp, "name", menu->caption );
    lxpanel_put_str( fp, "image", menu->fname );
    if( menu->config_data ) {
        char** lines = g_strsplit( menu->config_data, "\n", 0 );
        char** line;
        for( line = lines; *line; ++line ) {
            g_strstrip( *line );
            if( **line )
            {
                if( level == 0 )
                {
                    /* skip image and caption since we already save these two items */
                    if( g_str_has_prefix(*line, "image") || g_str_has_prefix(*line, "name") )
                        continue;
                }
                g_strchomp(*line); /* remove trailing spaces */
                if( g_str_has_suffix( *line, "{" ) )
                    ++level;
                else if( g_str_has_suffix( *line, "}" ) )
                    --level;
                lxpanel_put_line( fp, *line );
            }
        }
        g_strfreev( lines );
    }
}

static void apply_config(Plugin* p)
{
    menup* m = PRIV(p);
    if( m->fname )
        fb_button_set_from_file( m->img, m->fname, -1, plugin_get_icon_size(p) );
    fb_button_set_label_text( m->img, m->caption);
    fb_button_set_orientation(m->img, plugin_get_orientation(p));
}

static void menu_config( Plugin *p, GtkWindow* parent )
{
    GtkWidget* dlg;
    menup* menu = PRIV(p);
    dlg = create_generic_config_dlg(
        _(plugin_class(p)->name),
        GTK_WIDGET(parent),
        (GSourceFunc) apply_config, (gpointer) p,
        "", 0, (GType)CONF_TYPE_BEGIN_TABLE,
        _("Icon"), &menu->fname, (GType)CONF_TYPE_FILE_ENTRY,
        _("Caption"), &menu->caption, (GType)CONF_TYPE_STR,
        NULL);

    if (dlg)
        gtk_window_present( GTK_WINDOW(dlg) );
}

/* Callback when panel configuration changes. */
static void menu_panel_configuration_changed(Plugin * p)
{
    apply_config(p);
}

PluginClass menu_plugin_class = {

    PLUGINCLASS_VERSIONING,

    type : "menu",
    name : N_("Menu"),
    version: "2.0",
    description : N_("Application Menu"),

    constructor : menu_constructor,
    destructor  : menu_destructor,
    config : menu_config,
    save : save_config,
    panel_configuration_changed : menu_panel_configuration_changed,
    open_system_menu : menu_open_system_menu
};

