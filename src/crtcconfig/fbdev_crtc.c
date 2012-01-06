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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>

#include "xace.h"
#include "xacestr.h"
#include <X11/Xatom.h>

#include "fbdev.h"
#include "fbdevhw.h"
#include "fbdev_hw.h"
#include "fbdev_crtc_priv.h"
#include "fbdev_util.h"

#include "xf86Crtc.h"
#include "xf86Xinput.h"
#include "exevents.h"

/********************************************************************************************/
/* Implementation of Crtc entry points */
/********************************************************************************************/
/**
  * Turns the crtc on/off, or sets intermediate power levels if available.
  *
  * Unsupported intermediate modes drop to the lower power setting.  If the
  * mode is DPMSModeOff, the crtc must be disabled sufficiently for it to
  * be safe to call mode_set.
  */
static void fbdev_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
	return;
}

/**
  * Lock CRTC prior to mode setting, mostly for DRI.
  * Returns whether unlock is needed
  */
static Bool fbdev_crtc_lock(xf86CrtcPtr crtc)
{

	/* check whether unlock is needed */
	return TRUE;
}

/**
  * Lock CRTC prior to mode setting, mostly for DRI.
  * Returns whether unlock is needed
  */
static void fbdev_crtc_unlock(xf86CrtcPtr crtc)
{
	return;
}


/**
  * Callback to adjust the mode to be set in the CRTC.
  *
  * This allows a CRTC to adjust the clock or even the entire set of
  * timings, which is used for panels with fixed timings or for
  * buses with clock limitations.
  */
static Bool fbdev_crtc_mode_fixup(xf86CrtcPtr crtc, DisplayModePtr mode,
                                 DisplayModePtr adjusted_mode)
{
	return TRUE;
}

/**
  * Prepare CRTC for an upcoming mode set.
  */
static void fbdev_crtc_prepare(xf86CrtcPtr crtc)
{

}

/**
  * Callback for setting up a video mode after fixups have been made.
  */
static void fbdev_crtc_mode_set(xf86CrtcPtr crtc, DisplayModePtr mode,
                               DisplayModePtr adjusted_mode, int x, int y)
{

}

/**
  * Commit mode changes to a CRTC
  */
static void fbdev_crtc_commit(xf86CrtcPtr crtc)
{

}

/* Set the color ramps for the CRTC to the given values. */
static void fbdev_crtc_gamma_set(xf86CrtcPtr crtc, CARD16 * red, CARD16 * green,
                                CARD16 * blue, int size)
{

}

/**
  * Set cursor colors
  */
void fbdev_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{
	return;
}

/**
  * Set cursor position
  */
void fbdev_crtc_set_cursor_position(xf86CrtcPtr crtc, int x, int y)
{

	return;
}

/**
  * Show cursor
  */
void fbdev_crtc_show_cursor(xf86CrtcPtr crtc)
{

	return;
}

/**
  * Hide cursor
  */
void fbdev_crtc_hide_cursor(xf86CrtcPtr crtc)
{

	return;
}

/**
  * Load ARGB image
  */
void fbdev_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *image)
{

	return;
}

/**
  * Less fine-grained mode setting entry point for kernel modesetting
  */
Bool fbdev_crtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
                              Rotation rotation, int x, int y)
{
	return TRUE;
}


/**
  * Callback for panning. Doesn't change the mode.
  * Added in ABI version 2
  */
static void fbdev_crtc_set_origin(xf86CrtcPtr crtc, int x, int y)
{
	xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR, "set_origin: unimplemented");
	return;			/* XXX implement */
}

/* crtc funcs */
static const xf86CrtcFuncsRec fbdev_crtc_funcs =
{
	.dpms = fbdev_crtc_dpms,
	.save = NULL, /* XXX */
	.restore = NULL, /* XXX */
	.lock = fbdev_crtc_lock,
	.unlock = fbdev_crtc_unlock,
	.mode_fixup = fbdev_crtc_mode_fixup,

	.prepare = fbdev_crtc_prepare,
	.mode_set = fbdev_crtc_mode_set,
	.commit = fbdev_crtc_commit,
	.gamma_set = fbdev_crtc_gamma_set,

	.shadow_create = NULL,
	.shadow_allocate = NULL,
	.shadow_destroy = NULL,

	.set_cursor_colors = fbdev_crtc_set_cursor_colors,
	.set_cursor_position = fbdev_crtc_set_cursor_position,
	.show_cursor = fbdev_crtc_show_cursor,
	.hide_cursor = fbdev_crtc_hide_cursor,
	.load_cursor_argb = fbdev_crtc_load_cursor_argb,
	.destroy = NULL, /* XXX */
	.set_mode_major = NULL, /*???*/
#if RANDR_13_INTERFACE
	.set_origin = fbdev_crtc_set_origin,
#endif
};

void FBDevCrtcInit(ScrnInfoPtr pScrn, int num)
{
	xf86CrtcPtr crtc;
	FBDevCrtcPrivPtr fbdev_crtc;

	crtc = xf86CrtcCreate (pScrn, &fbdev_crtc_funcs);
	if (crtc == NULL)
		return;

	fbdev_crtc = xnfcalloc (sizeof (FBDevCrtcPriv), 1);
	if (!fbdev_crtc)
	{
		xf86CrtcDestroy (crtc);
		return;
	}

	crtc->driver_private = fbdev_crtc;

}


