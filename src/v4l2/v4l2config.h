/**************************************************************************

xserver-xorg-video-emulfb

Copyright 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: YoungHoon Jung <yhoon.jung@samsung.com>

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

#ifndef __V4L2CONFIG_H__
#define __V4L2CONFIG_H__

#define _V4L2_DEBUG // Disable this to speed up
#define _NEON_MEMCPY_
//#define RP_SCALER

#ifndef FALSE
#define FALSE 0
#define TRUE  (!FALSE)
#endif

typedef struct _CRECT
{
        int x;
        int y;
        int w;
        int h;
} CRECT;

typedef enum {
        V4L2_VIDEO_FMT_UNKNOWN,
        V4L2_VIDEO_FMT_UYVY,
        V4L2_VIDEO_FMT_YUYV,
        V4L2_VIDEO_FMT_YUY2,
        V4L2_VIDEO_FMT_YV12,
        V4L2_VIDEO_FMT_SUY2,
        V4L2_VIDEO_FMT_I420,
        V4L2_VIDEO_FMT_S420,
        V4L2_VIDEO_FMT_YU12,
        V4L2_VIDEO_FMT_NV12,
        V4L2_VIDEO_FMT_NV12T,
		V4L2_VIDEO_FMT_RGB565,
        V4L2_VIDEO_FMT_RGB24,
        V4L2_VIDEO_FMT_RGB32,
} V4L2Videoformat;

#endif 	// __V4L2CONFIG_H__
