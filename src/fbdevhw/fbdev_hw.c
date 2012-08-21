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

#include "fbdev.h"
#include "fbdev_hw.h"

void
FBDevGetVarScreenInfo(int fd, struct fb_var_screeninfo *fbVarInfo)
{
	if (0 != ioctl(fd,FBIOGET_VSCREENINFO, (void*)fbVarInfo))
		fprintf(stderr, "Error : fail to get FBIOGET_VSCREENINFO\n");
}

void
FBDevSetVarScreenInfo(int fd, struct fb_var_screeninfo *fbVarInfo)
{
	if (0 != ioctl(fd,FBIOPUT_VSCREENINFO,(void*)fbVarInfo))
		fprintf(stderr, "Error : fail to get FBIOPUT_VSCREENINFO\n");
}

void
FBDevGetFixScreenInfo(int fd, struct fb_fix_screeninfo *fbFixInfo)
{
	if (0 != ioctl(fd,FBIOGET_FSCREENINFO,(void*)fbFixInfo))
		fprintf(stderr, "Error : fail to get FBIOGET_FSCREENINFO\n");
}

Bool
FBDevScreenAlphaInit(int fd)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl(fd, FBIOGET_VSCREENINFO, &var);
	if (ret < 0)
	{
		return FALSE;
	}

	if (var.bits_per_pixel != 32)
	{
		return FALSE;
	}
	var.transp.length = 8;
	var.activate = FB_ACTIVATE_FORCE;
	ret = ioctl(fd, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0)
	{
		return FALSE;
	}

	return TRUE;
}

Bool
FBDevScreenAlphaDeinit(int fd)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl(fd, FBIOGET_VSCREENINFO, &var);
	if (ret < 0)
	{
		return FALSE;
	}

	var.transp.length = 0;
	ret = ioctl(fd, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0)
	{
		return FALSE;
	}

	return TRUE;
}

#include <sys/utsname.h>

/* activate fb */
Bool
FBDevActivateFB(int fd)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl(fd, FBIOGET_VSCREENINFO, &var);
	if (ret < 0)
	{
		ErrorF("failed to get fb_var\n");
		return FALSE;
	}

	var.activate = FB_ACTIVATE_FORCE;

	ret = ioctl (fd, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0)
	{
		ErrorF("failed to set fb_var.activate\n");
		return FALSE;
	}

	ret = ioctl (fd, FBIOBLANK, FB_BLANK_UNBLANK);
	if (ret < 0)
	{
		ErrorF("failed to set FBIOBLANK : FB_BLANK_UNBLANK\n");
		return FALSE;
	}

	return TRUE;
}

/* deactivate fb3 */
Bool
FBDevDeActivateFB(int fd)
{
	int ret;

	ret = ioctl (fd, FBIOBLANK, FB_BLANK_POWERDOWN);
	if (ret < 0)
	{
		ErrorF("failed to set FBIOBLANK : FB_BLANK_POWERDOWN\n");
		return FALSE;
	}

	return TRUE;
}

Bool
FBDevSetBaseFrameBuffer(int fd)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl(fd, FBIOGET_VSCREENINFO, &var);
	if (ret < 0)
	{
		return FALSE;
	}

	var.yoffset = 0;
	ret = ioctl (fd, FBIOPAN_DISPLAY,&var);
	if (ret < 0)
	{
		return FALSE;
	}
	return TRUE;
}


void
FBDevHWPanDisplay(int fd, int x, int y)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl(fd, FBIOGET_VSCREENINFO, &var);
	if(ret < 0)
	{
		return;
	}

	var.xoffset = x;
	var.yoffset = y;
	ret = ioctl(fd, FBIOPAN_DISPLAY, &var);
	if(ret < 0)
	{
		return;
	}
}
