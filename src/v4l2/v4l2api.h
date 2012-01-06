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

#include "v4l2config.h"
#include "v4l2_output_api.h"

int GetDisplayNums (void);
int AvailableDisplay (int index);

int CreateDisplay(void **pphandle, int index, int requestbuffer);

int DeleteDisplay(void *phandle, int exit);

int SetDisplayFormat(void *phandle, int w, int h, CRECT *dest, CRECT *crop, CRECT *win, V4L2Videoformat format, int rotate, int hflip, int vflip, int requestbuffer, int *imagesize);

int DrawDisplay (void *phandle,
                 unsigned char *buf,
                 xRectangle *img,
                 xRectangle *pixmap,
                 xRectangle *draw,
                 xRectangle *src,
                 xRectangle *dst,
                 RegionPtr   clip_region);

void * GetCapture(void * phandle, unsigned short * width, unsigned short * height,  int * rotation, V4L2Videoformat * format, int bUseRpScaler);

void * imgcpy(int width, int height,
				void * d, int d_off_x, int d_off_y, int d_size_w, int d_size_h,
				void *s, int s_off_x, int s_off_y, int s_size_w, int s_size_h,
				int * pitches, int * offsets, int channel);
