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
#ifndef FBDEV_VIDEO_H
#define FBDEV_VIDEO_H

typedef enum {
		MODE_INIT,
		MODE_V4L2,
		MODE_VIRTUAL
} FBDEV_PORT_MODE;

/* FBDev port information structure */
typedef struct {
	int index;
	FBDEV_PORT_MODE mode;
	int size;

	RegionRec clip;

	int rotation;
	int hflip;
	int vflip;
	xRectangle    dst;
	RegionRec     clipBoxes;
	int preemption;    /* 1:high, 0:default, -1:low */
	DrawablePtr pDraw;

	int   v4l2_index;
	void *v4l2_handle;
} FBDevPortPriv, *FBDevPortPrivPtr;

extern Bool fbdevVideoInit(ScreenPtr pScreen);

extern void fbdevVideoFini(ScreenPtr pScreen);

#endif // FBDEV_VIDEO_H
