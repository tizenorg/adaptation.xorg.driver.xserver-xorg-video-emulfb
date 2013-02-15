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
#include "fbdev_util.h"
#include "fbdev_fb.h"

struct s3cfb_user_window
{
	int x;
	int y;
};

#define S3CFB_WIN_ON                _IOW('F', 305, uint)
#define S3CFB_WIN_OFF               _IOW('F', 306, uint)
#define S3CFB_WIN_POSITION          _IOW('F', 203, struct s3cfb_user_window)
#define S3CFB_WIN_SET_PIXEL_ALPHA   _IOW('F', 211, struct fb_var_screeninfo)

Bool
fbdevFbGetVarScreenInfo (int fd, struct fb_var_screeninfo *fbVarInfo)
{
	int ret;

	return_val_if_fail (fd >= 0, FALSE);
	return_val_if_fail (fbVarInfo != NULL, FALSE);

	ret = ioctl (fd, FBIOGET_VSCREENINFO, (void*)fbVarInfo);

	if (ret)
	{
		xf86DrvMsg (0, X_ERROR, "FBIOGET_VSCREENINFO failed.\n");
		return FALSE;
	}

	return TRUE;
}

Bool
fbdevFbSetVarScreenInfo (int fd, struct fb_var_screeninfo *fbVarInfo)
{
	int ret;

	return_val_if_fail (fd >= 0, FALSE);
	return_val_if_fail (fbVarInfo != NULL, FALSE);

	ret = ioctl (fd, FBIOPUT_VSCREENINFO, (void*)fbVarInfo);

	if (ret)
	{
		xf86DrvMsg (0, X_ERROR, "FBIOPUT_VSCREENINFO failed.\n");
		return FALSE;
	}

	return TRUE;
}

Bool
fbdevFbGetFixScreenInfo (int fd, struct fb_fix_screeninfo *fbFixInfo)
{
	int ret;

	return_val_if_fail (fd >= 0, FALSE);
	return_val_if_fail (fbFixInfo != NULL, FALSE);

	ret = ioctl (fd, FBIOGET_FSCREENINFO, (void*)fbFixInfo);

	if (ret)
	{
		xf86DrvMsg (0, X_ERROR, "FBIOGET_FSCREENINFO failed.\n");
		return FALSE;
	}

	return TRUE;
}

Bool
fbdevFbSetWinPosition (int fd, int x, int y)
{
	struct s3cfb_user_window pos;
	int ret;

	return_val_if_fail (fd >= 0, FALSE);

	pos.x = x;
	pos.y = y;

	ret = ioctl (fd, S3CFB_WIN_POSITION, &pos);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "S3CFB_WIN_POSITION failed. pos(%d,%d)\n", x, y);
		return FALSE;
	}

	return TRUE;
}

Bool
fbdevFbScreenAlphaInit (int fd)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl (fd, FBIOGET_VSCREENINFO, &var);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "failed to get fb_var\n");
		return FALSE;
	}

	if (var.bits_per_pixel != 32)
	{
		xf86DrvMsg (0, X_ERROR, "per pixel overlay alpha is supported with 32 bpp mode\n");
		return FALSE;
	}
	var.transp.length = 8;
	var.activate = FB_ACTIVATE_FORCE;
	ret = ioctl (fd, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "failed to set fb_var\n");
		return FALSE;
	}

	return TRUE;
}

Bool
fbdevFbScreenAlphaDeinit (int fd)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl (fd, FBIOGET_VSCREENINFO, &var);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "failed to get fb_var\n");
		return FALSE;
	}

	var.transp.length = 0;
	ret = ioctl (fd, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "failed to set fb_var\n");
		return FALSE;
	}

	return TRUE;
}

#include <sys/utsname.h>
void
fbdevFbResetLCDModule()
{
	char rmmod[255];
	char insmod[255];
	struct utsname name;

	if (uname (&name))
	{
		ErrorF ("Fail get umane\n");
	}
	sprintf (rmmod, "/sbin/rmmod s3c_lcd");
	sprintf (insmod, "/sbin/insmod /opt/driver/s3c_lcd.ko");

	if (system (rmmod))
		ErrorF ("%s failed.\n", rmmod);
	if (system (insmod))
		ErrorF ("%s failed.\n", insmod);
}


/* activate fb */
Bool
fbdevFbActivate (int fd)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl (fd, FBIOGET_VSCREENINFO, &var);
	if (ret < 0)
	{
		ErrorF ("failed to get fb_var\n");
		return FALSE;
	}

	var.activate = FB_ACTIVATE_FORCE;

	ret = ioctl (fd, FBIOPUT_VSCREENINFO, &var);
	if (ret < 0)
	{
		ErrorF ("failed to set fb_var.activate\n");
		return FALSE;
	}

	ret = ioctl (fd, FBIOBLANK, FB_BLANK_UNBLANK);
	if (ret < 0)
	{
		ErrorF ("failed to set FBIOBLANK : FB_BLANK_UNBLANK\n");
		return FALSE;
	}

	return TRUE;
}

/* deactivate fb3 */
Bool
fbdevFbDeActivate (int fd)
{
	int ret;

	ret = ioctl (fd, FBIOBLANK, FB_BLANK_NORMAL);
	if (ret < 0)
	{
		ErrorF ("failed to set FBIOBLANK : FB_BLANK_NORMAL\n");
		return FALSE;
	}

	return TRUE;
}

Bool
fbdevFbSetBase (int fd)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl (fd, FBIOGET_VSCREENINFO, &var);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "failed to get fb_var\n");
		return FALSE;
	}

	var.yoffset = 0;
	ret = ioctl (fd, FBIOPAN_DISPLAY,&var);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "failed to set fb_var\n");
		return FALSE;
	}
	return TRUE;
}


void
fbdevFbHWPanDisplay (int fd, int x, int y)
{
	struct fb_var_screeninfo var;
	int ret;

	ret = ioctl (fd, FBIOGET_VSCREENINFO, &var);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_WARNING, "!!WARNING::%s::fail to get vscreen_info\n",
		            __func__);
		return;
	}

	var.xoffset = x;
	var.yoffset = y;
	ret = ioctl (fd, FBIOPAN_DISPLAY, &var);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_WARNING, "!!WARNING::%s::fail to pandisplay(xoff,%d)(yoff,%d)\n",
		            __func__,
		            var.xoffset,
		            var.yoffset);
		return;
	}
}
