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

#ifndef __FBDEV_VIDEO_FOURCC_H__
#define __FBDEV_VIDEO_FOURCC_H__

#include <fourcc.h>

/* http://www.fourcc.org/yuv.php
 * http://en.wikipedia.org/wiki/YUV
 */
#define FOURCC_RGB565 0x50424742
#define XVIMAGE_RGB565 \
   { \
    FOURCC_RGB565, \
    XvRGB, \
    LSBFirst, \
    {'R','G','B','P', \
        0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    16, \
    XvPacked, \
    1, \
    16, 0x0000F800, 0x000007E0, 0x0000001F, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, \
    {'R','G','B',0, \
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define FOURCC_RGB24 0x33424752
#define XVIMAGE_RGB24 \
   { \
    FOURCC_RGB24, \
    XvRGB, \
    LSBFirst, \
    {'R','G','B',0, \
        0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    24, \
    XvPacked, \
    1, \
    24, 0x00FF0000, 0x0000FF00, 0x000000FF, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, \
    {'R','G','B',0, \
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define FOURCC_RGB32 0x34424752
#define XVIMAGE_RGB32 \
   { \
    FOURCC_RGB32, \
    XvRGB, \
    LSBFirst, \
    {'R','G','B',0, \
        0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    32, \
    XvPacked, \
    1, \
    24, 0x00FF0000, 0x0000FF00, 0x000000FF, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, \
    {'X','R','G','B', \
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define FOURCC_ST12 0x32315453
#define XVIMAGE_ST12 \
   { \
    FOURCC_ST12, \
    XvYUV, \
    LSBFirst, \
    {'S','T','1','2', \
      0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    12, \
    XvPlanar, \
    3, \
    0, 0, 0, 0, \
    8, 8, 8, \
    1, 2, 2, \
    1, 2, 2, \
    {'Y','U','V', \
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define FOURCC_SN12 0x32314E53
#define XVIMAGE_SN12 \
   { \
    FOURCC_SN12, \
    XvYUV, \
    LSBFirst, \
    {'S','N','1','2', \
      0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    12, \
    XvPlanar, \
    3, \
    0, 0, 0, 0, \
    8, 8, 8, \
    1, 2, 2, \
    1, 2, 2, \
    {'Y','U','V', \
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define FOURCC_NV12 0x3231564E
#define XVIMAGE_NV12 \
   { \
    FOURCC_NV12, \
    XvYUV, \
    LSBFirst, \
    {'N','V','1','2', \
      0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    12, \
    XvPacked, \
    1, \
    0, 0, 0, 0, \
    8, 8, 8, \
    1, 2, 2, \
    1, 2, 2, \
    {'Y','U','V', \
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define FOURCC_S420 0x30323453
#define XVIMAGE_S420 \
   { \
	FOURCC_S420, \
        XvYUV, \
	LSBFirst, \
	{'S','4','2','0', \
	  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
	12, \
	XvPlanar, \
	3, \
	0, 0, 0, 0, \
	8, 8, 8, \
	1, 2, 2, \
	1, 2, 2, \
	{'Y','U','V', \
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
	XvTopToBottom \
   }
#define FOURCC_SUYV 0x56595553
#define XVIMAGE_SUYV \
   { \
	FOURCC_SUYV, \
        XvYUV, \
	LSBFirst, \
	{'S','U','Y','V', \
	  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
	16, \
	XvPacked, \
	1, \
	0, 0, 0, 0, \
	8, 8, 8, \
	1, 2, 2, \
	1, 1, 1, \
	{'Y','U','Y','V', \
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
	XvTopToBottom \
   }


#endif // __FBDEV_VIDEO_FOURCC_H__
