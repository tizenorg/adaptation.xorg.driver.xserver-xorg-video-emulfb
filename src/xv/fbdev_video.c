/*
 * xserver-xorg-video-emulfb
 *
 * Copyright 2004 Keith Packard
 * Copyright 2005 Eric Anholt
 * Copyright 2006 Nokia Corporation
 * Copyright 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.
 *
 * Contact: Boram Park <boram1288.park@samsung.com>
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <pixman.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvproto.h>
#include <X11/extensions/dpmsconst.h>
#include "fourcc.h"

#include "fb.h"
#include "fbdevhw.h"
#include "damage.h"

#include "xf86xv.h"

#include "fbdev.h"

#include "fbdev_dpms.h"
#include "fbdev_video.h"
#include "fbdev_util.h"
#include "fbdev_util.h"
#include "fbdev_pixman.h"
#include "fbdev_fb.h"
#include "fbdev_video_fourcc.h"
#include "fbdev_video_v4l2.h"
#include "fbdev_video_virtual.h"

#include "xv_types.h"

extern CallbackListPtr DPMSCallback;

static XF86VideoEncodingRec DummyEncoding[] =
{
	{ 0, "XV_IMAGE", -1, -1, { 1, 1 } },
	{ 1, "XV_IMAGE", 2560, 2560, { 1, 1 } },
};

static XF86VideoFormatRec Formats[] =
{
	{ 16, TrueColor },
	{ 24, TrueColor },
	{ 32, TrueColor },
};

static XF86AttributeRec Attributes[] =
{
	{ 0, -1, 270, "_USER_WM_PORT_ATTRIBUTE_ROTATION" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_HFLIP" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_VFLIP" },
	{ 0, -1, 1, "_USER_WM_PORT_ATTRIBUTE_PREEMPTION" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_DRAWING_MODE" },
	{ 0, 0, 1, "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF" },
};

typedef enum
{
	PAA_MIN,
	PAA_ROTATION,
	PAA_HFLIP,
	PAA_VFLIP,
	PAA_PREEMPTION,
	PAA_DRAWINGMODE,
	PAA_STREAMOFF,
	PAA_MAX
} FBDevPortAttrAtom;

enum
{
	ON_NONE,
	ON_FB,
	ON_WINDOW,
	ON_PIXMAP
};

static struct
{
	FBDevPortAttrAtom  paa;
	const char      *name;
	Atom             atom;
} atom_list[] =
{
	{ PAA_ROTATION, "_USER_WM_PORT_ATTRIBUTE_ROTATION", None },
	{ PAA_HFLIP, "_USER_WM_PORT_ATTRIBUTE_HFLIP", None },
	{ PAA_VFLIP, "_USER_WM_PORT_ATTRIBUTE_VFLIP", None },
	{ PAA_PREEMPTION, "_USER_WM_PORT_ATTRIBUTE_PREEMPTION", None },
	{ PAA_DRAWINGMODE, "_USER_WM_PORT_ATTRIBUTE_DRAWING_MODE", None },
	{ PAA_STREAMOFF, "_USER_WM_PORT_ATTRIBUTE_STREAM_OFF", None },
};

static int registered_handler;
extern CallbackListPtr DPMSCallback;

#define FBDEV_MAX_PORT    16
#define REQ_BUF_NUM       3

#define NUM_FORMATS       (sizeof(Formats) / sizeof(Formats[0]))
#define NUM_ATTRIBUTES    (sizeof(Attributes) / sizeof(Attributes[0]))
#define NUM_ATOMS         (sizeof(atom_list) / sizeof(atom_list[0]))

static PixmapPtr
_fbdevVideoGetPixmap (DrawablePtr pDraw)
{
    if (pDraw->type == DRAWABLE_WINDOW)
        return pDraw->pScreen->GetWindowPixmap ((WindowPtr) pDraw);
    else
        return (PixmapPtr) pDraw;
}

static void
_fbdevVideoGetRotation (ScreenPtr      pScreen,
                        FBDevPortPrivPtr pPortPriv,
                        DrawablePtr    pDraw,
                        int           *scn_rotate,
                        int           *rotate,
                        int           *hw_rotate)
{
	ScrnInfoPtr pScrnInfo = xf86Screens[pScreen->myNum];
	FBDevPtr pFBDev = FBDEVPTR (pScrnInfo);

	*scn_rotate = 0;
	*rotate = 0;
	*hw_rotate = 0;

#ifdef RANDR
	switch (pFBDev->rotate)
	{
	case RR_Rotate_90:
		*scn_rotate = 270;
        break;
	case RR_Rotate_180:
		*scn_rotate = 180;
        break;
	case RR_Rotate_270:
		*scn_rotate = 90;
        break;
	case RR_Rotate_0:
		break;
	}
#endif

	if (pPortPriv->rotate >= 0)
		*rotate = pPortPriv->rotate;

	*hw_rotate = (*rotate + *scn_rotate + 360) % 360;
}

static void
_fbdevVideoCloseV4l2Handle (ScrnInfoPtr pScrnInfo, FBDevPortPrivPtr pPortPriv)
{
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;

	if (!pPortPriv->v4l2_handle)
		return;

	fbdevVideoV4l2CloseHandle (pPortPriv->v4l2_handle);

	pFBDev->v4l2_owner[pPortPriv->v4l2_index] = NULL;

	pPortPriv->v4l2_handle = NULL;
	pPortPriv->v4l2_index = -1;

    if (pPortPriv->aligned_buffer)
    {
        free (pPortPriv->aligned_buffer);
        pPortPriv->aligned_buffer = NULL;
        pPortPriv->aligned_width = 0;
    }
}

static Atom
_fbdevVideoGetPortAtom (FBDevPortAttrAtom paa)
{
	int i;

	return_val_if_fail (paa > PAA_MIN && paa < PAA_MAX, None);

	for (i = 0; i < NUM_ATOMS; i++)
	{
		if (paa == atom_list[i].paa)
		{
			if (atom_list[i].atom == None)
				atom_list[i].atom = MakeAtom (atom_list[i].name, strlen (atom_list[i].name), TRUE);

			return atom_list[i].atom;
		}
	}

	ErrorF ("Error: Unknown Port Attribute Name!\n");

	return None;
}

static int
_fbdevVideoInterfbdevtXRects (xRectangle *dest, xRectangle *src1, xRectangle *src2)
{
	int dest_x, dest_y;
	int dest_x2, dest_y2;
	int return_val;

	return_val_if_fail (src1 != NULL, FALSE);
	return_val_if_fail (src2 != NULL, FALSE);

	return_val = FALSE;

	dest_x = MAX (src1->x, src2->x);
	dest_y = MAX (src1->y, src2->y);
	dest_x2 = MIN (src1->x + src1->width, src2->x + src2->width);
	dest_y2 = MIN (src1->y + src1->height, src2->y + src2->height);

	if (dest_x2 > dest_x && dest_y2 > dest_y)
	{
		if (dest)
		{
			dest->x = dest_x;
			dest->y = dest_y;
			dest->width = dest_x2 - dest_x;
			dest->height = dest_y2 - dest_y;
		}
		return_val = TRUE;
	}
	else if (dest)
	{
		dest->width = 0;
		dest->height = 0;
	}

	return return_val;
}

static int
_fbdevVideodrawingOn (FBDevPortPrivPtr pPortPriv, DrawablePtr pDraw)
{
    if (pDraw->type == DRAWABLE_PIXMAP)
        return ON_PIXMAP;
    else if (pDraw->type == DRAWABLE_WINDOW)
    {
        PropertyPtr prop = fbdev_util_get_window_property ((WindowPtr)pDraw,
                                                           "XV_ON_DRAWABLE");
        if (prop && *(int*)prop->data > 0)
            return ON_WINDOW;
    }

    return ON_FB;
}

static int
_fbdevVideoParseFormatBuffer (uchar *buf, uint *phy_addrs)
{
	XV_PUTIMAGE_DATA_PTR data = (XV_PUTIMAGE_DATA_PTR) buf;

	int valid = XV_PUTIMAGE_VALIDATE_DATA (data);
	if (valid < 0)
		return valid;

	phy_addrs[0] = data->YPhyAddr;
	phy_addrs[1] = data->CbPhyAddr;
	phy_addrs[2] = data->CrPhyAddr;

	return 0;
}

static XF86ImageRec *
_fbdevVideoGetImageInfo (int id)
{
	XF86ImagePtr pImages;
	int i, count = 0;

	pImages = fbdevVideoV4l2SupportImages (&count);

	for (i = 0; i < count; i++)
	{
		if (pImages[i].id == id)
			return &pImages[i];
	}

	return NULL;
};

static Bool
_fbdevVideoSetMode (ScrnInfoPtr pScrnInfo, FBDevPortPrivPtr pPortPriv, int *index)
{
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;
	Bool full = TRUE;
	int  i = 0;

	*index = -1;

	if (pPortPriv->preemption == -1)
	{
		pPortPriv->mode = PORT_MODE_WAITING;
		return TRUE;
	}

	for (i = 0; i < pFBDev->v4l2_num; i++)
		if (!fbdevVideoV4l2HandleOpened (i))
		{
			full = FALSE;
			break;
		}

	if (!full)
	{
		*index = i;
		pPortPriv->mode = PORT_MODE_V4L2;
		return TRUE;
	}

	/* All handles are occupied. So we need to steal one of them. */

	if (pPortPriv->preemption == 0)
	{
		pPortPriv->mode = PORT_MODE_WAITING;
		return TRUE;
	}

	for (i = 0; i < pFBDev->v4l2_num; i++)
	{
		FBDevPortPrivPtr pOwnerPort = (FBDevPortPrivPtr) pFBDev->v4l2_owner[i];

		if (pOwnerPort && pOwnerPort->preemption == 0)
		{
			_fbdevVideoCloseV4l2Handle (pScrnInfo, pOwnerPort);

			pOwnerPort->mode = PORT_MODE_WAITING;
			pPortPriv->mode = PORT_MODE_V4L2;
			*index = i;
			return TRUE;
		}
	}

	xf86DrvMsg (0, X_ERROR, "fbdev/put_image: Three or more preemptive ports were requested\n");

	return FALSE;
}

static int
_fbdevVideoPutImageV4l2 (ScrnInfoPtr pScrnInfo,
                         FBDevPortPrivPtr pPortPriv,
                         xRectangle *img,
                         xRectangle *src,
                         xRectangle *dst,
                         RegionPtr   clip_boxes,
                         int scn_rotate,
                         int rotate,
                         int hw_rotate,
                         XF86ImageRec *image_info,
                         uchar *buf,
                         FBDevPortMode modeBefore,
                         int v4l2_index,
                         DrawablePtr pDraw)
{
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;
	uint phy_addrs[4] = {0,};
	FBDevV4l2Memory memory;
	uint pixelformat;
	int fmt_type = 0;

	if (!fbdevVideoV4l2GetFormatInfo (image_info->id, &fmt_type, &pixelformat, &memory))
	{
		xf86DrvMsg (0, X_ERROR, "ID(%c%c%c%c) is not in 'format_infos'.\n",
		            image_info->id & 0xFF, (image_info->id & 0xFF00) >> 8,
		            (image_info->id & 0xFF0000) >> 16,  (image_info->id & 0xFF000000) >> 24);
		return BadRequest;
	}

	if (memory == V4L2_MEMORY_USERPTR)
	{
		int ret;
		if ((ret = _fbdevVideoParseFormatBuffer (buf, phy_addrs)) < 0)
		{
			if (ret == XV_HEADER_ERROR)
				xf86DrvMsg (0, X_ERROR, "XV_HEADER_ERROR\n");
			else if (ret == XV_VERSION_MISMATCH)
				xf86DrvMsg (0, X_ERROR, "XV_VERSION_MISMATCH\n");

			return BadRequest;
		}

		/* Skip frame */
		if (phy_addrs[0] == 0)
			return Success;
	}

	if (!pPortPriv->v4l2_handle)
	{
		pPortPriv->v4l2_handle = fbdevVideoV4l2OpenHandle (pScrnInfo->pScreen, v4l2_index, REQ_BUF_NUM);
		if (!pPortPriv->v4l2_handle)
		{
			int other_index = (v4l2_index == 0) ? 1 : 0;
			if (fbdevVideoV4l2HandleOpened (other_index))
			{
				xf86DrvMsg (0, X_ERROR, "fbdevVideoV4l2OpenHandle failed. no empty.\n");
				return BadRequest;
			}
			else
			{
				pPortPriv->v4l2_handle = fbdevVideoV4l2OpenHandle (pScrnInfo->pScreen, other_index, REQ_BUF_NUM);
				if (!pPortPriv->v4l2_handle)
				{
					xf86DrvMsg (0, X_ERROR, "fbdevVideoV4l2OpenHandle failed. fail open.\n");
					return BadRequest;
				}
			}

			v4l2_index = other_index;
		}

		pFBDev->v4l2_owner[v4l2_index] = (void*)pPortPriv;
		pPortPriv->v4l2_index = v4l2_index;
	}

	if (!fbdevVideoV4l2CheckSize (pPortPriv->v4l2_handle, pixelformat, img, src, dst, fmt_type, V4L2_MEMORY_MMAP))
		return BadRequest;

	if (fbdevVideoV4l2SetFormat (pPortPriv->v4l2_handle, img, src, dst,
	                             image_info->id, scn_rotate, hw_rotate,
	                             pPortPriv->hflip, pPortPriv->vflip,
	                             REQ_BUF_NUM, FALSE))
	{
		fbdevVideoV4l2Draw (pPortPriv->v4l2_handle, buf, phy_addrs);

		if (modeBefore == PORT_MODE_WAITING)
			pScrnInfo->pScreen->WindowExposures ((WindowPtr) pDraw, clip_boxes, NULL);

#if ENABLE_ARM
		/* update cliplist */
		if (!REGION_EQUAL (pScrnInfo->pScreen, &pPortPriv->clip, clip_boxes))
		{
			/* setting transparency length to 8 */
			if (!pFBDev->bFbAlphaEnabled)
			{
				fbdevFbScreenAlphaInit (fbdevHWGetFD (pScrnInfo));
				pFBDev->bFbAlphaEnabled = TRUE;
			}
		}
#endif

		return Success;
	}

	_fbdevVideoCloseV4l2Handle (pScrnInfo, pPortPriv);

	pPortPriv->mode = PORT_MODE_WAITING;

	return Success;
}

static int
_fbdevVideoPutImageOnDrawable (ScrnInfoPtr pScrnInfo,
                               FBDevPortPrivPtr pPortPriv,
                               xRectangle *img,
                               xRectangle *src,
                               xRectangle *dst,
                               RegionPtr   clip_boxes,
                               int scn_rotate,
                               int rotate,
                               XF86ImageRec *image_info,
                               uchar *buf,
                               DrawablePtr pDraw)
{
	pixman_format_code_t src_format, dst_format;
	xRectangle pxm = {0,};

	PixmapPtr pPixmap = _fbdevVideoGetPixmap (pDraw);

	pxm.width = pPixmap->drawable.width;
	pxm.height = pPixmap->drawable.height;

	switch (image_info->id)
	{
	case FOURCC_I420:
	case FOURCC_YV12:
		src_format = PIXMAN_yv12;
		break;
	case FOURCC_YUY2:
		src_format = PIXMAN_yuy2;
		break;
	case FOURCC_RGB565:
		src_format = PIXMAN_r5g6b5;
		break;
	case FOURCC_RGB32:
		src_format = PIXMAN_a8r8g8b8;
		break;
	default:
		return FALSE;
	}

	switch (image_info->id)
	{
	case FOURCC_I420:
		dst_format = PIXMAN_x8b8g8r8;
		break;
	default:
		dst_format = PIXMAN_x8r8g8b8;
		break;
	}

    if (image_info->id == FOURCC_I420 && img->width % 16)
    {
        int src_p[3] = {0,}, src_o[3] = {0,}, src_l[3] = {0,};
        int dst_p[3] = {0,}, dst_o[3] = {0,}, dst_l[3] = {0,};
        unsigned short src_w, src_h, dst_w, dst_h;
        int size;

        src_w = img->width;
        src_h = img->height;
        fbdevVideoQueryImageAttributes (NULL, image_info->id, &src_w, &src_h,
                                        src_p, src_o, src_l);

        dst_w = (img->width + 15) & ~15;
        dst_h = img->height;
        size = fbdevVideoQueryImageAttributes (NULL, image_info->id, &dst_w, &dst_h,
                                               dst_p, dst_o, dst_l);

        if (!pPortPriv->aligned_buffer)
        {
            pPortPriv->aligned_buffer = malloc (size);
            if (!pPortPriv->aligned_buffer)
                return FALSE;
        }

        fbdev_util_copy_image (src_w, src_h,
                               (char*)buf, src_w, src_h,
                               src_p, src_o, src_l,
                               (char*)pPortPriv->aligned_buffer, dst_w, dst_h,
                               dst_p, dst_o, dst_l,
                               3, 2, 2);

        pPortPriv->aligned_width = dst_w;
        img->width = dst_w;
        buf = pPortPriv->aligned_buffer;
    }

	/* support only RGB  */
	fbdev_pixman_convert_image (PIXMAN_OP_SRC,
	                            buf, pPixmap->devPrivate.ptr,
	                            src_format, dst_format,
	                            img, &pxm, src, dst,
	                            NULL, rotate,
	                            pPortPriv->hflip, pPortPriv->vflip);

    DamageDamageRegion (pDraw, clip_boxes);

	return Success;
}

static Bool
_fbdevVideoSetHWPortsProperty (ScreenPtr pScreen, int nums)
{
	WindowPtr pWin = pScreen->root;
	Atom atom_hw_ports;

	if (!pWin || !serverClient)
		return FALSE;

	atom_hw_ports = MakeAtom ("X_HW_PORTS", strlen ("X_HW_PORTS"), TRUE);

	dixChangeWindowProperty (serverClient,
	                         pWin, atom_hw_ports, XA_CARDINAL, 32,
	                         PropModeReplace, 1, (unsigned int*)&nums, FALSE);

	return TRUE;
}

static void
_fbdevVideoDPMSHandler(CallbackListPtr *list, pointer closure, pointer calldata)
{
	FBDevDPMSPtr pDPMSInfo = (FBDevDPMSPtr) calldata;

	if(!pDPMSInfo || !pDPMSInfo->pScrn)
	{
		xf86DrvMsg (0, X_ERROR, "[%s] DPMS info or screen info is invalid !\n", __FUNCTION__);
		return;
	}

	switch(DPMSPowerLevel)
	{
	case DPMSModeOn:
		break;

	case DPMSModeSuspend:
		break;

	case DPMSModeStandby://LCD on
	{
		ScrnInfoPtr pScrnInfo = pDPMSInfo->pScrn;
		FBDevPtr pFBDev = FBDEVPTR (pScrnInfo);
		XF86VideoAdaptorPtr pAdaptor = pFBDev->pAdaptor[0];
		int i;

		DRVLOG ("%s : DPMSModeStandby \n", __FUNCTION__);

		for (i = 0; i < FBDEV_MAX_PORT; i++)
		{
			FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) pAdaptor->pPortPrivates[i].ptr;
			if (!pPortPriv->v4l2_handle || !pPortPriv->need_streamon)
				continue;

			if (!fbdevVideoV4l2StreamOn (pPortPriv->v4l2_handle))
			{
				/* will re-open if failed */
				_fbdevVideoCloseV4l2Handle (pScrnInfo, pPortPriv);
				pPortPriv->v4l2_handle = NULL;
			}
			pPortPriv->need_streamon = FALSE;
		}
		break;
	}
	case DPMSModeOff://LCD off
	{
		ScrnInfoPtr pScrnInfo = pDPMSInfo->pScrn;
		FBDevPtr pFBDev = FBDEVPTR (pScrnInfo);
		XF86VideoAdaptorPtr pAdaptor = pFBDev->pAdaptor[0];
		int i;

		DRVLOG ("%s : DPMSModeOff \n", __FUNCTION__);

		for (i = 0; i < FBDEV_MAX_PORT; i++)
		{
			FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) pAdaptor->pPortPrivates[i].ptr;
			if (!pPortPriv->v4l2_handle || pPortPriv->need_streamon)
				continue;

			fbdevVideoV4l2StreamOff (pPortPriv->v4l2_handle);
			pPortPriv->need_streamon = TRUE;
		}

		break;
	}
	default:
		return;
	}
}

static void
_fbdevVideoBlockHandler (pointer data, OSTimePtr pTimeout, pointer pRead)
{
	ScrnInfoPtr pScrnInfo = (ScrnInfoPtr)data;
	FBDevPtr pFBDev = FBDEVPTR (pScrnInfo);
	ScreenPtr pScreen = pScrnInfo->pScreen;

	if(registered_handler && _fbdevVideoSetHWPortsProperty (pScreen, pFBDev->v4l2_num))
	{
		RemoveBlockAndWakeupHandlers(_fbdevVideoBlockHandler, (WakeupHandlerProcPtr)NoopDDA, data);
		registered_handler = FALSE;
	}
}

static int
FBDevVideoGetPortAttribute (ScrnInfoPtr pScrnInfo,
                            Atom        attribute,
                            INT32      *value,
                            pointer     data)
{
	DRVLOG ("[GetPortAttribute] \n");

	FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) data;

	if (attribute == _fbdevVideoGetPortAtom (PAA_ROTATION))
	{
		*value = pPortPriv->rotate;
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_HFLIP))
	{
		*value = pPortPriv->hflip;
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_VFLIP))
	{
		*value = pPortPriv->vflip;
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_PREEMPTION))
	{
		*value = pPortPriv->preemption;
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_DRAWINGMODE))
	{
		*value = (pPortPriv->mode == PORT_MODE_WAITING);
		return Success;
	}
	return BadMatch;
}

static int
FBDevVideoSetPortAttribute (ScrnInfoPtr pScrnInfo,
                            Atom        attribute,
                            INT32       value,
                            pointer     data)
{
	FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) data;

	if (attribute == _fbdevVideoGetPortAtom (PAA_ROTATION))
	{
		pPortPriv->rotate = value;
		DRVLOG ("[SetPortAttribute] rotate(%d) \n", value);
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_HFLIP))
	{
		pPortPriv->hflip = value;
		DRVLOG ("[SetPortAttribute] hflip(%d) \n", value);
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_VFLIP))
	{
		pPortPriv->vflip = value;
		DRVLOG ("[SetPortAttribute] vflip(%d) \n", value);
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_PREEMPTION))
	{
		pPortPriv->preemption = value;
		DRVLOG ("[SetPortAttribute] preemption(%d) \n", value);
		return Success;
	}
	else if (attribute == _fbdevVideoGetPortAtom (PAA_STREAMOFF))
	{
		DRVLOG ("[SetPortAttribute] STREAMOFF \n");
		_fbdevVideoCloseV4l2Handle (pScrnInfo, pPortPriv);
		return Success;
	}
	return BadMatch;
}

static void
FBDevVideoQueryBestSize (ScrnInfoPtr pScrnInfo,
                         Bool motion,
                         short vid_w, short vid_h,
                         short dst_w, short dst_h,
                         uint *p_w, uint *p_h,
                         pointer data)
{
	DRVLOG ("%s (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);

	*p_w = dst_w;
	*p_h = dst_h;
}

static void
FBDevVideoStop (ScrnInfoPtr pScrnInfo, pointer data, Bool exit)
{
	DRVLOG ("%s (%s:%d) exit(%d)\n", __FUNCTION__, __FILE__, __LINE__, exit);

	FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) data;

	if (pPortPriv->mode == PORT_MODE_V4L2)
	{
		_fbdevVideoCloseV4l2Handle (pScrnInfo, pPortPriv);

#if ENABLE_ARM
		FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;

		if (pFBDev->bFbAlphaEnabled)
		{
			fbdevFbScreenAlphaDeinit (fbdevHWGetFD (pScrnInfo));
			pFBDev->bFbAlphaEnabled = FALSE;
		}
#endif
	}

    if (pPortPriv->aligned_buffer)
    {
        free (pPortPriv->aligned_buffer);
        pPortPriv->aligned_buffer = NULL;
        pPortPriv->aligned_width = 0;
    }

	pPortPriv->mode = PORT_MODE_INIT;
	pPortPriv->preemption = 0;
	pPortPriv->rotate = -1;
	pPortPriv->need_streamon = FALSE;

	if (exit)
		REGION_EMPTY (pScrnInfo, &pPortPriv->clip);
}

static int
FBDevVideoReputImage (ScrnInfoPtr pScrn, short src_x, short src_y,
                                  short drw_x, short drw_y, short src_w,
                                  short src_h, short drw_w, short drw_h,
                                  RegionPtr clipBoxes, pointer data,
                                  DrawablePtr pDraw)

{
	return Success;
}

int
fbdevVideoQueryImageAttributes (ScrnInfoPtr    pScrnInfo,
                                int            id,
                                unsigned short *w,
                                unsigned short *h,
                                int            *pitches,
                                int            *offsets,
                                int            *lengths)
{
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
        if (lengths)
            lengths[0] = size;
		break;
	case FOURCC_RGB24:
		size += (*w << 1) + *w;
		if (pitches)
			pitches[0] = size;
		size *= *h;
        if (lengths)
            lengths[0] = size;
		break;
	case FOURCC_RGB32:
		size += (*w << 2);
		if (pitches)
			pitches[0] = size;
		size *= *h;
        if (lengths)
            lengths[0] = size;
		break;
	case FOURCC_I420:
	case FOURCC_S420:
	case FOURCC_YV12:
		*h = (*h + 1) & ~1;
		size = (*w + 3) & ~3;
		if (pitches)
			pitches[0] = size;

		size *= *h;
		if (offsets)
			offsets[1] = size;
        if (lengths)
            lengths[0] = size;

		tmp = ((*w >> 1) + 3) & ~3;
		if (pitches)
			pitches[1] = pitches[2] = tmp;

		tmp *= (*h >> 1);
		size += tmp;
		if (offsets)
			offsets[2] = size;
        if (lengths)
            lengths[1] = tmp;

		size += tmp;
        if (lengths)
            lengths[2] = tmp;

		break;
	case FOURCC_UYVY:
	case FOURCC_SUYV:
	case FOURCC_YUY2:
	case FOURCC_ST12:
	case FOURCC_SN12:
		size = *w << 1;
		if (pitches)
			pitches[0] = size;

		size *= *h;
		break;
	case FOURCC_NV12:
		size = *w;
		if (pitches)
			pitches[0] = size;

		size *= *h;
		if (offsets)
			offsets[1] = size;

		tmp = *w;
		if (pitches)
			pitches[1] = tmp;

		tmp *= (*h >> 1);
		size += tmp;
		break;
	default:
		return BadIDChoice;
	}

	return size;
}

static int
FBDEVVideoQueryImageAttributes (ScrnInfoPtr    pScrnInfo,
                                int            id,
                                unsigned short *w,
                                unsigned short *h,
                                int            *pitches,
                                int            *offsets)
{
    return fbdevVideoQueryImageAttributes (pScrnInfo, id, w, h, pitches, offsets, NULL);
}

int secUtilDumpRaw (const char * file, const void * data, int size)
{
//	int i;
    unsigned int * blocks;

    FILE * fp = fopen (file, "w+");
    if (fp == NULL)
    {
        return 0;
    }
    else
    {
        blocks = (unsigned int*)data;
        fwrite (blocks, 1, size, fp);

        fclose (fp);
    }

    return 0;
}

static int
FBDevVideoPutImage (ScrnInfoPtr pScrnInfo,
                    short src_x, short src_y, short dst_x, short dst_y,
                    short src_w, short src_h, short dst_w, short dst_h,
                    int id,
                    uchar *buf,
                    short width, short height,
                    Bool sync,
                    RegionPtr clip_boxes,
                    pointer data,
                    DrawablePtr pDraw)
{
	ScreenPtr pScreen = pScrnInfo->pScreen;
	FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) data;
	XF86ImageRec *image_info;
	FBDevPortMode modeBefore = pPortPriv->mode;
	xRectangle img = {0, 0, width, height};
	xRectangle src = {src_x, src_y, src_w, src_h};
	xRectangle dst = {dst_x, dst_y, dst_w, dst_h};
	int scn_rotate, rotate, hw_rotate;
	int v4l2_index = 0;
	FBDevPtr pFBDev = FBDEVPTR (pScrnInfo);
	int drawing;

	if (pFBDev->isLcdOff)
		return Success;

	pPortPriv->pDraw = pDraw;
	drawing = _fbdevVideodrawingOn (pPortPriv, pDraw);

	image_info = _fbdevVideoGetImageInfo (id);
	if (!image_info)
	{
		xf86DrvMsg (0, X_ERROR, "ID(%c%c%c%c) not supported.\n",
		            id & 0xFF, (id & 0xFF00) >> 8,
		            (id & 0xFF0000) >> 16,  (id & 0xFF000000) >> 24);
		return BadRequest;
	}

  static int i;
  char name[128];
  sprintf (name, "/mnt/host/temp/xv_%d_%d_%d.yuv", width, height, i++);
  secUtilDumpRaw (name, buf, ((width+3)&~3)*height*1.5);

	_fbdevVideoGetRotation (pScreen, pPortPriv, pDraw, &scn_rotate, &rotate, &hw_rotate);

	DRVLOG ("[PutImage] buf(%p) ID(%c%c%c%c) mode(%s) preem(%d) handle(%p) rot(%d+%d=%d) res(%dx%d) drawing(%d)\n",
	        buf, id & 0xFF, (id & 0xFF00) >> 8, (id & 0xFF0000) >> 16,  (id & 0xFF000000) >> 24,
	        pPortPriv->mode == PORT_MODE_WAITING ? "waiting" : "V4L2",
	        pPortPriv->preemption,
	        pPortPriv->v4l2_handle,
	        scn_rotate, rotate, hw_rotate,
	        pScreen->width, pScreen->height, drawing);

	DRVLOG ("[PutImage] \t   img(%d,%d %dx%d) src(%d,%d %dx%d) dst(%d,%d %dx%d) \n",
	        img.x, img.y, img.width, img.height,
	        src.x, src.y, src.width, src.height,
	        dst.x, dst.y, dst.width, dst.height);

	_fbdevVideoInterfbdevtXRects (&src, &src, &img);

	DRVLOG ("[PutImage] \t=> img(%d,%d %dx%d) src(%d,%d %dx%d) dst(%d,%d %dx%d) \n",
	        img.x, img.y, img.width, img.height,
	        src.x, src.y, src.width, src.height,
	        dst.x, dst.y, dst.width, dst.height);

	if (drawing == ON_PIXMAP || drawing == ON_WINDOW)
		return _fbdevVideoPutImageOnDrawable (pScrnInfo, pPortPriv, &img, &src, &dst, clip_boxes,
		                                      scn_rotate, hw_rotate, image_info, buf, pDraw);

	if (pPortPriv->need_streamon && pPortPriv->v4l2_handle)
	{
		_fbdevVideoCloseV4l2Handle (pScrnInfo, pPortPriv);
		pPortPriv->need_streamon = FALSE;
		pPortPriv->v4l2_handle = NULL;
	}

	if (!pPortPriv->v4l2_handle)
		if (!_fbdevVideoSetMode (pScrnInfo, pPortPriv, &v4l2_index))
			return BadRequest;

	if (pPortPriv->mode != PORT_MODE_V4L2)
		return Success;

	return _fbdevVideoPutImageV4l2 (pScrnInfo, pPortPriv, &img, &src, &dst, clip_boxes,
	                                scn_rotate, rotate, hw_rotate,
	                                image_info, buf, modeBefore, v4l2_index, pDraw);
}

/**
 * Set up all our internal structures.
 */
static XF86VideoAdaptorPtr
fbdevVideoSetupImageVideo (ScreenPtr pScreen)
{
	DRVLOG ("%s (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);

	XF86VideoAdaptorPtr pAdaptor;
	FBDevPortPrivPtr pPortPriv;
	XF86ImagePtr pImages;
	int i, count = 0;

	pAdaptor = calloc (1, sizeof (XF86VideoAdaptorRec) +
	                   (sizeof (DevUnion) + sizeof (FBDevPortPriv)) * FBDEV_MAX_PORT);
	if (pAdaptor == NULL)
		return NULL;

	DummyEncoding[0].width = pScreen->width;
	DummyEncoding[0].height = pScreen->height;

	pAdaptor->type = XvWindowMask | XvPixmapMask | XvInputMask | XvImageMask;
	pAdaptor->flags = (VIDEO_CLIP_TO_VIEWPORT | VIDEO_OVERLAID_IMAGES);
	pAdaptor->name = "FBDEV supporting Software Video Conversions";
	pAdaptor->nEncodings = sizeof (DummyEncoding) / sizeof (XF86VideoEncodingRec);
	pAdaptor->pEncodings = DummyEncoding;
	pAdaptor->nFormats = NUM_FORMATS;
	pAdaptor->pFormats = Formats;
	pAdaptor->nPorts = FBDEV_MAX_PORT;
	pAdaptor->pPortPrivates = (DevUnion*)(&pAdaptor[1]);

	pPortPriv =
	    (FBDevPortPrivPtr) (&pAdaptor->pPortPrivates[FBDEV_MAX_PORT]);

	for (i=0; i<FBDEV_MAX_PORT; i++)
	{
		pAdaptor->pPortPrivates[i].ptr = &pPortPriv[i];

		pPortPriv[i].index = i;
		pPortPriv[i].rotate = -1;
		pPortPriv[i].v4l2_index = -1;

		REGION_INIT (pScreen, &pPortPriv[i].clipBoxes, NullBox, 0);
	}

	pImages = fbdevVideoV4l2SupportImages (&count);

	pAdaptor->nAttributes = NUM_ATTRIBUTES;
	pAdaptor->pAttributes = Attributes;
	pAdaptor->nImages = count;
	pAdaptor->pImages = pImages;

	pAdaptor->PutImage             = FBDevVideoPutImage;
	pAdaptor->ReputImage           = FBDevVideoReputImage;
	pAdaptor->StopVideo            = FBDevVideoStop;
	pAdaptor->GetPortAttribute     = FBDevVideoGetPortAttribute;
	pAdaptor->SetPortAttribute     = FBDevVideoSetPortAttribute;
	pAdaptor->QueryBestSize        = FBDevVideoQueryBestSize;
	pAdaptor->QueryImageAttributes = FBDEVVideoQueryImageAttributes;

	return pAdaptor;
}

#ifdef XV
/**
 * Set up everything we need for Xv.
 */
Bool fbdevVideoInit (ScreenPtr pScreen)
{
	DRVLOG ("%s (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);

	ScrnInfoPtr pScrnInfo = xf86Screens[pScreen->myNum];
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;

	pFBDev->pAdaptor[0] = fbdevVideoSetupImageVideo (pScreen);
	if (!pFBDev->pAdaptor[0])
		return FALSE;

	pFBDev->pAdaptor[1] = fbdevVideoSetupVirtualVideo (pScreen);
	if (!pFBDev->pAdaptor[1])
	{
		free (pFBDev->pAdaptor[0]);
		return FALSE;
	}

	xf86XVScreenInit (pScreen, pFBDev->pAdaptor, ADAPTOR_NUM);

	pFBDev->v4l2_num   = fbdevVideoV4l2GetHandleNums ();
	pFBDev->v4l2_owner = (void**)calloc (sizeof (void*), pFBDev->v4l2_num);

	if(registered_handler == FALSE)
	{
		RegisterBlockAndWakeupHandlers(_fbdevVideoBlockHandler, (WakeupHandlerProcPtr)NoopDDA, pScrnInfo);
		registered_handler = TRUE;
	}

	if (AddCallback (&DPMSCallback, _fbdevVideoDPMSHandler, NULL) != TRUE)
	{
		xf86DrvMsg (pScrnInfo->scrnIndex, X_ERROR, "Failed to register _fbdevVideoDPMSHandler. \n");
		return FALSE;
	}

	return TRUE;
}

/**
 * Shut down Xv, used on regeneration.
 */
void fbdevVideoFini (ScreenPtr pScreen)
{
	DRVLOG ("%s (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);

	ScrnInfoPtr pScrnInfo = xf86Screens[pScreen->myNum];
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;

	XF86VideoAdaptorPtr pAdaptor = pFBDev->pAdaptor[0];
	int i;

	if (!pAdaptor)
		return;

	DeleteCallback (&DPMSCallback, _fbdevVideoDPMSHandler, NULL);

	for (i = 0; i < FBDEV_MAX_PORT; i++)
	{
		FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) pAdaptor->pPortPrivates[i].ptr;

		REGION_UNINIT (pScreen, &pPortPriv->clipBoxes);

		_fbdevVideoCloseV4l2Handle (pScrnInfo, pPortPriv);
	}

	free (pFBDev->pAdaptor[0]);
	pFBDev->pAdaptor[0] = NULL;

	free (pFBDev->pAdaptor[1]);
	pFBDev->pAdaptor[1] = NULL;

	if (pFBDev->v4l2_owner)
	{
		free (pFBDev->v4l2_owner);
		pFBDev->v4l2_owner = NULL;
		pFBDev->v4l2_num = 0;
	}
}

#endif

void
fbdevVideoSetOffset (ScrnInfoPtr pScrnInfo, int x, int y)
{
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;
	int i;
	for (i = 0; i < pFBDev->v4l2_num; i++)
	{
		FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) pFBDev->v4l2_owner[i];

		if (pPortPriv)
		{
			if (!pPortPriv->v4l2_handle)
				continue;

			fbdevVideoV4l2VideoOffset (pPortPriv->v4l2_handle, x , y);
		}
	}
}

void
fbdevVideoGetV4l2Handles (ScrnInfoPtr pScrnInfo, void ***handles, int *cnt)
{
	FBDevPtr pFBDev = (FBDevPtr) pScrnInfo->driverPrivate;
	void **ret;
	int i, j;

	*cnt = 0;

	for (i = 0; i < pFBDev->v4l2_num; i++)
	{
		FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) pFBDev->v4l2_owner[i];
		if (pPortPriv && pPortPriv->v4l2_handle)
			(*cnt)++;
	}

	if (*cnt == 0)
	{
		*handles = NULL;
		return;
	}

	ret = (void**)calloc (*cnt, sizeof (void*));
	*handles = ret;

	j = 0;

	for (i = 0; i < pFBDev->v4l2_num; i++)
	{
		FBDevPortPrivPtr pPortPriv = (FBDevPortPrivPtr) pFBDev->v4l2_owner[i];
		if (pPortPriv && pPortPriv->v4l2_handle)
		{
			ret[j] = pPortPriv->v4l2_handle;
			j++;
		}
	}
}
