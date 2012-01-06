/*
 * Copyright 2004 Keith Packard
 * Copyright 2005 Eric Anholt
 * Copyright 2006 Nokia Corporation
 * Copyright 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.
 *
 * Contact: YoungHoon Jung <yhoon.jung@samsung.com>
 *
 * Permission to use, copy, modify, distribute and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of the authors and/or copyright holders
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  The authors and
 * copyright holders make no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without any express
 * or implied warranty.
 *
 * THE AUTHORS AND COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <X11/Xatom.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvproto.h>
#include "fourcc.h"

#include "fbdevhw.h"
#include "damage.h"

#include "xf86xv.h"

#include "fbdev.h"

#include "v4l2api.h"

#include "fbdev_video.h"
#include "fbdev_hw.h"

#include "xv_types.h"

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

#ifndef max
#define max(x, y) (((x) >= (y)) ? (x) : (y))
#endif

#define ENSURE_AREA(offset, length, max) length = (offset + length > max ? max - offset : length)

#define SWAP(x, y, t) t = x, x = y, y = t

static XF86VideoEncodingRec DummyEncoding[] =
{
	/* Max width and height are filled in later. */
	{ 0, "XV_IMAGE", -1, -1, { 1, 1 } },
	{ 1, "XV_IMAGE", 2560, 2560, { 1, 1 } },
};

#define FOURCC_RGB565 0x50424742
#define XVIMAGE_RGB565 \
   { \
    FOURCC_RGB565, \
    XvRGB, \
    LSBFirst, \
    {'R','G','B','P', \
        0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    16, \
    XvPacked, \
    1, \
    16, 0x0000F800, 0x000007E0, 0x0000001F, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, \
    {'R','G','B',0, \
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

#define FOURCC_RGB32 0x34424752
#define XVIMAGE_RGB32 \
   { \
    FOURCC_RGB32, \
    XvRGB, \
    LSBFirst, \
    {'R','G','B','4', \
        0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
    32, \
    XvPlanar, \
    1, \
    24, 0x00FF0000, 0x0000FF00, 0x000000FF, \
    0, 0, 0, 0, 0, 0, 0, 0, 0, \
    {'X','R','G','B', \
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
    XvTopToBottom \
   }

static XF86ImageRec Images[] =
{
	XVIMAGE_I420,
	XVIMAGE_YV12,
	XVIMAGE_YUY2,
	XVIMAGE_RGB32,
	XVIMAGE_RGB565,
};

#define NUM_IMAGES (sizeof(Images) / sizeof(Images[0]))

static XF86VideoFormatRec Formats[] =
{
	{ 16, TrueColor },
	{ 24, TrueColor },
	{ 32, TrueColor },
};

#define NUM_FORMATS (sizeof(Formats) / sizeof(Formats[0]))

static XF86AttributeRec Attributes[] =
{
	{ 0, -1, 270, "_USER_WM_PORT_ATTRIBUTE_ROTATION" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_HFLIP" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_VFLIP" },
	{ 0, -1, 1, "_USER_WM_PORT_ATTRIBUTE_PREEMPTION" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_DRAWING_MODE" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF" },
};

#define NUM_ATTRIBUTES (sizeof(Attributes) / sizeof(Attributes[0]))
#define FBDEV_MAX_PORT    32

static void
fbdevGetRotation(ScreenPtr pScreen, FBDevPortPrivPtr pPortPriv, DrawablePtr pDraw, int * rotation, int * screen_rotation, int * xres, int * yres)
{
	ScrnInfoPtr pScrnInfo = xf86Screens[pScreen->myNum];
	FBDevPtr pFBDev = FBDEVPTR(pScrnInfo);

	*rotation = 0;
	*screen_rotation = 0;

#ifdef RANDR
	switch(pFBDev->rotate)
	{
	case RR_Rotate_270:
		*screen_rotation += 90;
	case RR_Rotate_180:
		*screen_rotation += 90;
	case RR_Rotate_90:
		*screen_rotation += 90;
	case RR_Rotate_0:
		break;
	}
#endif

	if (pPortPriv->rotation >= 0)
		*rotation = pPortPriv->rotation;
	*screen_rotation = (*rotation - *screen_rotation + 360) % 360;
}

typedef enum { _PAA_MIN, PAA_ROTATION, PAA_HFLIP, PAA_VFLIP, PAA_PREEMPTION, PAA_DRAWINGMODE, PAA_STREAMOFF, _PAA_MAX } PORT_ATTR_ATOM;
static struct
{
	PORT_ATTR_ATOM paa;
	const char * name;
	Atom atom;
} atom_list[] =
{
	{ PAA_ROTATION, "_USER_WM_PORT_ATTRIBUTE_ROTATION", None },
	{ PAA_HFLIP, "_USER_WM_PORT_ATTRIBUTE_HFLIP", None },
	{ PAA_VFLIP, "_USER_WM_PORT_ATTRIBUTE_VFLIP", None },
	{ PAA_PREEMPTION, "_USER_WM_PORT_ATTRIBUTE_PREEMPTION", None },
	{ PAA_DRAWINGMODE, "_USER_WM_PORT_ATTRIBUTE_DRAWING_MODE", None },
	{ PAA_STREAMOFF, "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF", None },
};

static Atom
getPortAtom(PORT_ATTR_ATOM paa)
{
	int i;
	static int n_atom_cnt = sizeof(atom_list) / sizeof(atom_list[0]);

	if (paa <= _PAA_MIN || paa >= _PAA_MAX)
	{
		ErrorF("Error: Invalid Port Attribute Name!\n");
		return None;
	}
	for (i=0; i<n_atom_cnt; i++)
	{
		if (paa == atom_list[i].paa)
		{
			if (atom_list[i].atom == None)
				atom_list[i].atom = MakeAtom (atom_list[i].name , strlen (atom_list[i].name), TRUE);

			return atom_list[i].atom;
		}
	}

	ErrorF("Error: Unknown Port Attribute Name!\n");
	return None;
}

/**
 * Xv attributes get/set support.
 */
static int
fbdevGetPortAttribute(ScrnInfoPtr pScrnInfo, Atom attribute, INT32 *value,
                      pointer data)
{
	FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) data;

	if (attribute == getPortAtom(PAA_ROTATION))
	{
		*value = pPortPriv->rotation;
		return Success;
	}
	else if (attribute == getPortAtom (PAA_HFLIP))
	{
		*value = pPortPriv->hflip;
		return Success;
	}
	else if (attribute == getPortAtom (PAA_VFLIP))
	{
		*value = pPortPriv->vflip;
		return Success;
	}
	else if (attribute == getPortAtom(PAA_PREEMPTION))
	{
		*value = pPortPriv->preemption;
		return Success;
	}
	else if (attribute == getPortAtom(PAA_DRAWINGMODE))
	{
		*value = (pPortPriv->mode == MODE_VIRTUAL);
		return Success;
	}
	return BadMatch;
}

static int
fbdevSetPortAttribute(ScrnInfoPtr pScrnInfo, Atom attribute, INT32 value,
                      pointer data)
{
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;
	FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) data;

	if (attribute == getPortAtom(PAA_ROTATION))
	{
		pPortPriv->rotation = value;
		return Success;
	}
	else if (attribute == getPortAtom (PAA_HFLIP))
	{
		pPortPriv->hflip = value;
		return Success;
	}
	else if (attribute == getPortAtom (PAA_VFLIP))
	{
		pPortPriv->vflip = value;
		return Success;
	}
	else if (attribute == getPortAtom(PAA_PREEMPTION))
	{
		pPortPriv->preemption = value;
		return Success;
	}
	else if (attribute == getPortAtom(PAA_STREAMOFF))
	{
		DeleteDisplay(pPortPriv->v4l2_handle, 0);
		pPortPriv->v4l2_handle = NULL;
		pFBDev->v4l2_owner[pPortPriv->v4l2_index] = NULL;
		return Success;
	}
	return BadMatch;
}

/**
 * Clip the image size to the visible screen.
 */
static void
fbdevQueryBestSize(ScrnInfoPtr pScrnInfo, Bool motion, short vid_w,
                   short vid_h, short dst_w, short dst_h,
                   unsigned int *p_w, unsigned int *p_h, pointer data)
{
	ScreenPtr pScreen = pScrnInfo->pScreen;

	if (dst_w < pScreen->width)
		*p_w = dst_w;
	else
		*p_w = pScreen->width;

	if (dst_h < pScreen->height)
		*p_h = dst_h;
	else
		*p_h = pScreen->height;
}

/**
 * Stop an overlay.  exit is whether or not the client's exiting.
 */
void
fbdevVideoStop(ScrnInfoPtr pScrnInfo, pointer data, Bool exit)
{
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;

	FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) data;

	if (pPortPriv->mode == MODE_V4L2)
	{
		if (pPortPriv->v4l2_handle && !DeleteDisplay(pPortPriv->v4l2_handle, 0))
		{
		}

		pPortPriv->v4l2_handle = NULL;
		pFBDev->v4l2_owner[pPortPriv->v4l2_index] = NULL;

		if (pFBDev->bFbAlphaEnabled)
		{
			FBDevScreenAlphaDeinit(fbdevHWGetFD(pScrnInfo));
			pFBDev->bFbAlphaEnabled = FALSE;
		}
	}
	pPortPriv->mode = MODE_INIT;
	pPortPriv->preemption = 0;
	pPortPriv->rotation = -1;

	if (exit)
	{
		REGION_EMPTY(pScrnInfo, &pPortPriv->clip);
	}
}

/**
 * ReputImage hook.  We always fail here if we're mid-migration or
 * on Hailstorm; we want stopped video to actually be _stopped_, due
 * to Hailstorm limitations.
 */
static int
fbdevReputImage( ScrnInfoPtr pScrnInfo, short drw_x, short drw_y,
                 RegionPtr clipBoxes, pointer data, DrawablePtr pDraw )

{
	return Success;
}

/**
 * Give image size and pitches.
 */
static int
fbdevQueryImageAttributes(ScrnInfoPtr pScrnInfo, int id, unsigned short *w,
                          unsigned short *h, int *pitches, int *offsets)
{
//    FbdevLog("FBDEV", LOGLEVEL_DEBUG, "%s (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);
	int size = 0, tmp = 0;

	*w = (*w + 1) & ~1;
	if (offsets)
		offsets[0] = 0;

	switch (id)
	{
	case FOURCC_RGB565:
		size += (*w << 1);
		if (pitches)
			pitches[0] = size;
		size *= *h;
		break;
	case FOURCC_RGB32:
		size += (*w << 2);
		if (pitches)
			pitches[0] = size;
		size *= *h;
		break;
	case FOURCC_I420:
	case FOURCC_YV12:
		size = *w;
		if (pitches)
			pitches[0] = size;

		size *= *h;
		if (offsets)
			offsets[1] = size;

		tmp = (*w >> 1);
		if (pitches)
			pitches[1] = pitches[2] = tmp;

		tmp *= (*h >> 1);
		size += tmp;
		if (offsets)
			offsets[2] = size;

		size += tmp;
		break;
	case FOURCC_UYVY:
	case FOURCC_YUY2:
		size = *w << 1;
		if (pitches)
			pitches[0] = size;

		size *= *h;
		break;
	default:
		return BadIDChoice;
	}

	return size;
}

#define DONT_FILL_ALPHA -1

static void
fbdevDisplayVideo (PixmapPtr pPixmap, RegionPtr pRgn, unsigned char *buf, int src_x, int src_y, int width, int height, int alpha)
{
	xRectangle    *rects, *r;
	int bpp;         //byte per pixel;
	BoxPtr    pBox = REGION_RECTS (pRgn);
	int        nBox = REGION_NUM_RECTS (pRgn);
	int i, j;

	unsigned char *pDst, *pSrc;
	int offset_x=0, offset_y=0;
#ifdef COMPOSITE
	offset_x = pPixmap->screen_x;
	offset_y = pPixmap->screen_y;
#endif

	rects = malloc (nBox * sizeof (xRectangle));
	if (!rects)
		goto bail0;
	r = rects;

	bpp = pPixmap->drawable.bitsPerPixel/8;

	if (bpp != 4)
		alpha = DONT_FILL_ALPHA;

	while (nBox--)
	{
		r->x = pBox->x1 + pPixmap->drawable.x;
		r->y = pBox->y1 + pPixmap->drawable.y;
		r->width = pBox->x2 - pBox->x1;
		r->height = pBox->y2 - pBox->y1;

		if (buf)
		{
			pDst = (unsigned char*)pPixmap->devPrivate.ptr + ((r->y-offset_y) * pPixmap->devKind) + ((r->x-offset_x)*bpp);
			pSrc = buf + (r->y - pRgn->extents.y1 + src_y)*width*bpp + (r->x - pRgn->extents.x1 + src_x)*bpp;

			for(i=0; i<r->height; i++, pDst+=pPixmap->devKind, pSrc+=width*bpp)
			{
				memcpy(pDst, pSrc, r->width*bpp);
			}
		}

		if (alpha != DONT_FILL_ALPHA)
		{
			pDst = (unsigned char*)pPixmap->devPrivate.ptr + ((r->y-offset_y) * pPixmap->devKind) + ((r->x-offset_x)*bpp);
			for(i=0; i<r->height; i++, pDst+=pPixmap->devKind)
				for(j=0; j<r->width; j++)
					pDst[j*bpp + 3] = alpha;
		}

		r++;
		pBox++;
	}

	free (rects);
bail0:
	;
}

static int
fbdevTranslateV4L2toFourCC(V4L2Videoformat format)
{
	switch (format)
	{
	case V4L2_VIDEO_FMT_I420:
		return FOURCC_I420;
	case V4L2_VIDEO_FMT_YUY2:
		return FOURCC_YUY2;
	case V4L2_VIDEO_FMT_YV12:
		return FOURCC_YV12;
	case V4L2_VIDEO_FMT_RGB32:
		return FOURCC_RGB32;
	case V4L2_VIDEO_FMT_RGB565:
		return FOURCC_RGB565;
	default:
		return 0;
	}
}

static int
intersectXRectangles ( xRectangle * dest, xRectangle * rect1, xRectangle * rect2)
{
	if (dest == NULL || rect1 == NULL || rect2 == NULL)
		return -1;

	dest->x = rect1->x > rect2->x ? rect1->x : rect2->x;
	dest->y = rect1->y > rect2->y ? rect1->y : rect2->y;
	dest->width = (rect1->x + rect1->width < rect2->x + rect2->width ? rect1->x + rect1->width : rect2->x + rect2->width) - dest->x;
	dest->height = (rect1->y + rect1->height < rect2->y + rect2->height ? rect1->y + rect1->height : rect2->y + rect2->height) - dest->y;

	if (dest->width <= 0 || dest->height <= 0)
		return 0;

	return 1;
}

static int
fbdevPutStill ( ScrnInfoPtr pScrnInfo,
                short vid_x, short vid_y, short drw_x, short drw_y,
                short vid_w, short vid_h, short drw_w, short drw_h,
                RegionPtr clipBoxes, pointer data, DrawablePtr pDraw )
{
	int ret = Success;
	int rotation = 0;
	unsigned short width, height;
	V4L2Videoformat format;
	FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) data;

	if (pPortPriv->mode == MODE_INIT)
		return Success;
	if (pPortPriv->mode == MODE_V4L2 && pPortPriv->v4l2_handle == NULL)
		return BadImplementation;

	if (pPortPriv->mode == MODE_VIRTUAL)
		return Success;

	void * captured_image = GetCapture(pPortPriv->v4l2_handle, &width, &height,  &rotation, &format, FALSE);
	PixmapPtr pPixmap;

	ScreenPtr pScreen = pScrnInfo->pScreen;
	if (pDraw->type == DRAWABLE_WINDOW)
		pPixmap =
		    (*pScreen->GetWindowPixmap)((WindowPtr) pDraw);
	else
		pPixmap = (PixmapPtr) pDraw;
	int		bpp = pPixmap->drawable.bitsPerPixel/8;

	ENSURE_AREA(vid_x, vid_w, pDraw->width);
	ENSURE_AREA(vid_y, vid_h, pDraw->height);

	xRectangle ResultRect;
	xRectangle vid = { vid_x, vid_y, vid_w, vid_h };

	if (intersectXRectangles(&ResultRect, &vid, &pPortPriv->dst) <= 0)
		return Success;

	int x1, y1;
	unsigned short w1, h1;
	x1 = (ResultRect.x - pPortPriv->dst.x) * width / pPortPriv->dst.width;
	y1 = (ResultRect.y - pPortPriv->dst.y) * height / pPortPriv->dst.height;
	w1 = ResultRect.width * width / pPortPriv->dst.width;
	h1 = ResultRect.height * height / pPortPriv->dst.height;

	int x2, y2;
	unsigned short w2, h2;
	x2 = ResultRect.x * drw_w / vid.width;
	y2 = ResultRect.y * drw_h / vid.height;
	w2 = ResultRect.width * drw_w / vid.width;
	h2 = ResultRect.height * drw_h / vid.height;

	void * pToBeFreed[8] = { NULL, };
	int nToBeFreed = 0;

	void * src_buf = NULL;
	void * dst_buf = NULL;

	int pitches[3], offsets[3];

	int src_format, dst_format;
	src_format = fbdevTranslateV4L2toFourCC(format);

	dst_format = 	FOURCC_RGB32;

	src_buf = captured_image;

	if ( (x1 || y1) || (w1 != width) || (h1 != height) )
	{
		int size = fbdevQueryImageAttributes(NULL, src_format, &w1, &h1, NULL, NULL);
		pToBeFreed[nToBeFreed++] = dst_buf = malloc(size);

		fbdevQueryImageAttributes(NULL, src_format, &width, &height, pitches, offsets);
		imgcpy(w1, h1, dst_buf, 0, 0, w1, h1,
		       src_buf, x1, y1, width, height,
		       pitches, offsets, src_format == FOURCC_I420 ? 3 : 1);

		src_buf = dst_buf;
	}

	if ( (w1 != w2) || (h1 != h2) || (src_format != dst_format) )
	{
		pToBeFreed[nToBeFreed++] = dst_buf = malloc(w2 * h2 * bpp);
		src_buf = dst_buf;
	}

	if ( (x2 || y2) || (w2 != drw_w) || (h2 != drw_h) )
	{
		pitches[0] = w2 * bpp;
		pToBeFreed[nToBeFreed++] = dst_buf = calloc(drw_w * drw_h, bpp);
		imgcpy(w2, h2, dst_buf, x2, y2, drw_w, drw_h,
		       src_buf, 0, 0, w2, h2,
		       pitches, NULL, 1);

		src_buf = dst_buf;
	}

	fbdevDisplayVideo (pPixmap, clipBoxes, src_buf, 0, 0, drw_w, drw_h, 0xFF);

	DamageDamageRegion(pDraw, clipBoxes);

	return ret;
}

static void cropLine(int * src, int * length, const int crop, const int crop_length)
{
	if (*src < crop)
	{
		*length = *length - (crop - *src);
		*src = crop;
	}
	if (*src + *length > crop + crop_length)
		*length = crop + crop_length - *src;
}

static void cropBox(int * src_x, int * src_y, int * src_w, int * src_h, const int crop_x, const int crop_y, const int crop_w, const int crop_h)
{
	cropLine(src_x, src_w, crop_x, crop_w);
	cropLine(src_y, src_h, crop_y, crop_h);
}

static void subtractBox(int * src_x, int * src_y, int * src_w, int * src_h, const int margin_x1, const int margin_x2, const int margin_y1, const int margin_y2)
{
	*src_x += margin_x1;
	*src_w -= margin_x1 + margin_x2;
	*src_y += margin_y1;
	*src_h -= margin_y1 + margin_y2;
}

static void
cropWindow(int xres, int yres, int src_x, int src_y, int src_w, int src_h, int dst_x, int dst_y, int dst_w, int dst_h, CRECT * crop, CRECT * win, int rotation)
{
	int temp;

	int dstX = dst_x;
	int dstY = dst_y;
	int dstW = dst_w;
	int dstH = dst_h;

	int marginX1, marginX2, marginY1, marginY2;

	cropBox(&dst_x, &dst_y, &dst_w, &dst_h, 0, 0, xres, yres);

	win->x = dst_x, win->y = dst_y, win->w = dst_w, win->h = dst_h;

	marginX1 = dst_x - dstX;
	marginX2 = (dstX + dstW) - (dst_x + dst_w);
	marginY1 = dst_y - dstY;
	marginY2 = (dstY + dstH) - (dst_y + dst_h);

	switch(rotation)
	{
	case 90:
		temp = marginX1;
		marginX1 = marginY1;
		marginY1 = marginX2;
		marginX2 = marginY2;
		marginY2 = temp;
		temp = dstW;
		dstW = dstH;
		dstH = temp;
		break;
	case 180:
		temp = marginX1;
		marginX1 = marginX2;
		marginX2 = temp;
		temp = marginY1;
		marginY1 = marginY2;
		marginY2 = temp;
		break;
	case 270:
		temp = marginX2;
		marginX2 = marginY1;
		marginY1 = marginX1;
		marginX1 = marginY2;
		marginY2 = temp;
		temp = dstW;
		dstW = dstH;
		dstH = temp;
		break;
	}

	marginX1 = marginX1 * src_w / dstW;
	marginX2 = marginX2 * src_w / dstW;
	marginY1 = marginY1 * src_h / dstH;
	marginY2 = marginY2 * src_h / dstH;

	subtractBox(&src_x, &src_y, &src_w, &src_h, marginX1, marginX2, marginY1, marginY2);

	crop->x = src_x;
	crop->y = src_y;
	crop->w = src_w;
	crop->h = src_h;
}

typedef struct
{
	int fourcc;
	V4L2Videoformat v4l2_format;
	Bool useDirectHWBuf;
} FORMAT_TYPE;

static FORMAT_TYPE format_table[] =
{
	{ FOURCC_RGB565,	V4L2_VIDEO_FMT_RGB565,	FALSE },
	{ FOURCC_RGB32,	V4L2_VIDEO_FMT_RGB32,	FALSE },
	{ FOURCC_I420,	V4L2_VIDEO_FMT_I420,	FALSE },
	{ FOURCC_YUY2,	V4L2_VIDEO_FMT_YUY2,	FALSE },
	{ FOURCC_YV12,	V4L2_VIDEO_FMT_YV12,	FALSE },
};

static FORMAT_TYPE *
getFormatInfo(int fourcc)
{
	int i;
	static int size = sizeof(format_table) / sizeof(FORMAT_TYPE);

	for (i = 0; i < size; i++)
	{
		if (format_table[i].fourcc == fourcc)
			return &format_table[i];
	}

	ErrorF ("Emulator doens't support the '%c%c%c%c' type of image. \n",
		fourcc & 0xFF, (fourcc & 0xFF00) >> 8, (fourcc & 0xFF0000) >> 16,  (fourcc & 0xFF000000) >> 24);

	return NULL;
}

static int
parseFormatBuffer(unsigned char * buf, unsigned int * phy_addrs)
{
	XV_PUTIMAGE_DATA_PTR data = (XV_PUTIMAGE_DATA_PTR) buf;

	int valid = XV_PUTIMAGE_VALIDATE_DATA(data);
	if (valid < 0)
		return valid;

	if (phy_addrs == NULL)
		return -0x10;

	phy_addrs[0] = data->YPhyAddr;
	phy_addrs[1] = data->CbPhyAddr;
	phy_addrs[2] = data->CrPhyAddr;

	return 0;
}

static int
fbdevPutImage( ScrnInfoPtr pScrnInfo,
               short src_x, short src_y, short dst_x, short dst_y,
               short src_w, short src_h, short dst_w, short dst_h,
               int id, unsigned char* buf, short width, short height, Bool sync,
               RegionPtr clip_boxes, pointer data, DrawablePtr pDraw )
{
	ScreenPtr pScreen = pScrnInfo->pScreen;
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;

	FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) data;

	FBDEV_PORT_MODE modeBefore = pPortPriv->mode;

	pPortPriv->pDraw = pDraw;
	int rotation, screen_rotation;
	int v4l2_index = 0;

	V4L2Videoformat format;
	int nBuffer = 1;

	int pitches[3], offsets[3];
	unsigned short w = width, h = height;
	PixmapPtr  pPixmap;

	CRECT crop = {src_x, src_y, src_w, src_h};
	CRECT win = {dst_x, dst_y, dst_w, dst_h};

	int xres = pScreen->width, yres = pScreen->height;

	FORMAT_TYPE * format_info = getFormatInfo(id);

	unsigned int phy_addrs[4];
	if (format_info == NULL)
		return BadRequest;

	if (pDraw->type == DRAWABLE_WINDOW)
		pPixmap = (*pScreen->GetWindowPixmap) ((WindowPtr) pDraw);
	else
		pPixmap = (PixmapPtr) pDraw;

	if (!pPortPriv->v4l2_handle)
	{
		pPortPriv->mode = MODE_VIRTUAL;

		if (pPortPriv->preemption == 0)
		{
			int full = TRUE;
			for (v4l2_index = 0; v4l2_index < pFBDev->v4l2_num; v4l2_index++)
				if (AvailableDisplay (v4l2_index))
				{
					full = FALSE;
					break;
				}

			if (full)
				pPortPriv->mode = MODE_VIRTUAL;
			else
				pPortPriv->mode = MODE_V4L2;
		}
		else if (pPortPriv->preemption == 1)
		{
			int can_create = FALSE;
			for (v4l2_index = 0; v4l2_index < pFBDev->v4l2_num; v4l2_index++)
			{
				FBDevPortPrivPtr pOwnerPort = (FBDevPortPrivPtr)pFBDev->v4l2_owner[v4l2_index];
				if (!pOwnerPort)
				{
					can_create = TRUE;
					pPortPriv->mode = MODE_V4L2;
					break;
				}
				else if (pOwnerPort->preemption == 0)
				{
					can_create = TRUE;
					fbdevPutStill (pScrnInfo,
					               pOwnerPort->dst.x, pOwnerPort->dst.y, pOwnerPort->dst.x, pOwnerPort->dst.y,
					               pOwnerPort->dst.width, pOwnerPort->dst.height, pOwnerPort->dst.width, pOwnerPort->dst.height,
					               &pOwnerPort->clipBoxes, pOwnerPort, pOwnerPort->pDraw);

					DeleteDisplay(pOwnerPort->v4l2_handle, 0);
					pOwnerPort->v4l2_handle = NULL;
					pFBDev->v4l2_owner[v4l2_index] = NULL;

					pOwnerPort->mode = MODE_VIRTUAL;
					pPortPriv->mode = MODE_V4L2;
					break;
				}
			}

			if (!can_create)
			{
				return BadRequest;
			}
		}
	}

	fbdevGetRotation(pScrnInfo->pScreen, pPortPriv, pDraw, &rotation, &screen_rotation, &xres, &yres);

	if (pPortPriv->mode == MODE_V4L2)
	{
		if (format_info->useDirectHWBuf)
		{
			int ret;
			if ((ret = parseFormatBuffer(buf, phy_addrs)) < 0)
			{
				if (ret == XV_HEADER_ERROR)
					ErrorF("XV_HEADER_ERROR\n");
				else if (ret == XV_VERSION_MISMATCH)
					ErrorF("XV_VERSION_MISMATCH\n");
				if (ret == -0x10)
					ErrorF("phy_addrs_ERROR\n");

				return BadRequest;
			}

			/* Skip frame */
			if (phy_addrs[0] == 0)
				return Success;
		}

		cropWindow(xres, yres, src_x, src_y, src_w, src_h, dst_x, dst_y, dst_w, dst_h, &crop, &win, rotation);

		if (!format_info->useDirectHWBuf)
		{
			src_x = crop.x;
			src_y = crop.y;
			src_w = crop.w;
			src_h = crop.h;
			crop.x = crop.y = 0;

			width = src_w;
			height = src_h;
		}

		format = format_info->v4l2_format;

		if (!pPortPriv->v4l2_handle)
		{
			pPortPriv->v4l2_index = CreateDisplay (&pPortPriv->v4l2_handle, v4l2_index, nBuffer);
			pFBDev->v4l2_owner[pPortPriv->v4l2_index] = (void *)pPortPriv;
		}

		if (pPortPriv->v4l2_handle)
		{
			if(SetDisplayFormat(pPortPriv->v4l2_handle, width, height,
			   &win, &crop, &win,
			   format, screen_rotation,
			   pPortPriv->hflip, pPortPriv->vflip,
			   nBuffer, &pPortPriv->size))
			{
				fbdevQueryImageAttributes(NULL, id, &w, &h, pitches, offsets);

				xRectangle img = {0, 0, w, h};
				xRectangle pxm = {0, 0, win.w, win.h};
				xRectangle draw = {0, 0, win.w, win.h};
				xRectangle src = {src_x, src_y, src_w, src_h};
				xRectangle dst = {0, 0, win.w, win.h};

				DrawDisplay (pPortPriv->v4l2_handle, buf, &img, &pxm, &draw, &src, &dst, clip_boxes);

				if (modeBefore == MODE_VIRTUAL)
					pScrnInfo->pScreen->WindowExposures((WindowPtr) pDraw, clip_boxes, NULL);

				/* update cliplist */
				if (!REGION_EQUAL(pScrnInfo->pScreen, &pPortPriv->clip, clip_boxes))
				{
					/* setting transparency length to 8 */
					if (!pFBDev->bFbAlphaEnabled)
					{
						FBDevScreenAlphaInit(fbdevHWGetFD(pScrnInfo));
						pFBDev->bFbAlphaEnabled = TRUE;
					}
				}

				return Success;
			}
			DeleteDisplay(pPortPriv->v4l2_handle, 0);
			pPortPriv->v4l2_handle = NULL;
			pFBDev->v4l2_owner[pPortPriv->v4l2_index] = NULL;
		}

		pPortPriv->mode = MODE_VIRTUAL;
	}

	if (pPortPriv->mode == MODE_VIRTUAL)
	{
		ScreenPtr pScreen = pScrnInfo->pScreen;
		PixmapPtr pPixmap;

		if (src_w != dst_w || src_h != dst_h)
		{
			return BadRequest;
		}
		if (id != FOURCC_RGB32)
		{
			return BadRequest;
		}

		if (pDraw->type == DRAWABLE_WINDOW)
			pPixmap =
			    (*pScreen->GetWindowPixmap)((WindowPtr) pDraw);
		else
			pPixmap = (PixmapPtr) pDraw;

		fbdevDisplayVideo (pPixmap, clip_boxes, buf, src_x, src_y, width, height, DONT_FILL_ALPHA);

		DamageDamageRegion(pDraw, clip_boxes);

		if (modeBefore == MODE_V4L2)
		{
			if (!DeleteDisplay(pPortPriv->v4l2_handle, 0))
			{
			}
			pPortPriv->v4l2_handle = NULL;
			pFBDev->v4l2_owner[pPortPriv->v4l2_index] = NULL;
		}

		return Success;
	}

	return Success;
}

/**
 * Set up all our internal structures.
 */
static XF86VideoAdaptorPtr
fbdevSetupImageVideo(ScreenPtr pScreen)
{
	XF86VideoAdaptorPtr pAdaptor;
	FBDevPortPrivPtr pPortPriv;

	int i;

	pAdaptor = calloc(1, sizeof(XF86VideoAdaptorRec) +
	                  (sizeof(DevUnion) + sizeof(FBDevPortPriv)) * FBDEV_MAX_PORT);
	if (pAdaptor == NULL)
		return NULL;

	DummyEncoding[0].width = pScreen->width;
	DummyEncoding[0].height = pScreen->height;

	pAdaptor->type = XvWindowMask | XvInputMask | XvImageMask | XvStillMask;
	pAdaptor->flags = (VIDEO_CLIP_TO_VIEWPORT | VIDEO_OVERLAID_IMAGES);
	pAdaptor->name = "FBDEV supporting Software Video Conversions";
	pAdaptor->nEncodings = sizeof(DummyEncoding) / sizeof(XF86VideoEncodingRec);
	pAdaptor->pEncodings = DummyEncoding;
	pAdaptor->nFormats = NUM_FORMATS;
	pAdaptor->pFormats = Formats;
	pAdaptor->nPorts = FBDEV_MAX_PORT;
	pAdaptor->pPortPrivates = (DevUnion *)(&pAdaptor[1]);

	pPortPriv =
	    (FBDevPortPrivPtr)(&pAdaptor->pPortPrivates[FBDEV_MAX_PORT]);

	for(i=0; i<FBDEV_MAX_PORT; i++)
	{
		pAdaptor->pPortPrivates[i].ptr = &pPortPriv[i];

		pPortPriv[i].index = i;
		pPortPriv[i].rotation = -1;

		REGION_INIT(pScreen, &pPortPriv[i].clipBoxes, NullBox, 0);
	}

	pAdaptor->nAttributes = NUM_ATTRIBUTES;
	pAdaptor->pAttributes = Attributes;
	pAdaptor->nImages = NUM_IMAGES;
	pAdaptor->pImages = Images;

	pAdaptor->PutStill             = fbdevPutStill;
	pAdaptor->PutImage             = fbdevPutImage;
	pAdaptor->ReputImage           = fbdevReputImage;
	pAdaptor->StopVideo            = fbdevVideoStop;
	pAdaptor->GetPortAttribute     = fbdevGetPortAttribute;
	pAdaptor->SetPortAttribute     = fbdevSetPortAttribute;
	pAdaptor->QueryBestSize        = fbdevQueryBestSize;
	pAdaptor->QueryImageAttributes = fbdevQueryImageAttributes;

	return pAdaptor;
}

/**
 * Set up everything we need for Xv.
 */
Bool fbdevVideoInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrnInfo = xf86Screens[pScreen->myNum];
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;

	pFBDev->pAdaptor = fbdevSetupImageVideo(pScreen);

	if (pFBDev->pAdaptor)
	{
		xf86XVScreenInit(pScreen, &pFBDev->pAdaptor, 1);
		pFBDev->v4l2_num = GetDisplayNums ();
		pFBDev->v4l2_owner = (void**)calloc(sizeof (void*), pFBDev->v4l2_num);
		return TRUE;
	}
	else
		return FALSE;
}

/**
 * Shut down Xv, used on regeneration.
 */
void fbdevVideoFini(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrnInfo = xf86Screens[pScreen->myNum];
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;

	XF86VideoAdaptorPtr pAdaptor = pFBDev->pAdaptor;
	int i;

	if (!pAdaptor)
		return;

	for(i = 0; i < FBDEV_MAX_PORT; i++)
	{
		FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) pAdaptor->pPortPrivates[i].ptr;

		REGION_UNINIT(pScreen, &pPortPriv->clipBoxes);

		if(pPortPriv->v4l2_handle)
			DeleteDisplay(pPortPriv->v4l2_handle, 0);
	}

	free(pAdaptor);
	pFBDev->pAdaptor = NULL;

	if (pFBDev->v4l2_owner)
	{
		free(pFBDev->v4l2_owner);
		pFBDev->v4l2_owner = NULL;
		pFBDev->v4l2_num = 0;
	}
}

int
fbdevVideoProcessArgument (int argc, char **argv, int i)
{
	return 1;
}
