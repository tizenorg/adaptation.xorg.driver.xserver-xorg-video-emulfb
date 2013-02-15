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
#include "fbdev_output_priv.h"
#include "fbdev_mode.h"
#include "fbdev_util.h"
#include "fbdev_video.h"

#include <xf86Crtc.h>

#define STR_XRR_VIDEO_OFFSET_PROPERTY "XRR_PROPERTY_VIDEO_OFFSET"

static Bool g_video_offset_prop_init = FALSE;

static Atom xrr_property_video_offset_atom;

static Bool
_lcd_output_prop_video_offset (xf86OutputPtr pOutput, Atom property, RRPropertyValuePtr value)
{
	int x, y;
	char str[128];
	char *p;

	if (g_video_offset_prop_init == FALSE)
	{
		xrr_property_video_offset_atom = MakeAtom (STR_XRR_VIDEO_OFFSET_PROPERTY,
		                                           strlen (STR_XRR_VIDEO_OFFSET_PROPERTY), TRUE);
		g_video_offset_prop_init = TRUE;
	}

	if (xrr_property_video_offset_atom != property)
		return FALSE;

	if (!value || !value->data || value->size == 0)
		return TRUE;

	if (value->format != 8)
		return TRUE;

	snprintf (str, sizeof(str), "%s", (char*)value->data);

	p = strtok (str, ",");
	return_val_if_fail (p != NULL, FALSE);
	x = atoi (p);

	p = strtok (NULL, ",");
	return_val_if_fail (p != NULL, FALSE);
	y = atoi (p);

	DRVLOG ("%s : offset(%d,%d) \n", __FUNCTION__, x, y);

	fbdevVideoSetOffset (pOutput->scrn, x, y);

	return TRUE;
}


/********************************************************************************************/
/* Implementation of Output entry points */
/********************************************************************************************/
static void lcd_output_create_resources(xf86OutputPtr output)
{
}

static void lcd_output_dpms(xf86OutputPtr output, int mode)
{
	return;
}

static void lcd_output_save(xf86OutputPtr output)
{
	return;
}

static void lcd_output_restore(xf86OutputPtr output)
{
	return;
}

static int lcd_output_mode_valid(xf86OutputPtr output, DisplayModePtr mode)
{
	FBDevPtr pFBDev = FBDEVPTR(output->scrn);

	if(pFBDev->builtin->HDisplay < mode->HDisplay)
		return MODE_HSYNC;

	if(pFBDev->builtin->VDisplay < mode->VDisplay)
		return MODE_VSYNC;

	output->scrn->currentMode = mode;

	return MODE_OK;

}

static Bool lcd_output_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
                                  DisplayModePtr adjusted_mode)
{
	return TRUE;
}

static void lcd_output_prepare(xf86OutputPtr output)
{
	return;
}

static void lcd_output_mode_set(xf86OutputPtr output, DisplayModePtr mode,
                                DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = output ->scrn;
	FBDevPtr pFBDev = FBDEVPTR(pScrn);

	/* have to set the physical size of the output */
	output->mm_width = pFBDev->var.width;
	output->mm_height = pFBDev->var.height;
	output->conf_monitor->mon_width = pFBDev->var.width;
	output ->conf_monitor->mon_height = pFBDev->var.height;

	return;
}

static void lcd_output_commit(xf86OutputPtr output)
{
	return;
}

static xf86OutputStatus lcd_output_detect(xf86OutputPtr output)
{
	return XF86OutputStatusConnected;
}



/**
  * Name : lcd_output_get_modes
  * Input :  xf86OutputPtr
  * Return : DisplayModePtr
  * Description : Query the device for the modes it provides.
     			This function may also update MonInfo, mm_width, and mm_height.
     			return singly-linked list of modes or NULL if no modes found.

  */
static int first_time_rotated = TRUE;
static DisplayModePtr lcd_output_get_modes(xf86OutputPtr output)
{
	FBDevPtr pFBDev = FBDEVPTR(output->scrn);

	/* [soolim:20100205] : if there is no mode name in xorg.conf, use the builtin mode */
	char *preferred_mode;
	preferred_mode = FBDevCheckPreferredMode(output->scrn, output);
	if(!preferred_mode)
	{
		int HDisplay, VDisplay;

		Bool rotated = (pFBDev->rotate & (RR_Rotate_90|RR_Rotate_270)) != 0;

		if(rotated)
		{
			if(first_time_rotated)
			{
				memcpy(&pFBDev->builtin_saved, pFBDev->builtin, sizeof(*pFBDev->builtin));
				VDisplay = pFBDev->builtin->VDisplay;
				HDisplay = pFBDev->builtin->HDisplay;
				pFBDev->builtin->HDisplay = VDisplay;
				pFBDev->builtin->VDisplay = HDisplay;
				pFBDev->builtin->name = "fake_mode";
				first_time_rotated = FALSE;
			}
			return xf86DuplicateMode(pFBDev->builtin);
		}
		else
		{
			return xf86DuplicateMode(pFBDev->builtin);
		}
	}

	/* [soolim:20100205] : gets the supported modes from framebuffer and makes the list */
	DisplayModePtr support_modes = NULL;

	if(!pFBDev->support_modes)
	{
		support_modes = FBDevGetSupportModes(pFBDev->builtin);
		if(pFBDev->builtin != NULL)
		{
			if(!pFBDev->builtin->next)
			{
				support_modes->prev = pFBDev->builtin;
				pFBDev->builtin->next = support_modes;
				pFBDev->builtin->prev = NULL;
			}
		}
		else
		{
			pFBDev->builtin = support_modes;
		}
	}

	return support_modes;
}

static Bool lcd_output_set_property(xf86OutputPtr output, Atom property,
                                    RRPropertyValuePtr value)
{
	if (_lcd_output_prop_video_offset (output, property, value))
		return TRUE;

	return TRUE;
}

static Bool lcd_output_get_property(xf86OutputPtr output, Atom property)
{
	return TRUE;
}

static xf86CrtcPtr lcd_output_crtc_get(xf86OutputPtr output)
{
	return 0;
}

static void lcd_output_destroy(xf86OutputPtr output)
{
	return;
}


/* output funcs */
static const xf86OutputFuncsRec lcd_output_funcs =
{
	.create_resources = lcd_output_create_resources,
	.destroy = lcd_output_destroy,
	.dpms = lcd_output_dpms,
	.save = lcd_output_save,
	.restore = lcd_output_restore,
	.mode_valid = lcd_output_mode_valid,

	.mode_fixup = lcd_output_mode_fixup,
	.prepare = lcd_output_prepare,
	.mode_set = lcd_output_mode_set,
	.commit = lcd_output_commit,
	.detect = lcd_output_detect,
	.get_modes = lcd_output_get_modes,
#ifdef RANDR_12_INTERFACE
	.set_property = lcd_output_set_property,
#endif
#ifdef RANDR_13_INTERFACE	/* not a typo */
	.get_property = lcd_output_get_property,
#endif
#ifdef RANDR_GET_CRTC_INTERFACE
	.get_crtc = lcd_output_crtc_get,
#endif
};

void LcdOutputInit(ScrnInfoPtr pScrn)
{
	xf86OutputPtr	    output;
	FBDevOutputPrivPtr    lcd_output;

	output = xf86OutputCreate (pScrn, &lcd_output_funcs, "LVDS1");
	if (!output)
		return;
	lcd_output = xnfcalloc (sizeof (FBDevOutputPriv), 1);
	if (!lcd_output)
	{
		xf86OutputDestroy (output);
		return;
	}

	output->driver_private = lcd_output;
	output->interlaceAllowed = FALSE;
	output->doubleScanAllowed = FALSE;

}
