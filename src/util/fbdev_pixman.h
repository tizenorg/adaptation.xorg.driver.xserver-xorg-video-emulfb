/**************************************************************************

xserver-xorg-video-emulfb

Copyright 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: Boram Park <boram1288.park@samsung.com>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#ifndef __FBDEV_PIXMAN_H__
#define __FBDEV_PIXMAN_H__

#include <sys/types.h>
#include <X11/Xdefs.h>
#include <fbdevhw.h>

#include <pixman.h>

#ifndef FALSE
#define FALSE 0
#define TRUE  (!FALSE)
#endif

#ifndef NULL
#define NULL (void*)0
#endif

int
fbdev_pixman_convert_image (pixman_op_t         op,
                            unsigned char      *srcbuf,
                            unsigned char      *dstbuf,
                            pixman_format_code_t src_format,
                            pixman_format_code_t dst_format,
                            xRectangle *img,
                            xRectangle *pxm,
                            xRectangle *src,
                            xRectangle *dst,
                            RegionPtr   clip_region,
                            int         rotate,
                            int         hflip,
                            int         vflip);

#endif /* __FBDEV_PIXMAN_H__ */
