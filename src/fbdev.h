/**************************************************************************

xserver-xorg-video-emulfb

Copyright 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: SooChan Lim <sc1.lim@samsung.com>

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

#ifndef FBDEV_H
#define FBDEV_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#include "fbdevhw.h"
#include "xf86xv.h"
#include <linux/fb.h>

#define PAGE_SIZE 4096

#define FBDEV_VERSION 1000 /* the version of the driver */
#define FBDEV_NAME "FBDEV" /* the name used to prefix messages */
#define FBDEV_DRIVER_NAME "emulfb" /* the driver name as used in config file.
				 * This name should match the name of the driver module binary
				 * In this driver, the name of the driver libary module is emulfb_drv.so.
																			  */
#define SWAPINT(i, j) \
{  int _t = i;  i = j;  j = _t; }

#define SEC_CURSOR_W	64
#define SEC_CURSOR_H	64

#define ADAPTOR_NUM  2

/* FBDev driver private data structure to hold the driver's screen-specific data */
typedef struct {
	unsigned char *fbstart;	/* start memory point of framebuffer: (fbmem + fboff) */
	unsigned char *fbmem;	/* mmap memory pointer of framebuffer */
	int fboff;		/* fb offset */
	int lineLength;
	CloseScreenProcPtr CloseScreen;
	CreateScreenResourcesProcPtr CreateScreenResources;
	void (*PointerMoved)(int index, int x, int y);
	EntityInfoPtr pEnt;

	/* driver options */
	OptionInfoPtr Options;
	Bool bSWCursorEnbled; /* software cursor enabled */
	int rotate;

	/* saved video mode */
	struct fb_var_screeninfo saved_var;

	/* Current information of Framebuffer */
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;

	/* mode infos */
	DisplayModePtr builtin;
	DisplayModeRec builtin_saved; /* original mode to send the fake mode when the screen rotates */
	DisplayModePtr support_modes;

        /* xv */
#ifdef XV
	XF86VideoAdaptorPtr pAdaptor[ADAPTOR_NUM];
	void  **v4l2_owner;
	int     v4l2_num;
	Bool bFbAlphaEnabled;
#endif

	Bool bLockScreen;

	/* dpms - flag for the control of lcd onoff*/
	Bool isLcdOff;

	/* Cursor */
	Bool enableCursor;
} FBDevRec, *FBDevPtr;
#define FBDEVPTR(p) ((FBDevPtr)((p)->driverPrivate))

#endif //FBDEV_H

