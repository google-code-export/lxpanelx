/**
 * Copyright (c) 2011 Vadim Ushakov
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

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <string.h>

#include "Xsupport.h"
#include "pixbuf-stuff.h"
#include "dbg.h"


/* X11 data types */
Atom a_UTF8_STRING;
Atom a_XROOTPMAP_ID;

/* old WM spec */
Atom a_WM_STATE;
Atom a_WM_CLASS;
Atom a_WM_DELETE_WINDOW;
Atom a_WM_PROTOCOLS;

/* new NET spec */

Atom a_NET_WORKAREA;
Atom a_NET_CLIENT_LIST;
Atom a_NET_CLIENT_LIST_STACKING;
Atom a_NET_NUMBER_OF_DESKTOPS;
Atom a_NET_CURRENT_DESKTOP;
Atom a_NET_DESKTOP_VIEWPORT;
Atom a_NET_DESKTOP_NAMES;
Atom a_NET_ACTIVE_WINDOW;
Atom a_NET_CLOSE_WINDOW;
Atom a_NET_SHOWING_DESKTOP;
Atom a_NET_SUPPORTED;

Atom a_NET_WM_STATE;
Atom a_NET_WM_STATE_MODAL;
Atom a_NET_WM_STATE_STICKY;
Atom a_NET_WM_STATE_MAXIMIZED_VERT;
Atom a_NET_WM_STATE_MAXIMIZED_HORZ;
Atom a_NET_WM_STATE_SHADED;
Atom a_NET_WM_STATE_SKIP_TASKBAR;
Atom a_NET_WM_STATE_SKIP_PAGER;
Atom a_NET_WM_STATE_HIDDEN;
Atom a_NET_WM_STATE_FULLSCREEN;
Atom a_NET_WM_STATE_ABOVE;
Atom a_NET_WM_STATE_BELOW;
Atom a_NET_WM_STATE_DEMANDS_ATTENTION;

Atom a_OB_WM_STATE_UNDECORATED;

Atom a_NET_WM_WINDOW_TYPE;
Atom a_NET_WM_WINDOW_TYPE_DESKTOP;
Atom a_NET_WM_WINDOW_TYPE_DOCK;
Atom a_NET_WM_WINDOW_TYPE_TOOLBAR;
Atom a_NET_WM_WINDOW_TYPE_MENU;
Atom a_NET_WM_WINDOW_TYPE_UTILITY;
Atom a_NET_WM_WINDOW_TYPE_SPLASH;
Atom a_NET_WM_WINDOW_TYPE_DIALOG;
Atom a_NET_WM_WINDOW_TYPE_NORMAL;
Atom a_NET_WM_DESKTOP;
Atom a_NET_WM_PID;
Atom a_NET_WM_NAME;
Atom a_NET_WM_VISIBLE_NAME;
Atom a_NET_WM_STRUT;
Atom a_NET_WM_STRUT_PARTIAL;
Atom a_NET_WM_ICON;
Atom a_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR;

/* SYSTEM TRAY spec */
Atom a_NET_SYSTEM_TRAY_OPCODE;
Atom a_NET_SYSTEM_TRAY_MESSAGE_DATA;
Atom a_NET_SYSTEM_TRAY_ORIENTATION;
Atom a_MANAGER;

Atom a_MOTIF_WM_HINTS;

Atom a_LXPANEL_CMD; /* for private client message */
Atom a_LXPANEL_TEXT_CMD;

/* if current window manager is EWMH conforming. */
gboolean is_ewmh_supported;

enum{
    I_UTF8_STRING,
    I_XROOTPMAP_ID,
    I_WM_STATE,
    I_WM_CLASS,
    I_WM_DELETE_WINDOW,
    I_WM_PROTOCOLS,

    I_NET_WORKAREA,
    I_NET_CLIENT_LIST,
    I_NET_CLIENT_LIST_STACKING,
    I_NET_NUMBER_OF_DESKTOPS,
    I_NET_CURRENT_DESKTOP,
    I_NET_DESKTOP_VIEWPORT,
    I_NET_DESKTOP_NAMES,
    I_NET_ACTIVE_WINDOW,
    I_NET_SHOWING_DESKTOP,
    I_NET_SUPPORTED,

    I_NET_WM_STATE,
    I_NET_WM_STATE_MODAL,
    I_NET_WM_STATE_STICKY,
    I_NET_WM_STATE_MAXIMIZED_VERT,
    I_NET_WM_STATE_MAXIMIZED_HORZ,
    I_NET_WM_STATE_SHADED,
    I_NET_WM_STATE_SKIP_TASKBAR,
    I_NET_WM_STATE_SKIP_PAGER,
    I_NET_WM_STATE_HIDDEN,
    I_NET_WM_STATE_FULLSCREEN,
    I_NET_WM_STATE_ABOVE,
    I_NET_WM_STATE_BELOW,
    I_NET_WM_STATE_DEMANDS_ATTENTION,

    I_OB_WM_STATE_UNDECORATED,

    I_NET_WM_WINDOW_TYPE,
    I_NET_WM_WINDOW_TYPE_DESKTOP,
    I_NET_WM_WINDOW_TYPE_DOCK,
    I_NET_WM_WINDOW_TYPE_TOOLBAR,
    I_NET_WM_WINDOW_TYPE_MENU,
    I_NET_WM_WINDOW_TYPE_UTILITY,
    I_NET_WM_WINDOW_TYPE_SPLASH,
    I_NET_WM_WINDOW_TYPE_DIALOG,
    I_NET_WM_WINDOW_TYPE_NORMAL,
    I_NET_WM_DESKTOP,
    I_NET_WM_PID,
    I_NET_WM_NAME,
    I_NET_WM_VISIBLE_NAME,
    I_NET_WM_STRUT,
    I_NET_WM_STRUT_PARTIAL,
    I_NET_WM_ICON,
    I_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR,

    I_NET_SYSTEM_TRAY_OPCODE,
    I_NET_SYSTEM_TRAY_MESSAGE_DATA,
    I_NET_SYSTEM_TRAY_ORIENTATION,
    I_MANAGER,

    I_MOTIF_WM_HINTS,

    I_LXPANEL_CMD,
    I_LXPANEL_TEXT_CMD,
    N_ATOMS
};


void resolve_atoms()
{
    static const char* atom_names[ N_ATOMS ];

    atom_names[ I_UTF8_STRING ] = "UTF8_STRING";
    atom_names[ I_XROOTPMAP_ID ] = "_XROOTPMAP_ID";
    atom_names[ I_WM_STATE ] = "WM_STATE";
    atom_names[ I_WM_CLASS ] = "WM_CLASS";
    atom_names[ I_WM_DELETE_WINDOW ] = "WM_DELETE_WINDOW";
    atom_names[ I_WM_PROTOCOLS ] = "WM_PROTOCOLS";
    atom_names[ I_NET_WORKAREA ] = "_NET_WORKAREA";
    atom_names[ I_NET_CLIENT_LIST ] = "_NET_CLIENT_LIST";
    atom_names[ I_NET_CLIENT_LIST_STACKING ] = "_NET_CLIENT_LIST_STACKING";
    atom_names[ I_NET_NUMBER_OF_DESKTOPS ] = "_NET_NUMBER_OF_DESKTOPS";
    atom_names[ I_NET_CURRENT_DESKTOP ] = "_NET_CURRENT_DESKTOP";
    atom_names[ I_NET_DESKTOP_VIEWPORT ] = "_NET_DESKTOP_VIEWPORT";
    atom_names[ I_NET_DESKTOP_NAMES ] = "_NET_DESKTOP_NAMES";
    atom_names[ I_NET_ACTIVE_WINDOW ] = "_NET_ACTIVE_WINDOW";
    atom_names[ I_NET_SHOWING_DESKTOP ] = "_NET_SHOWING_DESKTOP";
    atom_names[ I_NET_SUPPORTED ] = "_NET_SUPPORTED";
    atom_names[ I_NET_WM_DESKTOP ] = "_NET_WM_DESKTOP";

    atom_names[ I_NET_WM_STATE ] = "_NET_WM_STATE";
    atom_names[ I_NET_WM_STATE_MODAL ] = "_NET_WM_STATE_MODAL";
    atom_names[ I_NET_WM_STATE_STICKY ] = "_NET_WM_STATE_STICKY";
    atom_names[ I_NET_WM_STATE_MAXIMIZED_VERT ] = "_NET_WM_STATE_MAXIMIZED_VERT";
    atom_names[ I_NET_WM_STATE_MAXIMIZED_HORZ ] = "_NET_WM_STATE_MAXIMIZED_HORZ";
    atom_names[ I_NET_WM_STATE_SHADED ] = "_NET_WM_STATE_SHADED";
    atom_names[ I_NET_WM_STATE_SKIP_TASKBAR ] = "_NET_WM_STATE_SKIP_TASKBAR";
    atom_names[ I_NET_WM_STATE_SKIP_PAGER ] = "_NET_WM_STATE_SKIP_PAGER";
    atom_names[ I_NET_WM_STATE_HIDDEN ] = "_NET_WM_STATE_HIDDEN";
    atom_names[ I_NET_WM_STATE_FULLSCREEN ] = "_NET_WM_STATE_FULLSCREEN";
    atom_names[ I_NET_WM_STATE_ABOVE ] = "_NET_WM_STATE_ABOVE";
    atom_names[ I_NET_WM_STATE_BELOW ] = "_NET_WM_STATE_BELOW";
    atom_names[ I_NET_WM_STATE_DEMANDS_ATTENTION ] = "_NET_WM_STATE_DEMANDS_ATTENTION";

    atom_names[ I_OB_WM_STATE_UNDECORATED ] = "_OB_WM_STATE_UNDECORATED";

    atom_names[ I_NET_WM_WINDOW_TYPE ] = "_NET_WM_WINDOW_TYPE";
    atom_names[ I_NET_WM_WINDOW_TYPE_DESKTOP ] = "_NET_WM_WINDOW_TYPE_DESKTOP";
    atom_names[ I_NET_WM_WINDOW_TYPE_DOCK ] = "_NET_WM_WINDOW_TYPE_DOCK";
    atom_names[ I_NET_WM_WINDOW_TYPE_TOOLBAR ] = "_NET_WM_WINDOW_TYPE_TOOLBAR";
    atom_names[ I_NET_WM_WINDOW_TYPE_MENU ] = "_NET_WM_WINDOW_TYPE_MENU";
    atom_names[ I_NET_WM_WINDOW_TYPE_UTILITY ] = "_NET_WM_WINDOW_TYPE_UTILITY";
    atom_names[ I_NET_WM_WINDOW_TYPE_SPLASH ] = "_NET_WM_WINDOW_TYPE_SPLASH";
    atom_names[ I_NET_WM_WINDOW_TYPE_DIALOG ] = "_NET_WM_WINDOW_TYPE_DIALOG";
    atom_names[ I_NET_WM_WINDOW_TYPE_NORMAL ] = "_NET_WM_WINDOW_TYPE_NORMAL";
    atom_names[ I_NET_WM_DESKTOP ] = "_NET_WM_DESKTOP";
    atom_names[ I_NET_WM_PID ] = "_NET_WM_PID";
    atom_names[ I_NET_WM_NAME ] = "_NET_WM_NAME";
    atom_names[ I_NET_WM_VISIBLE_NAME ] = "_NET_WM_VISIBLE_NAME";
    atom_names[ I_NET_WM_STRUT ] = "_NET_WM_STRUT";
    atom_names[ I_NET_WM_STRUT_PARTIAL ] = "_NET_WM_STRUT_PARTIAL";
    atom_names[ I_NET_WM_ICON ] = "_NET_WM_ICON";
    atom_names[ I_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR ] = "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR";

    atom_names[ I_NET_SYSTEM_TRAY_OPCODE ] = "_NET_SYSTEM_TRAY_OPCODE";
    atom_names[ I_NET_SYSTEM_TRAY_MESSAGE_DATA ] = "_NET_SYSTEM_TRAY_MESSAGE_DATA";
    atom_names[ I_NET_SYSTEM_TRAY_ORIENTATION ] = "_NET_SYSTEM_TRAY_ORIENTATION";
    atom_names[ I_MANAGER ] = "MANAGER";

    atom_names[ I_MOTIF_WM_HINTS ] = "_MOTIF_WM_HINTS";

    atom_names[ I_LXPANEL_CMD ] = "_LXPANEL_CMD";
    atom_names[ I_LXPANEL_TEXT_CMD ] = "_LXPANEL_TEXT_CMD";

    Atom atoms[ N_ATOMS ];

    ENTER;

#ifndef DEBUG
    if( !  XInternAtoms( GDK_DISPLAY(), (char**)atom_names,
            N_ATOMS, False, atoms ) )
    {
        g_warning( "Error: unable to return Atoms" );
        return;
    }
#else
    int i;
    for (i = 0; i < N_ATOMS; i++) {
        DBG("Registering atom %s\n", atom_names[i]);
        if( !  XInternAtoms( GDK_DISPLAY(), ((char**)atom_names) + i,
               1, False, atoms + i) )
        {
            g_warning( "Error: unable to return Atoms" );
            return;
        }
    }
#endif

    a_UTF8_STRING = atoms[ I_UTF8_STRING ];
    a_XROOTPMAP_ID = atoms[ I_XROOTPMAP_ID ];
    a_WM_STATE = atoms[ I_WM_STATE ];
    a_WM_CLASS = atoms[ I_WM_CLASS ];
    a_WM_DELETE_WINDOW = atoms[ I_WM_DELETE_WINDOW ];
    a_WM_PROTOCOLS = atoms[ I_WM_PROTOCOLS ];

    a_NET_WORKAREA = atoms[ I_NET_WORKAREA ];
    a_NET_CLIENT_LIST = atoms[ I_NET_CLIENT_LIST ];
    a_NET_CLIENT_LIST_STACKING = atoms[ I_NET_CLIENT_LIST_STACKING ];
    a_NET_NUMBER_OF_DESKTOPS = atoms[ I_NET_NUMBER_OF_DESKTOPS ];
    a_NET_CURRENT_DESKTOP = atoms[ I_NET_CURRENT_DESKTOP ];
    a_NET_DESKTOP_VIEWPORT = atoms[ I_NET_DESKTOP_VIEWPORT ];
    a_NET_DESKTOP_NAMES = atoms[ I_NET_DESKTOP_NAMES ];
    a_NET_ACTIVE_WINDOW = atoms[ I_NET_ACTIVE_WINDOW ];
    a_NET_SHOWING_DESKTOP = atoms[ I_NET_SHOWING_DESKTOP ];
    a_NET_SUPPORTED = atoms[ I_NET_SUPPORTED ];

    a_NET_WM_STATE = atoms[ I_NET_WM_STATE ];
    a_NET_WM_STATE_MODAL = atoms[ I_NET_WM_STATE_MODAL ];
    a_NET_WM_STATE_STICKY = atoms[ I_NET_WM_STATE_STICKY ];
    a_NET_WM_STATE_MAXIMIZED_VERT = atoms[ I_NET_WM_STATE_MAXIMIZED_VERT ];
    a_NET_WM_STATE_MAXIMIZED_HORZ = atoms[ I_NET_WM_STATE_MAXIMIZED_HORZ ];
    a_NET_WM_STATE_SHADED = atoms[ I_NET_WM_STATE_SHADED ];
    a_NET_WM_STATE_SKIP_TASKBAR = atoms[ I_NET_WM_STATE_SKIP_TASKBAR ];
    a_NET_WM_STATE_SKIP_PAGER = atoms[ I_NET_WM_STATE_SKIP_PAGER ];
    a_NET_WM_STATE_HIDDEN = atoms[ I_NET_WM_STATE_HIDDEN ];
    a_NET_WM_STATE_FULLSCREEN = atoms[ I_NET_WM_STATE_FULLSCREEN ];
    a_NET_WM_STATE_ABOVE = atoms[ I_NET_WM_STATE_ABOVE ];
    a_NET_WM_STATE_BELOW = atoms[ I_NET_WM_STATE_BELOW ];
    a_NET_WM_STATE_DEMANDS_ATTENTION = atoms[ I_NET_WM_STATE_DEMANDS_ATTENTION ];

    a_OB_WM_STATE_UNDECORATED = atoms[ I_OB_WM_STATE_UNDECORATED ];

    a_NET_WM_WINDOW_TYPE = atoms[ I_NET_WM_WINDOW_TYPE ];
    a_NET_WM_WINDOW_TYPE_DESKTOP = atoms[ I_NET_WM_WINDOW_TYPE_DESKTOP ];
    a_NET_WM_WINDOW_TYPE_DOCK = atoms[ I_NET_WM_WINDOW_TYPE_DOCK ];
    a_NET_WM_WINDOW_TYPE_TOOLBAR = atoms[ I_NET_WM_WINDOW_TYPE_TOOLBAR ];
    a_NET_WM_WINDOW_TYPE_MENU = atoms[ I_NET_WM_WINDOW_TYPE_MENU ];
    a_NET_WM_WINDOW_TYPE_UTILITY = atoms[ I_NET_WM_WINDOW_TYPE_UTILITY ];
    a_NET_WM_WINDOW_TYPE_SPLASH = atoms[ I_NET_WM_WINDOW_TYPE_SPLASH ];
    a_NET_WM_WINDOW_TYPE_DIALOG = atoms[ I_NET_WM_WINDOW_TYPE_DIALOG ];
    a_NET_WM_WINDOW_TYPE_NORMAL = atoms[ I_NET_WM_WINDOW_TYPE_NORMAL ];
    a_NET_WM_DESKTOP = atoms[ I_NET_WM_DESKTOP ];
    a_NET_WM_PID = atoms[ I_NET_WM_PID ];
    a_NET_WM_NAME = atoms[ I_NET_WM_NAME ];
    a_NET_WM_VISIBLE_NAME = atoms[ I_NET_WM_VISIBLE_NAME ];
    a_NET_WM_STRUT = atoms[ I_NET_WM_STRUT ];
    a_NET_WM_STRUT_PARTIAL = atoms[ I_NET_WM_STRUT_PARTIAL ];
    a_NET_WM_ICON = atoms[ I_NET_WM_ICON ];
    a_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR = atoms[ I_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR ];

    a_NET_SYSTEM_TRAY_OPCODE = atoms[ I_NET_SYSTEM_TRAY_OPCODE ];
    a_NET_SYSTEM_TRAY_MESSAGE_DATA = atoms [ I_NET_SYSTEM_TRAY_MESSAGE_DATA ];
    a_NET_SYSTEM_TRAY_ORIENTATION = atoms[ I_NET_SYSTEM_TRAY_ORIENTATION ];
    a_MANAGER = atoms[ I_MANAGER ];

    a_MOTIF_WM_HINTS = atoms[ I_MOTIF_WM_HINTS ];

    a_LXPANEL_CMD = atoms[ I_LXPANEL_CMD ];
    a_LXPANEL_TEXT_CMD = atoms[ I_LXPANEL_TEXT_CMD ];

    RET();
}


void
Xclimsg(Window win, Atom type, long l0, long l1, long l2, long l3, long l4)
{
    XClientMessageEvent xev;
    xev.type = ClientMessage;
    xev.window = win;
    xev.message_type = type;
    xev.format = 32;
    xev.data.l[0] = l0;
    xev.data.l[1] = l1;
    xev.data.l[2] = l2;
    xev.data.l[3] = l3;
    xev.data.l[4] = l4;
    XSendEvent(GDK_DISPLAY(), GDK_ROOT_WINDOW(), False,
          (SubstructureNotifyMask | SubstructureRedirectMask),
          (XEvent *) &xev);
}

void
Xclimsgwm(Window win, Atom type, Atom arg)
{
    XClientMessageEvent xev;

    xev.type = ClientMessage;
    xev.window = win;
    xev.message_type = type;
    xev.format = 32;
    xev.data.l[0] = arg;
    xev.data.l[1] = GDK_CURRENT_TIME;
    XSendEvent(GDK_DISPLAY(), win, False, 0L, (XEvent *) &xev);
}


void *
get_utf8_property(Window win, Atom atom)
{
    Atom type;
    int format;
    gulong nitems;
    gulong bytes_after;
    gchar *val, *retval;
    int result;
    guchar *tmp = NULL;

    type = None;
    retval = NULL;
    result = XGetWindowProperty (GDK_DISPLAY(), win, atom, 0, G_MAXLONG, False,
          a_UTF8_STRING, &type, &format, &nitems,
          &bytes_after, &tmp);
    if (result != Success || type == None)
        return NULL;
    val = (gchar *) tmp;
    if (val) {
        if (type == a_UTF8_STRING && format == 8 && nitems != 0)
            retval = g_strndup (val, nitems);
        XFree (val);
    }
    return retval;

}

char **
get_utf8_property_list(Window win, Atom atom, int *count)
{
    Atom type;
    int format, i;
    gulong nitems;
    gulong bytes_after;
    gchar *s, **retval = NULL;
    int result;
    guchar *tmp = NULL;

    *count = 0;
    result = XGetWindowProperty(GDK_DISPLAY(), win, atom, 0, G_MAXLONG, False,
          a_UTF8_STRING, &type, &format, &nitems,
          &bytes_after, &tmp);
    if (result != Success || type != a_UTF8_STRING || tmp == NULL)
        return NULL;

    if (nitems) {
        gchar *val = (gchar *) tmp;
        DBG("res=%d(%d) nitems=%d val=%s\n", result, Success, nitems, val);
        for (i = 0; i < nitems; i++) {
            if (!val[i])
                (*count)++;
        }
        retval = g_new0 (char*, *count + 2);
        for (i = 0, s = val; i < *count; i++, s = s +  strlen (s) + 1) {
            retval[i] = g_strdup(s);
        }
        if (val[nitems-1]) {
            result = nitems - (s - val);
            DBG("val does not ends by 0, moving last %d bytes\n", result);
            g_memmove(s - 1, s, result);
            val[nitems-1] = 0;
            DBG("s=%s\n", s -1);
            retval[i] = g_strdup(s - 1);
            (*count)++;
        }
    }
    XFree (tmp);

    return retval;

}

void *
get_xaproperty (Window win, Atom prop, Atom type, int *nitems)
{
    Atom type_ret;
    int format_ret;
    unsigned long items_ret;
    unsigned long after_ret;
    unsigned char *prop_data;

    ENTER;
    prop_data = NULL;
    if (XGetWindowProperty (GDK_DISPLAY(), win, prop, 0, 0x7fffffff, False,
              type, &type_ret, &format_ret, &items_ret,
              &after_ret, &prop_data) != Success)
    {
        if( G_UNLIKELY(prop_data) )
            XFree( prop_data );
        if( nitems )
            *nitems = 0;
        RET(NULL);
    }
    if (nitems)
        *nitems = items_ret;
    RET(prop_data);
}

static char*
text_property_to_utf8 (const XTextProperty *prop)
{
  char **list;
  int count;
  char *retval;

  ENTER;
  list = NULL;
  count = gdk_text_property_to_utf8_list (gdk_x11_xatom_to_atom (prop->encoding),
                                          prop->format,
                                          prop->value,
                                          prop->nitems,
                                          &list);

  DBG("count=%d\n", count);
  if (count == 0)
    return NULL;

  retval = list[0];
  list[0] = g_strdup (""); /* something to free */

  g_strfreev (list);

  RET(retval);
}

char *
get_textproperty(Window win, Atom atom)
{
    XTextProperty text_prop;
    char *retval;

    ENTER;
    if (XGetTextProperty(GDK_DISPLAY(), win, &text_prop, atom)) {
        DBG("format=%d enc=%d nitems=%d value=%s   \n",
              text_prop.format,
              text_prop.encoding,
              text_prop.nitems,
              text_prop.value);
        retval = text_property_to_utf8 (&text_prop);
        if (text_prop.nitems > 0)
            XFree (text_prop.value);
        RET(retval);

    }
    RET(NULL);
}


int
get_net_number_of_desktops()
{
    int desknum;
    guint32 *data;

    ENTER;
    data = get_xaproperty (GDK_ROOT_WINDOW(), a_NET_NUMBER_OF_DESKTOPS,
          XA_CARDINAL, 0);
    if (!data)
        RET(0);

    desknum = *data;
    XFree (data);
    RET(desknum);
}


int
get_net_current_desktop ()
{
    int desk;
    guint32 *data;

    ENTER;
    data = get_xaproperty (GDK_ROOT_WINDOW(), a_NET_CURRENT_DESKTOP, XA_CARDINAL, 0);
    if (!data)
        RET(0);

    desk = *data;
    XFree (data);
    RET(desk);
}

int
get_net_wm_desktop(Window win)
{
    int desk = 0;
    guint32 *data;

    ENTER;
    data = get_xaproperty (win, a_NET_WM_DESKTOP, XA_CARDINAL, 0);
    if (data) {
        desk = *data;
        XFree (data);
    }
    RET(desk);
}

void
set_net_wm_desktop(Window win, int num)
{
    Xclimsg(win, a_NET_WM_DESKTOP, num, 0, 0, 0, 0);
}


GPid
get_net_wm_pid(Window win)
{
    GPid pid = 0;
    guint32 *data;

    ENTER;
    data = get_xaproperty (win, a_NET_WM_PID, XA_CARDINAL, 0);
    if (data) {
        pid = *data;
        XFree (data);
    }
    RET(pid);
}

void
get_net_wm_state(Window win, NetWMState *nws)
{
    Atom *state;
    int num3;

    ENTER;

    bzero(nws, sizeof(nws));
    if (!(state = get_xaproperty(win, a_NET_WM_STATE, XA_ATOM, &num3)))
        RET();

    DBG( "%x: netwm state = { ", (unsigned int)win);
    while (--num3 >= 0) {

        if (state[num3] == a_NET_WM_STATE_SKIP_PAGER) {
            DBG("NET_WM_STATE_SKIP_PAGER ");
            nws->skip_pager = 1;
        } else if (state[num3] == a_NET_WM_STATE_SKIP_TASKBAR) {
            DBG( "NET_WM_STATE_SKIP_TASKBAR ");
            nws->skip_taskbar = 1;
        } else if (state[num3] == a_NET_WM_STATE_STICKY) {
            DBG( "NET_WM_STATE_STICKY ");
            nws->sticky = 1;
        } else if (state[num3] == a_NET_WM_STATE_HIDDEN) {
            DBG( "NET_WM_STATE_HIDDEN ");
            nws->hidden = 1;
        } else if (state[num3] == a_NET_WM_STATE_SHADED) {
            DBG( "NET_WM_STATE_SHADED ");
            nws->shaded = 1;
        } else if (state[num3] == a_NET_WM_STATE_MODAL) {
            DBG( "NET_WM_STATE_MODAL ");
            nws->modal = 1;
        } else if (state[num3] == a_NET_WM_STATE_MAXIMIZED_VERT) {
            DBG( "NET_WM_STATE_MAXIMIZED_VERT ");
            nws->maximized_vert = 1;
        } else if (state[num3] == a_NET_WM_STATE_MAXIMIZED_HORZ) {
            DBG( "NET_WM_STATE_MAXIMIZED_HORZ ");
            nws->maximized_horz = 1;
        } else if (state[num3] == a_NET_WM_STATE_FULLSCREEN) {
            DBG( "NET_WM_STATE_FULLSCREEN; ");
            nws->fullscreen = 1;
        } else if (state[num3] == a_NET_WM_STATE_ABOVE) {
            DBG( "NET_WM_STATE_ABOVE ");
            nws->above = 1;
        } else if (state[num3] == a_NET_WM_STATE_BELOW) {
            DBG( "NET_WM_STATE_BELOW ");
            nws->below = 1;
        } else if (state[num3] == a_NET_WM_STATE_DEMANDS_ATTENTION) {
            DBG( "NET_WM_STATE_DEMANDS_ATTENTION ");
            nws->demands_attention = 1;
        } else if (state[num3] == a_OB_WM_STATE_UNDECORATED) {
            DBG( "OB_WM_STATE_UNDECORATED ");
            nws->ob_undecorated = 1;
        } else {
            DBG( "... ");
        }
    }
    XFree(state);
    DBG( "}\n");
    RET();
}

void
get_net_wm_window_type(Window win, NetWMWindowType *nwwt)
{
    Atom *state;
    int num3;


    ENTER;
    bzero(nwwt, sizeof(*nwwt));
    if (!(state = get_xaproperty(win, a_NET_WM_WINDOW_TYPE, XA_ATOM, &num3)))
        RET();

    DBG( "%x: netwm state = { ", (unsigned int)win);
    while (--num3 >= 0) {
        if (state[num3] == a_NET_WM_WINDOW_TYPE_DESKTOP) {
            DBG("NET_WM_WINDOW_TYPE_DESKTOP ");
            nwwt->desktop = 1;
        } else if (state[num3] == a_NET_WM_WINDOW_TYPE_DOCK) {
            DBG( "NET_WM_WINDOW_TYPE_DOCK ");
        nwwt->dock = 1;
    } else if (state[num3] == a_NET_WM_WINDOW_TYPE_TOOLBAR) {
            DBG( "NET_WM_WINDOW_TYPE_TOOLBAR ");
        nwwt->toolbar = 1;
        } else if (state[num3] == a_NET_WM_WINDOW_TYPE_MENU) {
            DBG( "NET_WM_WINDOW_TYPE_MENU ");
            nwwt->menu = 1;
    } else if (state[num3] == a_NET_WM_WINDOW_TYPE_UTILITY) {
            DBG( "NET_WM_WINDOW_TYPE_UTILITY ");
            nwwt->utility = 1;
    } else if (state[num3] == a_NET_WM_WINDOW_TYPE_SPLASH) {
            DBG( "NET_WM_WINDOW_TYPE_SPLASH ");
            nwwt->splash = 1;
    } else if (state[num3] == a_NET_WM_WINDOW_TYPE_DIALOG) {
            DBG( "NET_WM_WINDOW_TYPE_DIALOG ");
            nwwt->dialog = 1;
    } else if (state[num3] == a_NET_WM_WINDOW_TYPE_NORMAL) {
            DBG( "NET_WM_WINDOW_TYPE_NORMAL ");
            nwwt->normal = 1;
    } else {
        DBG( "... ");
    }
    }
    XFree(state);
    DBG( "}\n");
    RET();
}

int
get_wm_state (Window win)
{
    unsigned long *data;
    int ret = 0;

    ENTER;
    data = get_xaproperty (win, a_WM_STATE, a_WM_STATE, 0);
    if (data) {
        ret = data[0];
        XFree (data);
    }
    RET(ret);
}

typedef enum
{
    _MWM_DECOR_ALL      = 1 << 0, /*!< All decorations */
    _MWM_DECOR_BORDER   = 1 << 1, /*!< Show a border */
    _MWM_DECOR_HANDLE   = 1 << 2, /*!< Show a handle (bottom) */
    _MWM_DECOR_TITLE    = 1 << 3, /*!< Show a titlebar */
#if 0
    _MWM_DECOR_MENU     = 1 << 4, /*!< Show a menu */
#endif
    _MWM_DECOR_ICONIFY  = 1 << 5, /*!< Show an iconify button */
    _MWM_DECOR_MAXIMIZE = 1 << 6  /*!< Show a maximize button */
} MwmDecorations;

#define PROP_MOTIF_WM_HINTS_ELEMENTS 5
#define MWM_HINTS_DECORATIONS (1L << 1)
struct MwmHints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
};

void
set_decorations (Window win, gboolean decorate)
{
    struct MwmHints hints = {0,};
    hints.flags = MWM_HINTS_DECORATIONS;
    hints.decorations = decorate ? _MWM_DECOR_ALL : (_MWM_DECOR_BORDER | _MWM_DECOR_HANDLE) ;

    /* Set Motif hints, most window managers handle these */
    XChangeProperty(GDK_DISPLAY(), win,
                    a_MOTIF_WM_HINTS, 
                    a_MOTIF_WM_HINTS, 32, PropModeReplace, 
                    (unsigned char *)&hints, PROP_MOTIF_WM_HINTS_ELEMENTS);

    Xclimsg(win, a_NET_WM_STATE,
                decorate ? 0 : 1,
                a_OB_WM_STATE_UNDECORATED,
                0, 0, 0);
}

int
get_mvm_decorations(Window win)
{
    gboolean result = -1;

    struct MwmHints * hints;
    int nitems = 0;

    hints = (struct MwmHints *) get_xaproperty(win, a_MOTIF_WM_HINTS, a_MOTIF_WM_HINTS, &nitems);

    if (!hints || nitems < PROP_MOTIF_WM_HINTS_ELEMENTS)
    {
        /* nothing */
    }
    else
    {
        if (hints->flags & MWM_HINTS_DECORATIONS)
        {
            result = (hints->decorations & (_MWM_DECOR_ALL | _MWM_DECOR_TITLE)) ? 1 : 0;
        }
        else
        {
            result = 1;
        }
    }

    if (hints)
        XFree(hints);

    return result;
}

gboolean get_decorations (Window win, NetWMState * nws)
{
    if (check_net_supported(a_OB_WM_STATE_UNDECORATED))
    {
        NetWMState n;
        if (!nws)
        {
            nws = &n;
            get_net_wm_state(win, nws);
        }
        return !nws->ob_undecorated;
    }
    else
    {
        return get_mvm_decorations(win) != 0;
    }
}


static Atom * _net_supported = NULL;
static int _net_supported_nitems = 0;

void update_net_supported()
{
    ENTER;

    if (_net_supported)
    {
        XFree(_net_supported);
        _net_supported = NULL;
        _net_supported_nitems = 0;
    }
    
    _net_supported = get_xaproperty(GDK_ROOT_WINDOW(), a_NET_SUPPORTED, XA_ATOM, &_net_supported_nitems);

    RET();
}

gboolean check_net_supported(Atom atom)
{
    if (_net_supported_nitems < 1 || !_net_supported_nitems)
        return FALSE;

    int i;
    for (i = 0; i < _net_supported_nitems; i++)
    {
        if (_net_supported[i] == atom)
            return TRUE;
    }

    return FALSE;
}

#include <X11/extensions/Xcomposite.h>

gboolean is_xcomposite_available(void)
{
    static int result = -1;

    if (result < 0)
    {
	int event_base, error_base, major, minor;
	if (!XCompositeQueryExtension(GDK_DISPLAY(), &event_base, &error_base))
	{
	    result = FALSE;
	}
	else
	{
	    major = 0, minor = 2;
	    XCompositeQueryVersion(GDK_DISPLAY(), &major, &minor);
	    if (! (major > 0 || minor >= 2))
	    {
		result = FALSE;
	    }
	    else
	    {
		result = TRUE;
	    }
	}
    }

    return result;
}

/******************************************************************************/

static GdkPixbuf * get_net_wm_icon(Window task_win, int required_width, int required_height)
{
    GdkPixbuf * pixmap = NULL;
    int result = -1;

    /* Important Notes:
     * According to freedesktop.org document:
     * http://standards.freedesktop.org/wm-spec/wm-spec-1.4.html#id2552223
     * _NET_WM_ICON contains an array of 32-bit packed CARDINAL ARGB.
     * However, this is incorrect. Actually it's an array of long integers.
     * Toolkits like gtk+ use unsigned long here to store icons.
     * Besides, according to manpage of XGetWindowProperty, when returned format,
     * is 32, the property data will be stored as an array of longs
     * (which in a 64-bit application will be 64-bit values that are
     * padded in the upper 4 bytes).
     */

    /* Get the window property _NET_WM_ICON, if possible. */
    Atom type = None;
    int format;
    gulong nitems;
    gulong bytes_after;
    gulong * data = NULL;
    result = XGetWindowProperty(
        GDK_DISPLAY(),
        task_win,
        a_NET_WM_ICON,
        0, G_MAXLONG,
        False, XA_CARDINAL,
        &type, &format, &nitems, &bytes_after, (void *) &data);

    /* Inspect the result to see if it is usable.  If not, and we got data, free it. */
    if ((type != XA_CARDINAL) || (nitems <= 0))
    {
        if (data != NULL)
            XFree(data);
        result = -1;
    }

    /* If the result is usable, extract the icon from it. */
    if (result == Success)
    {
        /* Get the largest icon available, unless there is one that is the desired size. */
        /* FIXME: should we try to find an icon whose size is closest to
         * required_width and required_height to reduce unnecessary resizing? */
        gulong * pdata = data;
        gulong * pdata_end = data + nitems;
        gulong * max_icon = NULL;
        gulong max_w = 0;
        gulong max_h = 0;
        while ((pdata + 2) < pdata_end)
        {
            /* Extract the width and height. */
            gulong w = pdata[0];
            gulong h = pdata[1];
            gulong size = w * h;
            pdata += 2;

            /* Bounds check the icon. */
            if (pdata + size > pdata_end)
                break;

            /* Rare special case: the desired size is the same as icon size. */
            if ((required_width == w) && (required_height == h))
            {
                max_icon = pdata;
                max_w = w;
                max_h = h;
                break;
            }

            /* If the icon is the largest so far, capture it. */
            if ((w > max_w) && (h > max_h))
            {
                max_icon = pdata;
                max_w = w;
                max_h = h;
            }
            pdata += size;
        }

        /* If an icon was extracted, convert it to a pixbuf.
         * Its size is max_w and max_h. */
        if (max_icon != NULL)
        {
            /* Allocate enough space for the pixel data. */
            gulong len = max_w * max_h;
            guchar * pixdata = g_new(guchar, len * 4);

            /* Loop to convert the pixel data. */
            guchar * p = pixdata;
            int i;
            for (i = 0; i < len; p += 4, i += 1)
            {
                guint argb = max_icon[i];
                guint rgba = (argb << 8) | (argb >> 24);
                p[0] = rgba >> 24;
                p[1] = (rgba >> 16) & 0xff;
                p[2] = (rgba >> 8) & 0xff;
                p[3] = rgba & 0xff;
            }
            
            /* Initialize a pixmap with the pixel data. */
            pixmap = gdk_pixbuf_new_from_data(
                pixdata,
                GDK_COLORSPACE_RGB,
                TRUE, 8,	/* has_alpha, bits_per_sample */
                max_w, max_h, max_w * 4,
                (GdkPixbufDestroyNotify) g_free,
                NULL);
         }
         else
	    result = -1;

        /* Free the X property data. */
        XFree(data);
     }

    return pixmap;
}

static GdkPixbuf * get_icon_from_pixmap_mask(Pixmap xpixmap, Pixmap xmask)
{
    GdkPixbuf * pixmap = NULL;
    int result = -1;

    /* get pixmap geometry.*/
    unsigned int w, h;
    {
        Window unused_win;
        int unused;
        unsigned int unused_2;
        result = XGetGeometry(
            GDK_DISPLAY(), xpixmap,
            &unused_win, &unused, &unused, &w, &h, &unused_2, &unused_2) ? Success : -1;
    }

    /* convert it to a GDK pixbuf. */
    if (result == Success) 
    {
        pixmap = _gdk_pixbuf_get_from_pixmap(xpixmap, w, h);
        result = ((pixmap != NULL) ? Success : -1);
    }

    /* If we have success, see if the result needs to be masked.
     * Failures here are implemented as nonfatal. */
    if ((result == Success) && (xmask != None))
    {
        Window unused_win;
        int unused;
        unsigned int unused_2;
        if (XGetGeometry(
            GDK_DISPLAY(), xmask,
            &unused_win, &unused, &unused, &w, &h, &unused_2, &unused_2))
        {
            /* Convert the X mask to a GDK pixmap. */
            GdkPixbuf * mask = _gdk_pixbuf_get_from_pixmap(xmask, w, h);
            if (mask != NULL)
            {
                /* Apply the mask. */
                GdkPixbuf * masked_pixmap = _gdk_pixbuf_apply_mask(pixmap, mask);
                g_object_unref(G_OBJECT(pixmap));
                g_object_unref(G_OBJECT(mask));
                pixmap = masked_pixmap;
            }
        }
    }

    return pixmap;
}

static GdkPixbuf * get_icon_from_wm_hints(Window task_win)
{
    GdkPixbuf * pixmap = NULL;
    int result = -1;

    XWMHints * hints = XGetWMHints(GDK_DISPLAY(), task_win);
    result = (hints != NULL) ? Success : -1;
    Pixmap xpixmap = None;
    Pixmap xmask = None;

    if (result == Success)
    {
        /* WM_HINTS is available.  Extract the X pixmap and mask. */
        if ((hints->flags & IconPixmapHint))
            xpixmap = hints->icon_pixmap;
        if ((hints->flags & IconMaskHint))
            xmask = hints->icon_mask;
        XFree(hints);
        if (xpixmap != None)
        {
            result = Success;
        }
        else
            result = -1;
    }

    if (result == Success)
    {
        pixmap = get_icon_from_pixmap_mask(xpixmap, xmask);
    }

    return pixmap;
}

static GdkPixbuf * get_icon_from_kwm_win_icon(Window task_win)
{
    GdkPixbuf * pixmap = NULL;
    int result = -1;

    Pixmap xpixmap = None;
    Pixmap xmask = None;

    Atom type = None;
    int format;
    gulong nitems;
    gulong bytes_after;
    Pixmap *icons = NULL;
    Atom kwin_win_icon_atom = gdk_x11_get_xatom_by_name("KWM_WIN_ICON");
    result = XGetWindowProperty(
        GDK_DISPLAY(),
        task_win,
        kwin_win_icon_atom,
        0, G_MAXLONG,
        False, kwin_win_icon_atom,
        &type, &format, &nitems, &bytes_after, (void *) &icons);

    /* Inspect the result to see if it is usable.  If not, and we got data, free it. */
    if (type != kwin_win_icon_atom)
    {
        if (icons != NULL)
            XFree(icons);
        result = -1;
    }

    /* If the result is usable, extract the X pixmap and mask from it. */
    if (result == Success)
    {
        xpixmap = icons[0];
        xmask = icons[1];
        if (xpixmap != None)
        {
            result = Success;
        }
        else
            result = -1;
    }

    if (result == Success)
    {
        pixmap = get_icon_from_pixmap_mask(xpixmap, xmask);
    }

    return pixmap;

}

/* Get an icon from the window manager for a task, and scale it to a specified size. */
GdkPixbuf * get_wm_icon(Window task_win, int required_width, int required_height, Atom source, Atom * current_source)
{
    /* The result. */
    GdkPixbuf * pixmap = NULL;
    Atom possible_source = None;

    Atom kwin_win_icon_atom = gdk_x11_get_xatom_by_name("KWM_WIN_ICON");

    /* First, try to load icon from the `source` source. */

    Atom preferable_source = source;

    again:

    if (!pixmap && preferable_source == a_NET_WM_ICON)
    {
        pixmap = get_net_wm_icon(task_win, required_width, required_height);
        if (pixmap)
            possible_source = a_NET_WM_ICON;
    }

    if (!pixmap && preferable_source == XA_WM_HINTS)
    {
        pixmap = get_icon_from_wm_hints(task_win);
        if (pixmap)
            possible_source = XA_WM_HINTS;
    }

    if (!pixmap && preferable_source == kwin_win_icon_atom)
    {
        pixmap = get_icon_from_kwm_win_icon(task_win);
        if (pixmap)
            possible_source = kwin_win_icon_atom;
    }

    /* Second, try to load icon from the source that has succeed previous time. */

    if (!pixmap && *current_source && preferable_source != *current_source)
    {
        preferable_source = *current_source;
        goto again;
    }

    /* Third, try each source. */

    if (!pixmap)
    {
        pixmap = get_net_wm_icon(task_win, required_width, required_height);
        if (pixmap)
            possible_source = a_NET_WM_ICON;
    }

    if (!pixmap)
    {
        pixmap = get_icon_from_wm_hints(task_win);
        if (pixmap)
            possible_source = XA_WM_HINTS;
    }

    if (!pixmap)
    {
        pixmap = get_icon_from_kwm_win_icon(task_win);
        if (pixmap)
            possible_source = kwin_win_icon_atom;
    }

    if (pixmap)
        *current_source = possible_source;

    return pixmap;
}

void wm_noinput(Window w)
{
    XWMHints wmhints;
    wmhints.flags = InputHint;
    wmhints.input = 0;
    XSetWMHints (GDK_DISPLAY(), w, &wmhints);

    #define WIN_HINTS_SKIP_FOCUS      (1<<0)    /* "alt-tab" skips this win */
    guint32 val = WIN_HINTS_SKIP_FOCUS;
    XChangeProperty(GDK_DISPLAY(), w,
          XInternAtom(GDK_DISPLAY(), "_WIN_HINTS", False), XA_CARDINAL, 32,
          PropModeReplace, (unsigned char *) &val, 1);
}
