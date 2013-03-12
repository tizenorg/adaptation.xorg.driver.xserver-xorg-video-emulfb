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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#include "X11/XWDFile.h"
#include "fbdev.h"
#include "fbdev_util.h"

int fbdev_util_dump_raw(const char * file, const void * data, int width, int height)
{
	unsigned int * blocks;

	FILE * fp = fopen(file, "w+");
	if (fp == NULL)
		return 0;

	blocks = (unsigned int *) data;
	fwrite(blocks, 4, width*height, fp);

	fclose(fp);

	return 1;
}

#ifndef RR_Rotate_All
#define RR_Rotate_All	(RR_Rotate_0|RR_Rotate_90|RR_Rotate_180|RR_Rotate_270)
#endif

int fbdev_util_degree_to_rotate(int degree)
{
	int rotate;

	switch(degree)
	{
	case 0:
		rotate = RR_Rotate_0;
		break;
	case 90:
		rotate = RR_Rotate_90;
		break;
	case 180:
		rotate = RR_Rotate_180;
		break;
	case 270:
		rotate = RR_Rotate_270;
		break;
	default:
		rotate = 0;	/* ERROR */
		break;
	}

	return rotate;
}

int fbdev_util_rotate_to_degree(int rotate)
{
	int degree;

	switch(rotate & RR_Rotate_All)
	{
	case RR_Rotate_0:
		degree = 0;
		break;
	case RR_Rotate_90:
		degree = 90;
		break;
	case RR_Rotate_180:
		degree = 180;
		break;
	case RR_Rotate_270:
		degree = 270;
		break;
	default:
		degree = -1;	/* ERROR */
		break;
	}

	return degree;
}

static int
_fbdev_util_rotate_to_int(int rot)
{
	switch(rot & RR_Rotate_All)
	{
	case RR_Rotate_0:
		return 0;
	case RR_Rotate_90:
		return 1;
	case RR_Rotate_180:
		return 2;
	case RR_Rotate_270:
		return 3;
	}

	return 0;
}

int fbdev_util_rotate_add(int rot_a, int rot_b)
{
	int a = _fbdev_util_rotate_to_int(rot_a);
	int b = _fbdev_util_rotate_to_int(rot_b);

	return (int)((1 << ((a+b)%4))&RR_Rotate_All);
}

const PropertyPtr
fbdev_util_get_window_property(WindowPtr pWin, const char* prop_name)
{
	int rc;
	Mask prop_mode = DixReadAccess;
	Atom property;
	PropertyPtr pProp;

	property = MakeAtom(prop_name, strlen(prop_name), FALSE);
	if(property == None)
		return NULL;

	rc = dixLookupProperty(&pProp, pWin, property, serverClient, prop_mode);
	if (rc == Success && pProp->data)
	{
		return pProp;
	}

	return NULL;
}


void
fbdev_util_rotate_rect (int xres,
                        int yres,
                        int src_rot,
                        int dst_rot,
                        xRectangle *src)
{
	int diff;
	xRectangle temp;

	return_if_fail (src != NULL);

	if (src_rot == dst_rot)
		return;

	diff = (dst_rot - src_rot);
	if (diff < 0)
		diff = 360 + diff;

	if (src_rot % 180 && diff % 180)
		SWAP (xres, yres);

	switch (diff)
	{
	case 270:
		temp.x = yres - (src->y + src->height);
		temp.y = src->x;
		temp.width  = src->height;
		temp.height = src->width;
		break;
	case 180:
		temp.x = xres  - (src->x + src->width);
		temp.y = yres - (src->y + src->height);
		temp.width  = src->width;
		temp.height = src->height;
		break;
	case 90:
		temp.x = src->y;
		temp.y = xres - (src->x + src->width);
		temp.width  = src->height;
		temp.height = src->width;
		break;
	default:
		temp.x = src->x;
		temp.y = src->y;
		temp.width  = src->width;
		temp.height = src->height;
		break;
	}

	*src = temp;
}

void
fbdev_util_align_rect (int src_w, int src_h, int dst_w, int dst_h, xRectangle *fit, Bool hw)
{
	int fit_width;
	int fit_height;
	float rw, rh, max;

	if (!fit)
		return;

	return_if_fail (src_w > 0 && src_h > 0);
	return_if_fail (dst_w > 0 && dst_h > 0);

	rw = (float)src_w / dst_w;
	rh = (float)src_h / dst_h;
	max = MAX (rw, rh);

	fit_width = src_w / max;
	fit_height = src_h / max;

	if (hw)
		fit_width &= (~0x3);

	fit->x = (dst_w - fit_width) / 2;
	fit->y = (dst_h - fit_height) / 2;
	fit->width = fit_width;
	fit->height = fit_height;
}


void
drvlog (const char * f, ...)
{
    va_list args;
    char temp[1024];

    va_start (args, f);
    vsnprintf (temp, sizeof (temp), f, args);
    va_end (args);

    fwrite (temp, strlen (temp), 1, stderr);
}
