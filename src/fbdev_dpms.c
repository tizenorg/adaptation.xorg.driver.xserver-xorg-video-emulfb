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
#include <linux/fb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "fbdev.h"
#include "fbdevhw.h"
#include "fbdev_hw.h"
#include <X11/extensions/dpmsconst.h>

#include "misc.h"
#include "fbdev_dpms.h"

static void
fbdevDPMSSetFunc(ScrnInfoPtr pScrn, int mode, int flags)
{
	FBDevPtr pFBDev = FBDEVPTR(pScrn);

	switch(DPMSPowerLevel)
	{
	case DPMSModeOn:
	case DPMSModeSuspend:
		break;
	case DPMSModeStandby:
		if(pFBDev->isLcdOff == FALSE) break;

		/* lcd on */
		if (-1 == ioctl(fbdevHWGetFD(pScrn), FBIOBLANK, FB_BLANK_UNBLANK))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "FBIOBLANK: %s\n", strerror(errno));
		}

		pFBDev->isLcdOff = FALSE;
		break;
	case DPMSModeOff:
		if(pFBDev->isLcdOff == TRUE) break;

		/* lcd off */
		if (-1 == ioctl(fbdevHWGetFD(pScrn), FBIOBLANK, FB_BLANK_POWERDOWN))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "FBIOBLANK: %s\n", strerror(errno));
		}

		pFBDev->isLcdOff = TRUE;
		break;
	default:
		return;
	}
}

xf86DPMSSetProc*
FBDevDPMSSet(void)
{
	return fbdevDPMSSetFunc;
}

static Bool first_savescreen = FALSE;

static Bool
fbdevSaveScreenFunc(ScreenPtr pScreen, int mode)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

	if(!first_savescreen)
	{
		first_savescreen = TRUE;
		if (-1 == ioctl(fbdevHWGetFD(pScrn), FBIOBLANK, (void *)(0)))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "FBIOBLANK: %s\n", strerror(errno));
		}
	}

	return TRUE;
}


SaveScreenProcPtr
FBDevSaveScreen(void)
{
	return fbdevSaveScreenFunc;
}

