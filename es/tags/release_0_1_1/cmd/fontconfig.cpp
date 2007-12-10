/*
 * Copyright (c) 2007
 * Nintendo Co., Ltd.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Nintendo makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

#include <string.h>
#include <cairo.h>
#include <es.h>
#include <es/handle.h>
#include <es/base/IProcess.h>
#include <es/base/IStream.h>

using namespace es;

ICurrentProcess* System();

#define WIDTH   1024
#define HEIGHT  768

Handle<IPageable> framebuffer;
u8* framebufferPtr;

void init()
{
    Handle<IContext> root = System()->getRoot();
    framebuffer = root->lookup("device/framebuffer");
    long long size;
    size = framebuffer->getSize();
    framebufferPtr = (u8*) System()->map(0, size,
                                 ICurrentProcess::PROT_READ | ICurrentProcess::PROT_WRITE,
                                 ICurrentProcess::MAP_SHARED,
                                 framebuffer, 0);
}

/*
    To test, copy the the following files to the disk image:
        vcopy /dev/sdb ../../es-trunk/cmd/fonts.conf
        vcopy /dev/sdb ../../es-trunk/cmd/fonts.dtd
        vcopy /dev/sdb ../../es-trunk/cmd/40-generic.conf conf.d/
        vcopy /dev/sdb /usr/share/fonts/liberation/LiberationMono-Regular.ttf fonts/
        vcopy /dev/sdb /usr/share/fonts/liberation/LiberationSans-Regular.ttf fonts/
        vcopy /dev/sdb /usr/share/fonts/liberation/LiberationSerif-Regular.ttf fonts/

 */
int main(int argc, char* argv[])
{
    init();

    cairo_surface_t *surface;
    cairo_t *cr;

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 320, 320);
    cr = cairo_create (surface);

    cairo_translate (cr, 10, 10);
    cairo_scale (cr, 100, 100);

    cairo_rectangle (cr, 0.0, 0.0, 3.0, 3.0);
    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
    cairo_fill (cr);

    cairo_text_extents_t te;

    cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
    cairo_select_font_face (cr, "Liberation Serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, 1.0);
    cairo_text_extents (cr, "Hello", &te);
    cairo_move_to (cr, 1.5 - te.width / 2 - te.x_bearing, 0.5 - te.height / 2 - te.y_bearing);
    cairo_show_text (cr, "Hello");

    cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);
    cairo_select_font_face (cr, "Liberation Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, 1.0);
    cairo_text_extents (cr, "Hello", &te);
    cairo_move_to (cr, 1.5 - te.width / 2 - te.x_bearing, 1.5 - te.height / 2 - te.y_bearing);
    cairo_show_text (cr, "Hello");

    cairo_set_source_rgb (cr, 0.0, 0.0, 1.0);
    cairo_select_font_face (cr, "Liberation Mono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, 1.0);
    cairo_text_extents (cr, "Hello", &te);
    cairo_move_to (cr, 1.5 - te.width / 2 - te.x_bearing, 2.5 - te.height / 2 - te.y_bearing);
    cairo_show_text (cr, "Hello");

    esReport("--- cairo_image_surface_get_data ---\n");
    u8* data = cairo_image_surface_get_data (surface);
    for (int y = 0; y < 320; ++y)
    {
        memmove(framebufferPtr + 4 * WIDTH * y,
                data + 4 * 320 * y,
                4 * 320);
    }
}
