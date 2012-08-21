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

/* all driver need this */
#include "xf86.h"
#include "xf86_OSproc.h"

#include "fb.h"
#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"
#include "colormapst.h"
#include "xf86cmap.h"
#include "xf86xv.h"
#include "xf86Crtc.h"

#include "fbdev.h"
#include "fbdevhw.h"
#include "fbdev_hw.h"
#include "fbdev_video.h"
#include "fbdev_crtcconfig.h"
#include "fbdev_dpms.h"
#include "fbdev_event_trace.h"

#include <string.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

/* prototypes */
static const OptionInfoRec * 	FBDevAvailableOptions(int chipid, int busid);
static void	FBDevIdentify(int flags);
static Bool	FBDevProbe(DriverPtr drv, int flags);
static Bool	FBDevPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	FBDevScreenInit(int Index, ScreenPtr pScreen, int argc, char **argv);
static Bool	FBDevEnterVT(int scrnIndex, int flags);
static void	FBDevLeaveVT(int scrnIndex, int flags);
static Bool	FBDevCloseScreen(int scrnIndex, ScreenPtr pScreen);
static void	FBDevSaveCurrent(ScrnInfoPtr pScrn);

/* This DriverRec must be defined in the driver for Xserver to load this driver */
_X_EXPORT DriverRec FBDEV =
{
	FBDEV_VERSION,
	FBDEV_DRIVER_NAME,
	FBDevIdentify,
	FBDevProbe,
	FBDevAvailableOptions,
	NULL,
	0,
	NULL,
};

/* Supported "chipsets" */
static SymTabRec FBDevChipsets[] =
{
	{-1, NULL }
};

/* Supported options */
typedef enum
{
	OPTION_SWCURSOR,
	OPTION_FBDEV
} FBDevOpts;

static const OptionInfoRec FBDevOptions[] =
{
	{ OPTION_SWCURSOR,	"swcursor", OPTV_BOOLEAN,	{0},	FALSE },
	{ OPTION_FBDEV,	"fbdev",    OPTV_STRING,	{0},	FALSE },
	{ -1,		NULL,	    OPTV_NONE,		{0},	FALSE }
};

/* -------------------------------------------------------------------- */


#ifdef XFree86LOADER

MODULESETUPPROTO(FBDevSetup);

static XF86ModuleVersionInfo FBDevVersRec =
{
	"emulfb",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	PACKAGE_VERSION_MAJOR,
	PACKAGE_VERSION_MINOR,
	PACKAGE_VERSION_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	NULL,
	{0,0,0,0}
};

_X_EXPORT XF86ModuleData emulfbModuleData = { &FBDevVersRec, FBDevSetup, NULL };

pointer
FBDevSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	if (!setupDone)
	{
		setupDone = TRUE;
		xf86AddDriver(&FBDEV, module, HaveDriverFuncs);
		return (pointer)1;
	}
	else
	{
		if (errmaj) *errmaj = LDR_ONCEONLY;
		return NULL;
	}
}

#endif /* XFree86LOADER */

static Bool
FBDevGetRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate != NULL)
		return TRUE;

	pScrn->driverPrivate = xnfcalloc(sizeof(FBDevRec), 1);
	return TRUE;
}

static void
FBDevFreeRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate == NULL)
		return;
	free(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

/* -------------------------------------------------------------------- */

static const OptionInfoRec *
FBDevAvailableOptions(int chipid, int busid)
{
	return FBDevOptions;
}

static void
FBDevIdentify(int flags)
{
	xf86PrintChipsets(FBDEV_NAME, "driver for framebuffer", FBDevChipsets);
}

/*
 * Probing the device with the device node, this probing depend on the specific hw.
 * This function just verify whether the display hw is avaliable or not.
 */
static Bool
FBDevHWProbe(struct pci_device * pPci, char *device,char **namep)
{
	return fbdevHWProbe(pPci,device,namep);
}

/* The purpose of this function is to identify all instances of hardware supported
 * by the driver. The probe must find the active device sections that match the driver
 * by calling xf86MatchDevice().
 */
static Bool
FBDevProbe(DriverPtr drv, int flags)
{
	int i;
	ScrnInfoPtr pScrn;
	GDevPtr *devSections;
	int numDevSections;
	char *dev;
	int entity;
	Bool foundScreen = FALSE;

	/* For now, just bail out for PROBE_DETECT. */
	if (flags & PROBE_DETECT)
		return FALSE;

	if ((numDevSections = xf86MatchDevice(FBDEV_DRIVER_NAME, &devSections)) <= 0)
		return FALSE;

	if (!xf86LoadDrvSubModule(drv, "fbdevhw"))
		return FALSE;

	for (i = 0; i < numDevSections; i++)
	{
		dev = xf86FindOptionValue(devSections[i]->options,"fbdev");
		if (FBDevHWProbe(NULL,dev,NULL))
		{
			pScrn = NULL;
			entity = xf86ClaimFbSlot(drv, 0, devSections[i], TRUE);
			pScrn = xf86ConfigFbEntity(pScrn,0,entity, NULL,NULL,NULL,NULL);

			if (pScrn)
			{
				foundScreen = TRUE;

				pScrn->driverVersion = FBDEV_VERSION;
				pScrn->driverName = FBDEV_DRIVER_NAME;
				pScrn->name = FBDEV_NAME;
				pScrn->Probe = FBDevProbe;
				pScrn->PreInit = FBDevPreInit;
				pScrn->ScreenInit = FBDevScreenInit;
				pScrn->SwitchMode = fbdevHWSwitchModeWeak();
				pScrn->AdjustFrame = fbdevHWAdjustFrameWeak();
				pScrn->EnterVT 	= FBDevEnterVT;
				pScrn->LeaveVT = FBDevLeaveVT;
				pScrn->ValidMode = fbdevHWValidModeWeak();

				xf86DrvMsg(pScrn->scrnIndex, X_INFO
				           ,"using %s\n", dev ? dev : "default device");
			}
		}
	}
	free(devSections);

	return foundScreen;
}

/*
 * Return the default depth and bits per pixel.
 * Determine the depth and the bpp supported by hw with the hw color format.
 */
static int
FBDevGetDefaultDepth(ScrnInfoPtr pScrn, int *bitsPerPixel)
{
	int defaultDepth;

	/* finding out the valid default_depth */
	defaultDepth = fbdevHWGetDepth(pScrn,bitsPerPixel);

	/* the default depth is not more than 24 */
	defaultDepth = ((*bitsPerPixel)==32)?24:*bitsPerPixel;

	return defaultDepth;
}

/*
 * Initialize the device Probing the device with the device node,
 * this probing depend on the specific hw.
 * This function just verify whether the display hw is avaliable or not.
 */
static Bool
FBDevHWInit(ScrnInfoPtr pScrn, struct pci_device *pPci, char *device)
{
	/* open device : open the framebuffer device */
	if (!fbdevHWInit(pScrn, NULL, device))
	{
		return FALSE;
	}

	return TRUE;
}

/*
  * DeInitialize the hw
  */
static void
FBDevHWDeInit(ScrnInfoPtr pScrn)
{
	/* close the fd of the fb device ??? */

	return;
}

/*
 * Check the driver option.
 * Set the option flags to the driver private
 */
static void
FBDevCheckDriverOptions(ScrnInfoPtr pScrn)
{
	FBDevPtr pFBDev = FBDEVPTR(pScrn);

	/* sw cursor */
	if (xf86ReturnOptValBool(pFBDev->Options, OPTION_SWCURSOR, FALSE))
		pFBDev->bSWCursorEnbled = TRUE;
}



/*
 * This is called before ScreenInit to probe the screen configuration.
 * The main tasks to do in this funtion are probing, module loading, option handling,
 * card mapping, and Crtcs setup.
 */
static Bool
FBDevPreInit(ScrnInfoPtr pScrn, int flags)
{
	FBDevPtr pFBDev;
	int default_depth, fbbpp;
	char *path;
	Gamma defualt_gamma = {0.0, 0.0, 0.0};
	rgb default_weight = { 0, 0, 0 };
	int flag24;

	if (flags & PROBE_DETECT) return FALSE;

	/* Check the number of entities, and fail if it isn't one. */
	if (pScrn->numEntities != 1)
		return FALSE;

	pScrn->monitor = pScrn->confScreen->monitor;

	/* allocate private */
	FBDevGetRec(pScrn);
	pFBDev = FBDEVPTR(pScrn);

	pFBDev->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

	/* can set the path with fbdev option */
	path = xf86FindOptionValue(pFBDev->pEnt->device->options, "fbdev");

	/* Init HW */
	if(!FBDevHWInit(pScrn,NULL,path))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_Error
		           , "fail to initialize hardware\n");
		goto bail1;
	}

	/* finding out the valid default_depth */
	default_depth = FBDevGetDefaultDepth(pScrn,&fbbpp);

	/* set the depth and the bpp to pScrn */
	flag24 = Support24bppFb | Support32bppFb;
	if (!xf86SetDepthBpp(pScrn, default_depth, default_depth, fbbpp, flag24))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR
		           , "fail to find the depth\n");
		goto bail1;
	}
	xf86PrintDepthBpp(pScrn); /* just print out the depth and the bpp */

	/* color weight */
	if (!xf86SetWeight(pScrn, default_weight, default_weight))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR
		           , "fail to set the color weight of RGB\n");
		goto bail1;
	}

	/* visual init, make a TrueColor, -1 */
	if (!xf86SetDefaultVisual(pScrn, -1))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR
		           , "fail to initialize the default visual\n");
		goto bail1;
	}

	/* set gamma */
	if (!xf86SetGamma(pScrn,defualt_gamma))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR
		           , "fail to set the gamma\n");
		goto bail1;
	}

	pScrn->progClock = TRUE;
	pScrn->rgbBits   = 8;
	pScrn->chipset   = "fbdev";
	pScrn->videoRam  = fbdevHWGetVidmem(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO
	           , "hardware: %s (video memory: %dkB)\n"
	           , fbdevHWGetName(pScrn)
	           , pScrn->videoRam/1024);

	/* Collect all the option flags (fill in pScrn->options) */
	xf86CollectOptions(pScrn, NULL);

	/*
	  * Process the options based on the information S5POptions.
	  * The results are written to pS5P->Options. If all the options
	  * processing is done within this fuction a local variable "options"
	  * can be used instead of pS5P->Options
	  */
	if (!(pFBDev->Options = malloc(sizeof(FBDevOptions))))
		goto bail1;
	memcpy(pFBDev->Options, FBDevOptions, sizeof(FBDevOptions));
	xf86ProcessOptions(pScrn->scrnIndex, pFBDev->pEnt->device->options, pFBDev->Options);

	/* Check with the driver options */
	FBDevCheckDriverOptions(pScrn);

	/* Set the Crtc,  the default Output, and the current Mode */
	xf86DrvMsg(pScrn->scrnIndex, X_INFO
	           , "checking modes against framebuffer device and creating crtc and ouput...\n");
	if(!FBDevCrtcConfigInit(pScrn))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR
		           , "Fail to init the CrtcConfig\n");
		goto bail1;
	}
	FBDevSaveCurrent(pScrn);

	/* TODO::soolim:: re-confirm this condition */
	if(pScrn->currentMode->HDisplay == pFBDev->var.xres_virtual
	        && pScrn->currentMode->VDisplay <= pFBDev->var.yres_virtual)
	{
		pScrn->virtualX = pFBDev->var.xres_virtual;
		pScrn->virtualY = pFBDev->var.yres_virtual;
	}
	else
	{
		pScrn->virtualX = pScrn->currentMode->HDisplay;
		pScrn->virtualY = pScrn->currentMode->VDisplay;
	}
	pScrn->displayWidth = pScrn->virtualX;
	xf86PrintModes(pScrn); /* just print the current mode */

	/* Set display resolution */
	if(pFBDev->var.width && pFBDev->var.height)
	{
		pScrn->monitor->widthmm = pFBDev->var.width;
		pScrn->monitor->heightmm = pFBDev->var.height;
	}
	xf86SetDpi(pScrn, 0, 0);

	/* Load fb submodule */
	if (!xf86LoadSubModule(pScrn, "fb"))
	{
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "fail to load fb module\n");
		goto bail1;
	}

	return TRUE;

bail1:
	FBDevFreeRec(pScrn);
	FBDevHWDeInit(pScrn);
	return FALSE;
}

static void
FBDevAdjustFrame(int scrnIndex, int x, int y, int flags)
{
	fbdevHWAdjustFrame(scrnIndex,x,y,flags);
}


/* Save the hw information */
static void
FBDevSave(ScrnInfoPtr pScrn)
{
	FBDevPtr pFBDev = FBDEVPTR(pScrn);

	FBDevGetVarScreenInfo(fbdevHWGetFD(pScrn), &pFBDev->saved_var);
}

/* Restore the hw information */
static void
FBDevRestore(ScrnInfoPtr pScrn)
{
	FBDevPtr pFBDev = FBDEVPTR(pScrn);

	FBDevSetVarScreenInfo(fbdevHWGetFD(pScrn), &pFBDev->saved_var);
}

/* Save the current hw information */
static void
FBDevSaveCurrent(ScrnInfoPtr pScrn)
{
	FBDevPtr pFBDev = FBDEVPTR(pScrn);

	FBDevGetVarScreenInfo(fbdevHWGetFD(pScrn), &pFBDev->var);
	FBDevGetFixScreenInfo(fbdevHWGetFD(pScrn), &pFBDev->fix);
}

static Bool
FBDevModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
	if (!fbdevHWModeInit(pScrn, mode))
		return FALSE;

	return TRUE;
}

/* Get the address of the framebuffer */
static unsigned char *
FBDevGetFbAddr(ScrnInfoPtr pScrn)
{
	FBDevPtr pFBDev = FBDEVPTR(pScrn);
	return pFBDev->fbstart;
}

static Bool
FBDevScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	FBDevPtr pFBDev = FBDEVPTR(pScrn);
	VisualPtr visual;
	int init_picture = 0;
	unsigned char * pFbAddr;
	pFBDev->rotate = RR_Rotate_0;
	Bool rotated = (pFBDev->rotate & (RR_Rotate_90|RR_Rotate_270)) != 0;

	xf86DrvMsg(scrnIndex,X_INFO,
	           "Infomation of Visual is \n\tbitsPerPixel=%d, depth=%d, defaultVisual=%s\n"
	           "\tmask: %x,%x,%x, offset: %d,%d,%d\n",
	           pScrn->bitsPerPixel,
	           pScrn->depth,
	           xf86GetVisualName(pScrn->defaultVisual),
	           (unsigned int) pScrn->mask.red,
	           (unsigned int) pScrn->mask.green,
	           (unsigned int) pScrn->mask.blue,
	           (int)pScrn->offset.red,
	           (int)pScrn->offset.green,
	           (int)pScrn->offset.blue);

	pFBDev->fbmem = fbdevHWMapVidmem(pScrn); /* mmap memory pointer */
	if (!pFBDev->fbmem)
	{
		xf86DrvMsg(scrnIndex,X_ERROR
		           ,"mapping of video memory  failed\n");
		return FALSE;
	}
	pFBDev->fboff = fbdevHWLinearOffset(pScrn);

	/* save fb information */
	FBDevSave(pScrn);

	/*  set mode and set fb info */
	DisplayModePtr tmpMode;

	if(rotated)
		tmpMode = &pFBDev->builtin_saved;
	else
		tmpMode = pScrn->currentMode;

	if (!FBDevModeInit(pScrn, tmpMode))
	{
		xf86DrvMsg(scrnIndex,X_ERROR
		           ,"mode initialization failed\n");
		return FALSE;
	}

	fbdevHWSaveScreen(pScreen, SCREEN_SAVER_ON);
	FBDevAdjustFrame(scrnIndex,0,0,0);

	FBDevSaveCurrent(pScrn);

	/* mi layer */
	miClearVisualTypes();
	if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits, TrueColor))
	{
		xf86DrvMsg(scrnIndex,X_ERROR
		           ,"visual type setup failed for %d bits per pixel [1]\n"
		           , pScrn->bitsPerPixel);
		return FALSE;
	}
	if (!miSetPixmapDepths())
	{
		xf86DrvMsg(scrnIndex,X_ERROR
		           ,"pixmap depth setup failed\n");
		return FALSE;
	}

	/* set the starting point of the framebuffer */
	pFBDev->fbstart = pFBDev->fbmem + pFBDev->fboff;

	/* Get the screen address */
	pFbAddr = FBDevGetFbAddr(pScrn);

	switch (pScrn->bitsPerPixel)
	{
	case 16:
	case 24:
	case 32:
		if(! fbScreenInit(pScreen, pFbAddr,
		                  pScrn->virtualX, pScrn->virtualY,
		                  pScrn->xDpi, pScrn->yDpi,
		                  pScrn->virtualX, /*Pixel width for framebuffer*/
		                  pScrn->bitsPerPixel))
			return FALSE;

		init_picture = 1;

		break;
	default:
		xf86DrvMsg(scrnIndex, X_ERROR
		           , "internal error: invalid number of bits per pixel (%d) encountered\n"
		           , pScrn->bitsPerPixel);
		break;
	}

	if (pScrn->bitsPerPixel > 8)
	{
		/* Fixup RGB ordering */
		visual = pScreen->visuals + pScreen->numVisuals;
		while (--visual >= pScreen->visuals)
		{
			if ((visual->class | DynamicClass) == DirectColor)
			{
				visual->offsetRed   = pScrn->offset.red;
				visual->offsetGreen = pScrn->offset.green;
				visual->offsetBlue  = pScrn->offset.blue;
				visual->redMask     = pScrn->mask.red;
				visual->greenMask   = pScrn->mask.green;
				visual->blueMask    = pScrn->mask.blue;
			}
		}
	}

	/* must be after RGB ordering fixed */
	if (init_picture && !fbPictureInit(pScreen, NULL, 0))
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING
		           , "Render extension initialisation failed\n");

	/* XVideo Initiailization here */
#ifdef XV
	if (!fbdevVideoInit(pScreen))
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		           "XVideo extention initialization failed\n");
#endif

	xf86SetBlackWhitePixels(pScreen);
	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);

	/* Check whether the SWCURSOR option is enbled */
	if(pFBDev->bSWCursorEnbled)
	{
		/* use software cursor */
		miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
	}
	else
	{
		/* use dummy hw_cursro instead of sw_cursor */
		miDCInitialize(pScreen, xf86GetPointerScreenFuncs());
		xf86DrvMsg(pScrn->scrnIndex, X_INFO
		           , "Initializing HW Cursor\n");

		if(!xf86_cursors_init(pScreen, SEC_CURSOR_W, SEC_CURSOR_H,  (HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
		                      HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
		                      HARDWARE_CURSOR_INVERT_MASK |
		                      HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
		                      HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
		                      HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64 |
		                      HARDWARE_CURSOR_ARGB)))
		{
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR
			           , "Hardware cursor initialization failed\n");
		}
	}

	/* crtc init */
	if (!xf86CrtcScreenInit(pScreen))
		return FALSE;

	/* set the desire mode : set the mode to xf86crtc here */
	xf86SetDesiredModes(pScrn);

	/* colormap */
	if (!miCreateDefColormap(pScreen))
	{
		xf86DrvMsg(scrnIndex, X_ERROR
		           , "internal error: miCreateDefColormap failed \n");
		return FALSE;
	}

	if(!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPaletteWeak(), NULL, CMAP_PALETTED_TRUECOLOR))
		return FALSE;

	xf86DPMSInit(pScreen, FBDevDPMSSet(), 0);
	pScreen->SaveScreen = FBDevSaveScreen();
	pFBDev->isLcdOff = FALSE;

	/* Wrap the current CloseScreen function */
	pFBDev->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = FBDevCloseScreen;

    /* register the event hook */
    fbdevTraceInstallHooks ();

	return TRUE;
}


static Bool
FBDevEnterVT(int scrnIndex, int flags)
{

	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

	xf86DrvMsg(pScrn->scrnIndex, X_INFO
	           , "EnterVT::Hardware state at EnterVT:\n");
	return TRUE;
}

static void
FBDevLeaveVT(int scrnIndex, int flags)
{

	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

	xf86DrvMsg(pScrn->scrnIndex, X_INFO
	           , "LeaveVT::Hardware state at LeaveVT:\n");
}

static Bool
FBDevCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	FBDevPtr pFBDev = FBDEVPTR(pScrn);

    fbdevTraceUninstallHooks ();

	FBDevRestore(pScrn);

	fbdevHWUnmapVidmem(pScrn);
	pScrn->vtSema = FALSE;

	if(!pFBDev->bLockScreen)
		FBDevRestore(pScrn);
	else
		ErrorF("Screen closed but LCD was locked\n");

	FBDevHWDeInit(pScrn);

	pScreen->CreateScreenResources = pFBDev->CreateScreenResources;
	pScreen->CloseScreen = pFBDev->CloseScreen;

	return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}




