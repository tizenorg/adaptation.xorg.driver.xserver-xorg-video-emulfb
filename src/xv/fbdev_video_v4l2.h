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

#ifndef __FBDEV_VIDEO_V4L2_H__
#define __FBDEV_VIDEO_V4L2_H__

#include <fbdevhw.h>
#include <xf86xv.h>

#include "fbdev_v4l2.h"

XF86ImagePtr fbdevVideoV4l2SupportImages (int *count);

Bool    fbdevVideoV4l2GetFormatInfo (int id, int *type, uint *pixelformat, FBDevV4l2Memory *memory);
Bool    fbdevVideoV4l2CheckSize (void *handle, uint pixelformat,
                                 xRectangle *img, xRectangle *src, xRectangle *dst,
                                 int type, int memory);

int     fbdevVideoV4l2GetHandleNums (void);
Bool    fbdevVideoV4l2HandleOpened  (int index);

void*   fbdevVideoV4l2OpenHandle    (ScreenPtr pScreen, int index, int requestbuffer);
void    fbdevVideoV4l2CloseHandle   (void *handle);

Bool    fbdevVideoV4l2StreamOn      (void *handle);
void    fbdevVideoV4l2StreamOff     (void *handle);

int     fbdevVideoV4l2SetFormat     (void *handle,
                                     xRectangle *img,   /* src->x, src->y : not used. */
                                     xRectangle *crop,
                                     xRectangle *dest,
                                     uint id,
                                     int  scn_rotate,
                                     int  hw_rotate,
                                     int  hflip,
                                     int  vflip,
                                     int  requestbuffer,
                                     Bool  sync);

int     fbdevVideoV4l2Draw          (void *handle, uchar *buf, uint *phy_addrs);

#endif
