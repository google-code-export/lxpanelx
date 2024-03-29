/**
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

/* Remote controller of lxpanelx */

#include "lxpanelxctl.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Display* dpy;

static const char usage[] =
        "\nlxpanelxctl - LXPanelX Controller\n"
        "Usage: lxpanelxctl <command>\n\n"
        "Available commands:\n"
        "menu\tshow system menu\n"
        "run\tshow run dialog\n"
        "config\tshow configuration dialog\n"
        "restart\trestart lxpanelx\n"
        "exit\texit lxpanelx\n\n";
/*
static int get_cmd( const char* cmd )
{
    if( ! strcmp( cmd, "menu") )
        return LXPANELX_CMD_SYS_MENU;
    else if( ! strcmp( cmd, "run") )
        return LXPANELX_CMD_RUN;
    else if( ! strcmp( cmd, "config") )
        return LXPANELX_CMD_CONFIG;
    else if( ! strcmp( cmd, "restart") )
        return LXPANELX_CMD_RESTART;
    else if( ! strcmp( cmd, "exit") )
        return LXPANELX_CMD_EXIT;
    return -1;
}*/

int main( int argc, char** argv )
{
    char *display_name = (char *)getenv("DISPLAY");
//    XEvent ev;
    Window root;
    Atom cmd_atom;
//    int cmd = -1;
    /* int restart; */

    if( argc < 2 )
    {
        printf( usage );
        return 1;
    }
/*
    if (argc == 2)
        cmd = get_cmd(argv[1]);

    if (cmd != -1)
    {
        dpy = XOpenDisplay(display_name);
        if (dpy == NULL) {
            printf("Cant connect to display: %s\n", display_name);
            exit(1);
        }
        root = DefaultRootWindow(dpy);
        cmd_atom = XInternAtom(dpy, "_LXPANELX_CMD", False);
        memset(&ev, '\0', sizeof ev);
        ev.xclient.type = ClientMessage;
        ev.xclient.window = root;
        ev.xclient.message_type = cmd_atom;
        ev.xclient.format = 8;

        ev.xclient.data.b[0] = cmd;

        XSendEvent(dpy, root, False,
                   SubstructureRedirectMask|SubstructureNotifyMask, &ev);
        XSync(dpy, False);
        XCloseDisplay(dpy);
    }
    else*/
    {
        size_t buff_size = 0;
        int i;
        for (i = 1; i < argc; i++)
        {
            buff_size += strlen(argv[i]) + 1;
        }

        char * buff = (char *) calloc(buff_size, sizeof(char));

        size_t buff_pos = 0;
        for (i = 1; i < argc; i++)
        {
            size_t s = strlen(argv[i]) + 1;
            memcpy(buff + buff_pos, argv[i], s * sizeof(char));
            buff_pos += s;
        }

        
        dpy = XOpenDisplay(display_name);
        if (dpy == NULL) {
            printf("Cant connect to display: %s\n", display_name);
            exit(1);
        }
        root = DefaultRootWindow(dpy);
        cmd_atom = XInternAtom(dpy, "_LXPANELX_TEXT_CMD", False);

        Atom type_atom = XInternAtom(dpy, "UTF8_STRING", False);

        XChangeProperty (dpy, root, cmd_atom, type_atom, 8, PropModeReplace,
                         (unsigned char *) buff, buff_size);
        XSync(dpy, False);
        XCloseDisplay(dpy);
    }

    return 0;
}

