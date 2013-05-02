/**************************************************************************

xserver-xorg-video-emulfb

Copyright 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: Boram Park <boram1288.park@samsung.com>

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

#include <pixman.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xv.h>

#include "fbdev.h"
#include "fbdev_util.h"
#include "fbdev_video.h"
#include "fbdev_video_fourcc.h"
#include "fbdev_video_virtual.h"
#include "fbdev_video_v4l2.h"
#include "fbdev_pixman.h"

#include "xv_types.h"

#define BUF_NUM     3

enum
{
	COMP_SRC,
	COMP_OVER,
	COMP_MAX,
};

enum
{
	DATA_TYPE_UI,
	DATA_TYPE_VIDEO,
	DATA_TYPE_MAX,
};

static unsigned int support_formats[] =
{
	FOURCC_RGB32,
};

static XF86VideoEncodingRec dummy_encoding[] =
{
	{ 0, "XV_IMAGE", -1, -1, { 1, 1 } },
	{ 1, "XV_IMAGE", 2560, 2560, { 1, 1 } },
};

static XF86ImageRec images[] =
{
	XVIMAGE_RGB32,
	XVIMAGE_SN12,
	XVIMAGE_ST12,
};

static XF86VideoFormatRec formats[] =
{
	{ 16, TrueColor },
	{ 24, TrueColor },
	{ 32, TrueColor },
};

static XF86AttributeRec attributes[] =
{
	{ 0, 0, 0x7fffffff, "_USER_WM_PORT_ATTRIBUTE_FORMAT" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_CAPTURE" },
	{ 0, 0, DATA_TYPE_MAX, "_USER_WM_PORT_ATTRIBUTE_DATA_TYPE" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF" },
};

typedef enum
{
	PAA_MIN,
	PAA_FORMAT,
	PAA_CAPTURE,
	PAA_DATA_TYPE,
	PAA_STREAMOFF,
	PAA_MAX
} FBDEVPortAttrAtom;

static struct
{
	FBDEVPortAttrAtom  paa;
	const char      *name;
	Atom             atom;
} atoms[] =
{
	{ PAA_FORMAT, "_USER_WM_PORT_ATTRIBUTE_FORMAT", None },
	{ PAA_CAPTURE, "_USER_WM_PORT_ATTRIBUTE_CAPTURE", None },
	{ PAA_DATA_TYPE, "_USER_WM_PORT_ATTRIBUTE_DATA_TYPE", None },
	{ PAA_STREAMOFF, "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF", None },
};

/* FBDEV port information structure */
typedef struct
{
	/* index */
	int     index;

	/* port attribute */
	int     id;
	Bool    capture;

	/* information from outside */
	ScrnInfoPtr pScrn;
	DrawablePtr pDraw;
	RegionPtr   clipBoxes;

	void *black;
	int   black_w;
	int   black_h;

	struct xorg_list link;
} FBDEVPortPriv, *FBDEVPortPrivPtr;

static RESTYPE event_drawable_type;

typedef struct _FBDEVVideoResource
{
	XID            id;
	RESTYPE        type;
	FBDEVPortPrivPtr pPort;
	ScrnInfoPtr    pScrn;
} FBDEVVideoResource;

#define FBDEV_MAX_PORT        1

#define NUM_IMAGES        (sizeof(images) / sizeof(images[0]))
#define NUM_FORMATS       (sizeof(formats) / sizeof(formats[0]))
#define NUM_ATTRIBUTES    (sizeof(attributes) / sizeof(attributes[0]))
#define NUM_ATOMS         (sizeof(atoms) / sizeof(atoms[0]))

static void FBDEVVirtualVideoStop (ScrnInfoPtr pScrn, pointer data, Bool exit);

static PixmapPtr
_fbdevVirtualVideoGetPixmap (DrawablePtr pDraw)
{
	if (pDraw->type == DRAWABLE_WINDOW)
		return pDraw->pScreen->GetWindowPixmap ((WindowPtr) pDraw);
	else
		return (PixmapPtr) pDraw;
}

static Bool
_fbdevVirtualVideoIsSupport (unsigned int id)
{
	int i;

	for (i = 0; i < sizeof (support_formats) / sizeof (unsigned int); i++)
		if (support_formats[i] == id)
			return TRUE;

	return FALSE;
}

static Atom
_fbdevVideoGetPortAtom (FBDEVPortAttrAtom paa)
{
	int i;

	return_val_if_fail (paa > PAA_MIN && paa < PAA_MAX, None);

	for (i = 0; i < NUM_ATOMS; i++)
	{
		if (paa == atoms[i].paa)
		{
			if (atoms[i].atom == None)
				atoms[i].atom = MakeAtom (atoms[i].name,
				                          strlen (atoms[i].name), TRUE);

			return atoms[i].atom;
		}
	}

	ErrorF ("Error: Unknown Port Attribute Name!\n");

	return None;
}

static void
_fbdevVirtualVideoStreamOff (FBDEVPortPrivPtr pPort)
{
	DRVLOG ("STREAM_OFF!\n");

	if (pPort->clipBoxes)
	{
		RegionDestroy (pPort->clipBoxes);
		pPort->clipBoxes = NULL;
	}

	if (pPort->black)
	{
		free (pPort->black);
		pPort->black = NULL;
	}

	pPort->black_w = 0;
	pPort->black_h = 0;

	pPort->pScrn = NULL;
	pPort->pDraw = NULL;
	pPort->capture = FALSE;
	pPort->id = FOURCC_RGB32;
}

static int
_fbdevVirtualVideoPreProcess (ScrnInfoPtr pScrn, FBDEVPortPrivPtr pPort,
                              RegionPtr clipBoxes, DrawablePtr pDraw)
{
	if (pPort->pScrn == pScrn && pPort->pDraw == pDraw &&
	        pPort->clipBoxes && clipBoxes && RegionEqual (pPort->clipBoxes, clipBoxes))
		return Success;

	pPort->pScrn = pScrn;
	pPort->pDraw = pDraw;

	if (clipBoxes)
	{
		if (pPort->clipBoxes)
		{
			RegionDestroy (pPort->clipBoxes);
			pPort->clipBoxes = NULL;
		}

		if (!pPort->clipBoxes)
			pPort->clipBoxes = RegionCreate (NULL, 1);

		return_val_if_fail (pPort->clipBoxes != NULL, BadAlloc);

		RegionCopy (pPort->clipBoxes, clipBoxes);
	}

	DRVLOG ("[%s] pDraw(0x%x, %dx%d). \n", __FUNCTION__, pDraw->id, pDraw->width, pDraw->height);

	return Success;
}

static int
_fbdevVirtualVideoAddDrawableEvent (FBDEVPortPrivPtr pPort)
{
	FBDEVVideoResource *resource;
	void *ptr;
    int ret;

	return_val_if_fail (pPort->pScrn != NULL, BadImplementation);
	return_val_if_fail (pPort->pDraw != NULL, BadImplementation);

	ptr = NULL;
	ret = dixLookupResourceByType (&ptr, pPort->pDraw->id,
	                         event_drawable_type, NULL, DixWriteAccess);
	if (ret == Success && ptr)
		return Success;

	resource = malloc (sizeof (FBDEVVideoResource));
	if (resource == NULL)
		return BadAlloc;

	if (!AddResource (pPort->pDraw->id, event_drawable_type, resource))
	{
		free (resource);
		return BadAlloc;
	}

	DRVLOG ("[%s] id(%ld). \n", __FUNCTION__, pPort->pDraw->id);

	resource->id = pPort->pDraw->id;
	resource->type = event_drawable_type;
	resource->pPort = pPort;
	resource->pScrn = pPort->pScrn;

	return Success;
}

static int
_fbdevVirtualVideoRegisterEventDrawableGone (void *data, XID id)
{
	FBDEVVideoResource *resource = (FBDEVVideoResource*)data;

	DRVLOG ("[%s] id(%ld). \n", __FUNCTION__, id);

	if (!resource)
		return Success;

	if (!resource->pPort || !resource->pScrn)
		return Success;

	resource->pPort->pDraw = NULL;

	FBDEVVirtualVideoStop (resource->pScrn, (pointer)resource->pPort, 1);

	free(resource);

	return Success;
}

static void
_fbdevVideoEnsureBlackBuffer (FBDEVPortPrivPtr pPort, int width, int height)
{
	int i, size, *byte;

	if (pPort->black_w != width || pPort->black_w != height)
	{
		if (pPort->black)
		{
			free (pPort->black);
			pPort->black = NULL;
		}
	}

	if (pPort->black)
		return;

	size = width * height;

	pPort->black = (void*)malloc (size * 4);
	pPort->black_w = width;
	pPort->black_h = height;

	byte = (int*)pPort->black;
	for (i = 0; i < size; i++)
		byte[i] = 0xff000000;
}

static Bool
_fbdevVirtualVideoRegisterEventResourceTypes (void)
{
	event_drawable_type = CreateNewResourceType (_fbdevVirtualVideoRegisterEventDrawableGone,
	                      "Fbdev Virtual Video Drawable");

	if (!event_drawable_type)
		return FALSE;

	return TRUE;
}

static int
_fbdevVirtualVideoCompositeLayers (FBDEVPortPrivPtr pPort)
{
	ScreenPtr pScreen = pPort->pScrn->pScreen;
	PixmapPtr screen_pixmap;
	PixmapPtr pPixmap;
	void **handles = NULL;
	int    handle_cnt = 0;
	void *srcbuf = NULL, *dstbuf = NULL;
	xRectangle src = {0,}, dst = {0,}, pxm = {0,};
	pixman_format_code_t format;
	pixman_op_t op;
	Bool over = FALSE;
	PropertyPtr rotate_prop;
	int rotate = 0;

	screen_pixmap = (*pScreen->GetScreenPixmap) (pScreen);
	return_val_if_fail (screen_pixmap != NULL, BadRequest);
	return_val_if_fail (screen_pixmap->devPrivate.ptr != NULL, BadRequest);

	pPixmap = _fbdevVirtualVideoGetPixmap (pPort->pDraw);
	return_val_if_fail (pPixmap != NULL, BadRequest);
	return_val_if_fail (pPixmap->devPrivate.ptr != NULL, BadRequest);

	dstbuf = calloc (1, screen_pixmap->drawable.width * screen_pixmap->drawable.height * 4);
	pxm.width = screen_pixmap->drawable.width;
	pxm.height = screen_pixmap->drawable.height;

	format = PIXMAN_a8r8g8b8;

	fbdevVideoGetV4l2Handles (pPort->pScrn, &handles, &handle_cnt);
	if (handles)
	{
		int i;
		for (i = 0; i < handle_cnt; i++)
		{
			srcbuf = NULL;
			CLEAR (src);
			CLEAR (dst);

			fbdevVideoGetFBInfo (handles[i], &srcbuf, &dst);
			if (!srcbuf || dst.width == 0 || dst.height == 0)
				continue;
			src = dst;
			src.x = src.y = 0;

			if (!over)
			{
				op = PIXMAN_OP_SRC;
				over = TRUE;
			}
			else
				op = PIXMAN_OP_OVER;

			fbdev_pixman_convert_image (op,
			                            srcbuf, dstbuf,
			                            format, format,
			                            &src, &pxm, &src, &dst,
			                            NULL, 0, 0, 0);
		}

		free (handles);
	}

	srcbuf = screen_pixmap->devPrivate.ptr;
	src = dst = pxm;

	if (!over)
	{
		op = PIXMAN_OP_SRC;
		over = TRUE;
	}
	else
		op = PIXMAN_OP_OVER;

	fbdev_pixman_convert_image (op,
	                            srcbuf, dstbuf,
	                            format, format,
	                            &src, &pxm, &src, &dst,
	                            NULL, 0, 0, 0);


	rotate_prop = fbdev_util_get_window_property (pScreen->root,
	              "_E_ILLUME_ROTATE_ROOT_ANGLE");
	if (rotate_prop)
		rotate = *(int*)rotate_prop->data;

	if (rotate != 0 ||
	        pxm.width != pPort->pDraw->width ||
	        pxm.height != pPort->pDraw->height)
	{
		xRectangle rect;
		int width, height;

		width = pxm.width;
		height = pxm.height;

		if (rotate % 180)
			SWAP (width, height);

		fbdev_util_align_rect (width, height,
		                       pPort->pDraw->width, pPort->pDraw->height,
		                       &rect, FALSE);

		src = pxm;
		pxm.width = pPort->pDraw->width;
		pxm.height = pPort->pDraw->height;
		dst = rect;

		fbdev_pixman_convert_image (PIXMAN_OP_SRC,
		                            dstbuf, pPixmap->devPrivate.ptr,
		                            format, format,
		                            &src, &pxm, &src, &dst,
		                            NULL, rotate, 0, 0);
	}
	else
		memcpy (pPixmap->devPrivate.ptr, dstbuf,
		        pPort->pDraw->width * pPort->pDraw->height * 4);

	free (dstbuf);

	DamageDamageRegion (pPort->pDraw, pPort->clipBoxes);

	return Success;
}

static int
FBDEVVirtualVideoGetPortAttribute (ScrnInfoPtr pScrn,
                                   Atom        attribute,
                                   INT32      *value,
                                   pointer     data)
{
	FBDEVPortPrivPtr pPort = (FBDEVPortPrivPtr) data;

	if (attribute == _fbdevVideoGetPortAtom (PAA_FORMAT))
	{
		*value = pPort->id;
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_CAPTURE))
	{
		*value = pPort->capture;
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_DATA_TYPE))
	{
		*value = DATA_TYPE_UI;
		return Success;
	}

	return BadMatch;
}

static int
FBDEVVirtualVideoSetPortAttribute (ScrnInfoPtr pScrn,
                                   Atom        attribute,
                                   INT32       value,
                                   pointer     data)
{
	FBDEVPortPrivPtr pPort = (FBDEVPortPrivPtr) data;

	if (attribute == _fbdevVideoGetPortAtom (PAA_FORMAT))
	{
		if (!_fbdevVirtualVideoIsSupport ((unsigned int)value))
		{
			DRVLOG ("[%s] id(%c%c%c%c) not supported.\n", __FUNCTION__,
                    value & 0xFF, (value & 0xFF00) >> 8,
		            (value & 0xFF0000) >> 16,  (value & 0xFF000000) >> 24);
			return BadRequest;
		}

		pPort->id = (unsigned int)value;
		DRVLOG ("[%s] id(%d) \n", __FUNCTION__, value);
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_CAPTURE))
	{
		pPort->capture = (value > 0) ? TRUE : FALSE;
		DRVLOG ("[%s] capture(%d) \n", __FUNCTION__, pPort->capture);
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_STREAMOFF))
	{
		DRVLOG ("[%s] STREAM_OFF! \n", __FUNCTION__);
		_fbdevVirtualVideoStreamOff (pPort);
		return Success;
	}

	return BadMatch;
}

/* vid_w, vid_h : no meaning for us. not using.
 * dst_w, dst_h : size to hope for PutStill.
 * p_w, p_h     : real size for PutStill.
 */
static void
FBDEVVirtualVideoQueryBestSize (ScrnInfoPtr pScrn,
                                Bool motion,
                                short vid_w, short vid_h,
                                short dst_w, short dst_h,
                                unsigned int *p_w, unsigned int *p_h,
                                pointer data)
{
	if (p_w)
		*p_w = (unsigned int)dst_w;
	if (p_h)
		*p_h = (unsigned int)dst_h;
}

/* vid_x, vid_y, vid_w, vid_h : no meaning for us. not using.
 * drw_x, drw_y, dst_w, dst_h : no meaning for us. not using.
 * Only pDraw's size is used.
 */
static int
FBDEVVirtualVideoPutStill (ScrnInfoPtr pScrn,
                           short vid_x, short vid_y, short drw_x, short drw_y,
                           short vid_w, short vid_h, short drw_w, short drw_h,
                           RegionPtr clipBoxes, pointer data, DrawablePtr pDraw )
{
	FBDEVPortPrivPtr pPort = (FBDEVPortPrivPtr) data;
	int ret = BadRequest;
	PixmapPtr pPixmap;

	_fbdevVideoEnsureBlackBuffer (pPort, pDraw->width, pDraw->height);

	goto_if_fail (pPort->id > 0, get_still_fail);

	ret = _fbdevVirtualVideoPreProcess (pScrn, pPort, clipBoxes, pDraw);
	goto_if_fail (ret == Success, get_still_fail);

	ret = _fbdevVirtualVideoAddDrawableEvent (pPort);
	goto_if_fail (ret == Success, get_still_fail);

	if (!pPort->capture)
	{
		DRVLOG ("[%s] emulfb supports only capture mode. \n", __FUNCTION__);
		goto get_still_fail;
	}

	/* check drawable */
	return_val_if_fail (pDraw->type == DRAWABLE_PIXMAP, BadPixmap);

	ret = _fbdevVirtualVideoCompositeLayers (pPort);
	goto_if_fail (ret == Success, get_still_fail);

	return Success;

get_still_fail:
	pPixmap = _fbdevVirtualVideoGetPixmap (pDraw);

	if (pPixmap->devPrivate.ptr && pPort->black)
		memcpy (pPixmap->devPrivate.ptr, pPort->black,
		        pPort->pDraw->width * pPort->pDraw->height * 4);

	DamageDamageRegion (pPort->pDraw, clipBoxes);

	return ret;
}

static void
FBDEVVirtualVideoStop (ScrnInfoPtr pScrn, pointer data, Bool exit)
{
	FBDEVPortPrivPtr pPort = (FBDEVPortPrivPtr) data;

	_fbdevVirtualVideoStreamOff (pPort);
}

XF86VideoAdaptorPtr
fbdevVideoSetupVirtualVideo (ScreenPtr pScreen)
{
	XF86VideoAdaptorPtr pAdaptor;
	FBDEVPortPrivPtr pPort;
	int i;

	pAdaptor = calloc (1, sizeof (XF86VideoAdaptorRec) +
	                   (sizeof (DevUnion) + sizeof (FBDEVPortPriv)) * FBDEV_MAX_PORT);
	if (!pAdaptor)
		return NULL;

	dummy_encoding[0].width = pScreen->width;
	dummy_encoding[0].height = pScreen->height;

	pAdaptor->type = XvWindowMask | XvPixmapMask | XvInputMask | XvStillMask;
	pAdaptor->flags = 0;
	pAdaptor->name = "FBDEV Virtual Video";
	pAdaptor->nEncodings = sizeof (dummy_encoding) / sizeof (XF86VideoEncodingRec);
	pAdaptor->pEncodings = dummy_encoding;
	pAdaptor->nFormats = NUM_FORMATS;
	pAdaptor->pFormats = formats;
	pAdaptor->nPorts = FBDEV_MAX_PORT;
	pAdaptor->pPortPrivates = (DevUnion*)(&pAdaptor[1]);

	pPort = (FBDEVPortPrivPtr) (&pAdaptor->pPortPrivates[FBDEV_MAX_PORT]);

	for (i = 0; i < FBDEV_MAX_PORT; i++)
	{
		pAdaptor->pPortPrivates[i].ptr = &pPort[i];
		pPort[i].index = i;
		pPort[i].id = FOURCC_RGB32;
	}

	pAdaptor->nAttributes = NUM_ATTRIBUTES;
	pAdaptor->pAttributes = attributes;
	pAdaptor->nImages = NUM_IMAGES;
	pAdaptor->pImages = images;

	pAdaptor->GetPortAttribute     = FBDEVVirtualVideoGetPortAttribute;
	pAdaptor->SetPortAttribute     = FBDEVVirtualVideoSetPortAttribute;
	pAdaptor->QueryBestSize        = FBDEVVirtualVideoQueryBestSize;
	pAdaptor->PutStill             = FBDEVVirtualVideoPutStill;
	pAdaptor->StopVideo            = FBDEVVirtualVideoStop;

	if (!_fbdevVirtualVideoRegisterEventResourceTypes ())
	{
		ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
		xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "Failed to register EventResourceTypes. \n");
		return NULL;
	}

	return pAdaptor;
}
