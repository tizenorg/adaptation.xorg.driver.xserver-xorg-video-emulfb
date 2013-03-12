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
#include "fbdevhw.h"
#include "fbdev_crtcconfig.h"
#include "fbdev_crtc_priv.h"
#include "fbdev_output_priv.h"

#include "xf86Crtc.h"

#define MIN_CRTC_DPY_WIDTH 320
#define MAX_CRTC_DPY_WIDTH 1024
#define MIN_CRTC_DPY_HEIGHT 200
#define MAX_CRTC_DPY_HEIGHT 1024

#define NUM_CRTCS 1

/**
  * Requests that the driver resize the screen.
  *
  * The driver is responsible for updating scrn->virtualX and scrn->virtualY.
  * If the requested size cannot be set, the driver should leave those values
  * alone and return FALSE.
  *
  * A naive driver that cannot reallocate the screen may simply change
  * virtual[XY].  A more advanced driver will want to also change the
  * devPrivate.ptr and devKind of the screen pixmap, update any offscreen
  * pixmaps it may have moved, and change pScrn->displayWidth.
  */
static Bool fbdev_crtc_config_resize (ScrnInfoPtr scrn, int width, int height)
{

	scrn->virtualX = width;
	scrn->virtualY = height;

	return TRUE;
}


/* crtc_config_func */
static const xf86CrtcConfigFuncsRec fbdev_crtc_config_funcs =
{
	.resize = fbdev_crtc_config_resize,
};


/*
 * Initialize the CrtcConfig.
 * And initialize the mode setting throught create the avaliable crtcs and outputs
 * then Initialize the Configuration of Crtc
 */
Bool FBDevCrtcConfigInit(ScrnInfoPtr pScrn)
{
	int min_width, max_width, min_height, max_height;
	xf86CrtcConfigPtr crtc_config;
	int i, o, c;
	FBDevPtr pFBDev = FBDEVPTR(pScrn);
	/* TODO: check this routines later whether it is right setting */
	{

		fbdevHWUseBuildinMode(pScrn);   /* sets pScrn->modes */
		pScrn->modes = xf86DuplicateMode(pScrn->modes); /* well done, fbdevhw. */
		pScrn->modes->name = NULL;      /* fbdevhw string can't be freed */
		pScrn->modes->type = M_T_DRIVER | M_T_PREFERRED;
		pScrn->currentMode = pScrn->modes;
		pFBDev->builtin = xf86DuplicateMode(pScrn->modes);

	}

	/* allocate an xf86CrtcConfig */
	xf86CrtcConfigInit(pScrn, &fbdev_crtc_config_funcs);
	crtc_config = XF86_CRTC_CONFIG_PTR (pScrn);

	min_width = pScrn->modes->HDisplay;
	max_width = pScrn->modes->HDisplay;
	min_height = pScrn->modes->VDisplay;
	max_height =  pScrn->modes->VDisplay;

	xf86CrtcSetSizeRange(pScrn, min_width, min_height, max_width, max_height);

	/* set up the crtcs */
	for( i = 0; i < NUM_CRTCS; i++)
		FBDevCrtcInit(pScrn, i);

	/* set up the outputs */
	LcdOutputInit(pScrn);

	/* [TODO]: set the crtc to the output in some manner ???? */
	for (o = 0; o < crtc_config->num_output; o++)
	{
		xf86OutputPtr output = crtc_config->output[o];
		int crtc_mask;

		crtc_mask = 0;
		for (c = 0; c < crtc_config->num_crtc; c++)
		{
			crtc_mask |= (1 << c);
		}
		output->possible_crtcs = crtc_mask;
		output->possible_clones = FALSE;
	}

	/* initialize the configuration */
	if(!xf86InitialConfiguration(pScrn, TRUE))
	{
		return FALSE;
	}

	return TRUE;

}
