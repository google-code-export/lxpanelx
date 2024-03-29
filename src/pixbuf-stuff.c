/**
 * Copyright (c) 2012 Vadim Ushakov
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

#include <lxpanelx/pixbuf-stuff.h>
#include <lxpanelx/gtkcompat.h>
#include "panel_private.h"
#include <gdk/gdkx.h>
#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>

//#define PIXBUF_INTERP GDK_INTERP_NEAREST
//#define PIXBUF_INTERP GDK_INTERP_TILES
//#define PIXBUF_INTERP GDK_INTERP_BILINEAR
#define PIXBUF_INTERP_GOOD_QUALITY GDK_INTERP_HYPER
#define PIXBUF_INTERP_POOR_QUALITY GDK_INTERP_BILINEAR

/******************************************************************************/

/* Get a pixbuf from a pixmap.
 * Originally from libwnck, Copyright (C) 2001 Havoc Pennington. */
GdkPixbuf * _gdk_pixbuf_get_from_pixmap(Pixmap xpixmap, int width, int height)
{
    /* Get the drawable. */
    GdkDrawable * drawable = (GdkDrawable *) gdk_xid_table_lookup(xpixmap);
    if (drawable != NULL)
        g_object_ref(G_OBJECT(drawable));
    else
        drawable = gdk_pixmap_foreign_new(xpixmap);

    GdkColormap * colormap = NULL;
    GdkPixbuf * retval = NULL;
    if (drawable != NULL)
    {
        if (width < 0 || height < 0)
        {
            width = 1;
            height = 1;
            gdk_pixmap_get_size(GDK_PIXMAP(drawable), &width, &height);
        }

        /* Get the colormap.
         * If the drawable has no colormap, use no colormap or the system colormap as recommended in the documentation of gdk_drawable_get_colormap. */
        colormap = gdk_drawable_get_colormap(drawable);
        gint depth = gdk_drawable_get_depth(drawable);
        if (colormap != NULL)
            g_object_ref(G_OBJECT(colormap));
        else if (depth == 1)
            colormap = NULL;
        else
        {
            if (depth == 32)
                colormap = gdk_screen_get_rgba_colormap(gdk_drawable_get_screen(drawable));
            else
                colormap = gdk_screen_get_rgb_colormap(gdk_drawable_get_screen(drawable));
            if (!colormap)
                colormap = gdk_screen_get_system_colormap(gdk_drawable_get_screen(drawable));
            g_object_ref(G_OBJECT(colormap));
        }

        /* Be sure we aren't going to fail due to visual mismatch. */
        if ((colormap != NULL) && (gdk_colormap_get_visual(colormap)->depth != depth))
        {
            g_object_unref(G_OBJECT(colormap));
            colormap = NULL;
        }

        /* Do the major work. */
        retval = gdk_pixbuf_get_from_drawable(NULL, drawable, colormap, 0, 0, 0, 0, width, height);
    }

    /* Clean up and return. */
    if (colormap != NULL)
        g_object_unref(G_OBJECT(colormap));
    if (drawable != NULL)
        g_object_unref(G_OBJECT(drawable));
    return retval;
}

/* http://git.gnome.org/browse/libwnck/tree/libwnck/tasklist.c?h=gnome-2-30 */

void _wnck_dim_icon(GdkPixbuf *pixbuf)
{
    int x, y, pixel_stride, row_stride;
    guchar *row, *pixels;
    int w, h;
    int i;

    if (!pixbuf)
        return;

    if (!gdk_pixbuf_get_has_alpha(pixbuf))
        return;

    gdouble alpha_multiplier = g_key_file_get_double(global_settings, "Dim", "AlphaMultiplier", NULL);
    gdouble rgb_offset       = g_key_file_get_double(global_settings, "Dim", "RGBOffset", NULL);
    gdouble desaturation     = g_key_file_get_double(global_settings, "Dim", "Desaturation", NULL);

    w = gdk_pixbuf_get_width(pixbuf);
    h = gdk_pixbuf_get_height(pixbuf);

    pixel_stride = 4;

    row = gdk_pixbuf_get_pixels(pixbuf);
    row_stride = gdk_pixbuf_get_rowstride(pixbuf);

    for (y = 0; y < h; y++)
    {
        pixels = row;

        for (x = 0; x < w; x++)
        {
            if (desaturation)
            {
                int gray = (pixels[0] + pixels[1] + pixels[2]) / 3;
                pixels[0] = pixels[0] * (1.0 - desaturation) + gray * desaturation;
                pixels[1] = pixels[1] * (1.0 - desaturation) + gray * desaturation;
                pixels[2] = pixels[2] * (1.0 - desaturation) + gray * desaturation;
            }

            if (rgb_offset)
            {
                for (i = 0; i < 3; i++)
                {
                    int v = pixels[i] + rgb_offset * 255;
                    if (v < 0)
                        v = 0;
                    if (v > 255)
                        v = 255;
                    pixels[i] = v;
                }
            }

            pixels[3] *= alpha_multiplier;
            pixels += pixel_stride;
        }
        row += row_stride;
    }
}

/* Apply a mask to a pixbuf.
 * Originally from libwnck, Copyright (C) 2001 Havoc Pennington. */
GdkPixbuf * _gdk_pixbuf_apply_mask(GdkPixbuf * pixbuf, GdkPixbuf * mask)
{
    /* Initialize. */
    int w = MIN(gdk_pixbuf_get_width(mask), gdk_pixbuf_get_width(pixbuf));
    int h = MIN(gdk_pixbuf_get_height(mask), gdk_pixbuf_get_height(pixbuf));
    GdkPixbuf * with_alpha = gdk_pixbuf_add_alpha(pixbuf, FALSE, 0, 0, 0);
    guchar * dst = gdk_pixbuf_get_pixels(with_alpha);
    guchar * src = gdk_pixbuf_get_pixels(mask);
    int dst_stride = gdk_pixbuf_get_rowstride(with_alpha);
    int src_stride = gdk_pixbuf_get_rowstride(mask);

    /* Loop to do the work. */
    int i;
    for (i = 0; i < h; i += 1)
    {
        int j;
        for (j = 0; j < w; j += 1)
        {
            guchar * s = src + i * src_stride + j * 3;
            guchar * d = dst + i * dst_stride + j * 4;

            /* s[0] == s[1] == s[2], they are 255 if the bit was set, 0 otherwise. */
            d[3] = ((s[0] == 0) ? 0 : 255);	/* 0 = transparent, 255 = opaque */
        }
    }

    return with_alpha;
}

/******************************************************************************/

void _gdk_pixbuf_get_pixel (GdkPixbuf *pixbuf, int x, int y, unsigned * red, unsigned * green, unsigned * blue, unsigned * alpha)
{
    int width, height, rowstride, n_channels;
    guchar *pixels, *p;

    n_channels = gdk_pixbuf_get_n_channels (pixbuf);

    g_assert (gdk_pixbuf_get_colorspace (pixbuf) == GDK_COLORSPACE_RGB);
    g_assert (gdk_pixbuf_get_bits_per_sample (pixbuf) == 8);
    //g_assert (gdk_pixbuf_get_has_alpha (pixbuf));
    //g_assert (n_channels == 4);

    width = gdk_pixbuf_get_width (pixbuf);
    height = gdk_pixbuf_get_height (pixbuf);

    g_assert (x >= 0 && x < width);
    g_assert (y >= 0 && y < height);

    rowstride = gdk_pixbuf_get_rowstride (pixbuf);
    pixels = gdk_pixbuf_get_pixels (pixbuf);

    p = pixels + y * rowstride + x * n_channels;
    if (red)
        *red   = p[0];
    if (green)
        *green = p[1];
    if (blue)
        *blue  = p[2];
    if (alpha)
        *alpha = (n_channels > 3) ? p[3] : 0;
}

GdkPixbuf * _gdk_pixbuf_scale_in_rect(GdkPixbuf * pixmap, int required_width, int required_height, gboolean good_quality)
{
    /* If we got a pixmap, scale it and return it. */
    if (pixmap == NULL)
        return NULL;
    else
    {
        gulong w = gdk_pixbuf_get_width (pixmap);
        gulong h = gdk_pixbuf_get_height (pixmap);

        gulong w1 = w;
        gulong h1 = h;

        if ((w > required_width) || (h > required_height))
        {
            float rw = required_width;
            float rh = required_height;
            float sw = w;
            float sh = h;

            float scalew = rw / sw;
            float scaleh = rh / sh;
            float scale = scalew < scaleh ? scalew : scaleh;

            sw *= scale;
            sh *= scale;

            w = sw;
            h = sh;

            if (w < 2)
                w = 2;
            if (h < 2)
                h = 2;
        }

        GdkInterpType interp = good_quality ?
            PIXBUF_INTERP_GOOD_QUALITY:
            PIXBUF_INTERP_POOR_QUALITY;

        /* Attemp to avoid hang up in gdk_pixbuf_scale_simple(). */
        interp =
        ((w < 3 && h < 3) ||
         (w < 5 && h < 5 && w1 > 200 && h1 > 2000) ) ? GDK_INTERP_NEAREST : interp;

        //g_print("w = %d, h = %d --> w = %d, h = %d\n", w1, h1, w, h);

        GdkPixbuf * ret = gdk_pixbuf_scale_simple(pixmap, w, h, interp);

        return ret;
    }
}

void _gdk_pixbuf_get_color_sample (GdkPixbuf *pixbuf, GdkColor * c1, GdkColor * c2)
{
    /* scale pixbuff down */

    GdkPixbuf * p1 = _gdk_pixbuf_scale_in_rect(pixbuf, 3, 3, FALSE);

    gulong pw = gdk_pixbuf_get_width(p1);
    gulong ph = gdk_pixbuf_get_height(p1);

    gdouble r = 0, g = 0, b = 0;

    unsigned r1, g1, b1, a; int samples_count = 0;

    /* pick colors */

    if (pw < 1 || ph < 1)
    {
        r1 += 128; g1 += 128; b1 += 128; samples_count++;
    }
    else if (pw == 1 && ph == 1)
    {
        _gdk_pixbuf_get_pixel(p1, 0, 0, &r1, &g1, &b1, &a); r += r1; g += g1; b += b1; samples_count++;
    }
    else
    {
        if (pw > 1 && ph > 1)
            _gdk_pixbuf_get_pixel(p1, 1, 1, &r1, &g1, &b1, &a); r += r1; g += g1; b += b1; samples_count++;
        if (ph > 1)
            _gdk_pixbuf_get_pixel(p1, 0, 1, &r1, &g1, &b1, &a); r += r1; g += g1; b += b1; samples_count++;
        if (pw > 1)
            _gdk_pixbuf_get_pixel(p1, 1, 0, &r1, &g1, &b1, &a); r += r1; g += g1; b += b1; samples_count++;
        if (ph > 2)
            _gdk_pixbuf_get_pixel(p1, 0, 2, &r1, &g1, &b1, &a); r += r1; g += g1; b += b1; samples_count++;
        if (pw > 2)
            _gdk_pixbuf_get_pixel(p1, 2, 0, &r1, &g1, &b1, &a); r += r1; g += g1; b += b1; samples_count++;
    }

    g_object_unref(p1);

    /* to range (0,1) */

    r /= (samples_count * 255); g /= (samples_count * 255); b /= (samples_count * 255);

    if (r > 1)
        r = 1;
    if (g > 1)
        g = 1;
    if (b > 1)
        b = 1;

    gdouble h = 0, s = 0, v = 0;

    /* adjust saturation and value */

    gtk_rgb_to_hsv(r, g, b, &h, &s, &v);

    gdouble saturation_min = g_key_file_get_double(global_settings, "ColorSample", "SaturationMin", NULL);
    gdouble saturation_max = g_key_file_get_double(global_settings, "ColorSample", "SaturationMax", NULL);
    gdouble value_min = g_key_file_get_double(global_settings, "ColorSample", "ValueMin", NULL);
    gdouble value_max = g_key_file_get_double(global_settings, "ColorSample", "ValueMax", NULL);
    gdouble saturation_delta = g_key_file_get_double(global_settings, "ColorSample", "SaturationDelta", NULL);
    gdouble value_delta = g_key_file_get_double(global_settings, "ColorSample", "ValueDelta", NULL);

    if (s < saturation_min)
        s = saturation_min;
    if (s > saturation_max)
        s = saturation_max;

    if (v < value_min)
        v = value_min;
    if (v > value_max)
        v = value_max;

    gtk_hsv_to_rgb(h, s, v, &r, &g, &b);

    c1->red   = r * (256 * 256 - 1);
    c1->green = g * (256 * 256 - 1);
    c1->blue  = b * (256 * 256 - 1);

    s += saturation_delta;
    v += value_delta;

    gtk_hsv_to_rgb(h, s, v, &r, &g, &b);

    c2->red   = r * (256 * 256 - 1);
    c2->green = g * (256 * 256 - 1);
    c2->blue  = b * (256 * 256 - 1);

}

GdkPixbuf * _composite_thumb_icon(GdkPixbuf * thumb, GdkPixbuf * icon, int size, int icon_size)
{
    GdkPixbuf * p1 = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, size, size);
    gdk_pixbuf_fill(p1, 0x00000000);
    gulong w = gdk_pixbuf_get_width(thumb);
    gulong h = gdk_pixbuf_get_height(thumb);
    gulong x = (size - w) / 2;
    gulong y = (size - h) / 2;
    gdk_pixbuf_copy_area(thumb, 0, 0, w, h, p1, x, y);
    if (icon)
    {
        GdkPixbuf * p3 = _gdk_pixbuf_scale_in_rect(icon, icon_size, icon_size, TRUE);
        gulong w = gdk_pixbuf_get_width(p3);
        gulong h = gdk_pixbuf_get_height(p3);
        gdk_pixbuf_composite(p3, p1,
            size - w, size - h, w, h,
            size - w, size - h, 1, 1,
            PIXBUF_INTERP_GOOD_QUALITY,
            255);
        g_object_unref(p3);
    }
    return p1;
}
