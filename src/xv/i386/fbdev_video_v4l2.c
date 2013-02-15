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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvproto.h>

#include "fbdev_util.h"
#include "fbdev_video_v4l2.h"

#include "fbdev.h"
#include "fbdev_fb.h"
#include "fbdev_util.h"
#include "fbdev_video.h"
#include "fbdev_video_fourcc.h"
#include "fbdev_pixman.h"

#define BUF_NUM     1

typedef struct _DeviceInfo
{
	char *video;     /* video */
	int   bOpened;
} DeviceInfo;

typedef struct _FormatInfo
{
	int           id;
	int           type;
	uint          pixelformat;
	FBDevV4l2Memory memory;
} FormatInfo;

typedef struct
{
	int index;

	ScreenPtr pScreen;

	int       video_fd;

	struct
	{
		int init;
		xRectangle img;
		xRectangle crop;
		xRectangle dst;
		int id;
		int hw_rotate;
		int scn_rotate;
		int hflip;
		int vflip;
		int streamon;
		int cur_idx;

		void* last_buffer;
		int   queued_index;

		void* backup;
		Bool  sync;
	} status;

	int size;
	unsigned int pixfmt;
	FBDevV4l2FimcBuffer fimcbuf;
	FBDevV4l2SrcBuffer *src_buf;
	FBDevV4l2DstBuffer *dst_buf;
	FBDevV4l2Memory     memory;

	int offset_x;
	int offset_y;

	int re_setting;
} FBDevDispHandle;

enum
{
	TYPE_RGB,
	TYPE_YUV444,
	TYPE_YUV422,
	TYPE_YUV420,
};

static FormatInfo format_infos [] =
{
	{ FOURCC_RGB565,    TYPE_RGB,       V4L2_PIX_FMT_RGB565,    V4L2_MEMORY_MMAP },
	{ FOURCC_RGB32,     TYPE_RGB,       V4L2_PIX_FMT_RGB32,     V4L2_MEMORY_MMAP },
	{ FOURCC_I420,      TYPE_YUV420,    V4L2_PIX_FMT_YUV420,    V4L2_MEMORY_MMAP },
	{ FOURCC_YUY2,      TYPE_YUV422,    V4L2_PIX_FMT_YUYV,      V4L2_MEMORY_MMAP },
	{ FOURCC_YV12,      TYPE_YUV420,    V4L2_PIX_FMT_YVU420,    V4L2_MEMORY_MMAP },

};

static XF86ImageRec Images[] =
{
	XVIMAGE_I420,
	XVIMAGE_YV12,
	XVIMAGE_YUY2,
	XVIMAGE_RGB32,
	XVIMAGE_RGB565,
};

static DeviceInfo device_infos[] =
{
	{ "/dev/video1", FALSE },
	{ "/dev/video2", FALSE },
};

#define NUM_IMAGES        (sizeof(Images) / sizeof(XF86ImageRec))
#define DEVICE_NUMS (sizeof (device_infos) / sizeof (DeviceInfo))

static Bool
_fbdevVideoV4l2SetDst (int fd, int offset_x, int offset_y, xRectangle *rect)
{
	struct v4l2_format      format = {0,};
	int caps;

	return_val_if_fail (fd >= 0, FALSE);

	DRVLOG ("[SetDst] offset(%d,%d) win(%d,%d %dx%d) \n", offset_x, offset_y,
	        rect->x, rect->y, rect->width, rect->height);

	caps = V4L2_CAP_VIDEO_OVERLAY;
	if (!fbdev_v4l2_querycap (fd, caps))
		return FALSE;

	/* set format */
	CLEAR (format);
	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	if (!fbdev_v4l2_g_fmt (fd, &format))
		return FALSE;

	format.fmt.win.w.left   = rect->x + offset_x;
	format.fmt.win.w.top    = rect->y + offset_y;
	format.fmt.win.w.width  = rect->width;
	format.fmt.win.w.height = rect->height;
	if (!fbdev_v4l2_s_fmt (fd, &format))
		return FALSE;

	return TRUE;
}

static Bool
_fbdevVideoV4l2SetBuffer (int fd,
                          FBDevV4l2BufType type,
                          FBDevV4l2Memory memory,
                          int num_buf,
                          FBDevV4l2DstBuffer **mem_buf)
{
	struct v4l2_requestbuffers	req = {0,};

	return_val_if_fail (fd >= 0, FALSE);

	DRVLOG ("[SetBuffer] num_buf(%d) mem_buf(%p) memory(%d)\n",
	        num_buf, mem_buf, memory);

	CLEAR (req);
	req.count  = num_buf;
	req.type   = type;
	req.memory = memory;
	if (!fbdev_v4l2_reqbuf (fd, &req))
		return FALSE;

	if (memory == V4L2_MEMORY_MMAP)
	{
		FBDevV4l2DstBuffer *out_buf;
		int i;

		return_val_if_fail (mem_buf != NULL, FALSE);

		out_buf = calloc (req.count, sizeof (FBDevV4l2DstBuffer));
		return_val_if_fail (out_buf != NULL, FALSE);

		for (i = 0; i < num_buf; i++)
		{
			struct v4l2_buffer buffer = {0,};

			CLEAR (buffer);
			buffer.index  = i;
			buffer.type   = type;
			buffer.memory = V4L2_MEMORY_MMAP;
			if (!fbdev_v4l2_querybuf (fd, &buffer))
			{
				free (out_buf);
				return FALSE;
			}

			out_buf[i].index  = buffer.index;
			out_buf[i].size   = buffer.length;
			out_buf[i].buf    = mmap (NULL, buffer.length,
			                          PROT_READ | PROT_WRITE , \
			                          MAP_SHARED , fd, buffer.m.offset & ~(PAGE_SIZE - 1));
			out_buf[i].buf    += buffer.m.offset & (PAGE_SIZE - 1);

			if (out_buf[i].buf == MAP_FAILED)
			{
				xf86DrvMsg (0, X_ERROR, "[SetBuffer] mmap failed. index(%d)\n", i);
				free (out_buf);
				return FALSE;
			}
		}

		*mem_buf = out_buf;
	}

	return TRUE;
}

static Bool
_fbdevVideoV4l2ClearBuffer (int fd,
                            FBDevV4l2BufType type,
                            FBDevV4l2Memory memory,
                            int num_buf,
                            FBDevV4l2DstBuffer *mem_buf)
{
	struct v4l2_requestbuffers req = {0,};

	return_val_if_fail (fd >= 0, FALSE);

	DRVLOG ("[ClearBuffer] memory(%d) num_buf(%d) mem_buf(%p)\n",
	        num_buf, mem_buf, memory);

	if (memory == V4L2_MEMORY_MMAP && mem_buf)
	{
		int i;

		for (i = 0; i < num_buf; i++)
			if (mem_buf[i].buf)
				if (munmap (mem_buf[i].buf, mem_buf[i].size) == -1)
					xf86DrvMsg (0, X_WARNING, "[ClearBuffer] Failed to unmap v4l2 buffer at index %d\n", i);

		free (mem_buf);
	}

	CLEAR (req);
	req.count  = 0;
	req.type   = type;
	req.memory = memory;
	if (!fbdev_v4l2_reqbuf (fd, &req))
		return FALSE;

	return TRUE;
}

static Bool
_fbdevVideoV4l2StreamOn (int fd)
{
	return_val_if_fail (fd >= 0, FALSE);

	DRVLOG ("[StreamOn] \n");

	if (!fbdev_v4l2_start_overlay (fd))
		return FALSE;

	return TRUE;
}


static Bool
_fbdevVideoV4l2StreamOff (int fd)
{
	return_val_if_fail (fd >= 0, FALSE);

	DRVLOG ("[StreamOff] \n");

	if (!fbdev_v4l2_stop_overlay (fd))
		return FALSE;

	return TRUE;
}

static int
_open_device (char *device)
{
	int fd;

	return_val_if_fail (device != NULL, -1);

	fd = open (device, O_RDWR);
	if (fd < 0)
	{
		xf86DrvMsg (0, X_ERROR, "Cannot open '%s'. fd(%d)\n", device, fd);
		return -1;
	}

	DRVLOG ("'%s' opened. fd(%d) \n", device, fd);

	return fd;
}

static void
_close_device (int fd)
{
	int ret;

	return_if_fail (fd >= 0);

	ret = close (fd);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "Cannot close fd(%d)\n", fd);
		return;
	}

	DRVLOG ("fd(%d) closed. \n", fd);
}

static Bool
_fbdevVideoV4l2EnsureStreamOff (FBDevDispHandle *hdisp)
{
	if (!hdisp->status.streamon)
		return TRUE;

	if (!_fbdevVideoV4l2StreamOff (hdisp->video_fd))
		return FALSE;

	hdisp->status.streamon = FALSE;

	return TRUE;
}

static Bool
_fbdevVideoV4l2OpenDevice (FBDevDispHandle *hdisp, int index)
{
	if (device_infos[index].bOpened)
	{
		DRVLOG ("[OpenDevice, %p] Already opened : %s\n", hdisp, device_infos[index].video);
		return FALSE;
	}

	hdisp->video_fd = _open_device (device_infos[index].video);
	if (hdisp->video_fd < 0)
		return FALSE;

	device_infos[index].bOpened = TRUE;

	return TRUE;
}

/* This function never failed.
 * If failed, it means kernel has some problems.
 * Then a device should be rebooted.
 */
static void
_fbdevVideoV4l2CloseDevice (FBDevDispHandle *hdisp)
{
	if (!_fbdevVideoV4l2EnsureStreamOff (hdisp)) // We will consider this as a LCD Off case.
		xf86DrvMsg (0, X_WARNING, "[CloseDevice, %p] Warning : Cannot stream off!! \n", hdisp);

	if (hdisp->video_fd >= 0)
		if (!_fbdevVideoV4l2ClearBuffer (hdisp->video_fd,
		                                 V4L2_BUF_TYPE_VIDEO_OVERLAY,
		                                 hdisp->memory,
		                                 BUF_NUM,
		                                 hdisp->dst_buf))
			xf86DrvMsg (0, X_WARNING, "[CloseDevice, %p] Warning : Cannot clear buffer!! \n", hdisp);

	hdisp->dst_buf = NULL;

	if (hdisp->video_fd >= 0)
	{
		_close_device (hdisp->video_fd);
		hdisp->video_fd = -1;
	}

	device_infos[hdisp->index].bOpened = FALSE;
}

int
fbdevVideoV4l2GetHandleNums (void)
{
	return DEVICE_NUMS;
}

Bool
fbdevVideoV4l2HandleOpened (int index)
{
	return_val_if_fail (index < DEVICE_NUMS, FALSE);

	return device_infos[index].bOpened;
}

void *
fbdevVideoV4l2OpenHandle (ScreenPtr pScreen, int index, int requestbuffer)
{
	FBDevDispHandle *handle;

	return_val_if_fail (pScreen != NULL, NULL);
	return_val_if_fail (index < DEVICE_NUMS, NULL);

	handle = (FBDevDispHandle*)calloc (sizeof (FBDevDispHandle), 1);

	return_val_if_fail (handle != NULL, NULL);

	handle->pScreen = pScreen;
	handle->index = index;
	handle->status.hw_rotate = -1;
	handle->status.id     = 0;
	handle->status.cur_idx = -1;
	handle->status.init = 0;
	handle->memory = V4L2_MEMORY_MMAP;
	handle->video_fd = -1;

	if (!_fbdevVideoV4l2OpenDevice (handle, index))
	{
		free (handle);
		return NULL;
	}

	DRVLOG ("[OpenHandle, %p] Handle(%d) opened. \n", handle, index);

	return handle;
}

void
fbdevVideoV4l2CloseHandle (void *handle)
{
	FBDevDispHandle *hdisp = (FBDevDispHandle*)handle;

	return_if_fail (handle != NULL);

	_fbdevVideoV4l2CloseDevice ((FBDevDispHandle*)handle);

	if (hdisp->status.backup)
	{
		free (hdisp->status.backup);
		hdisp->status.backup = NULL;
	}

	DRVLOG ("[CloseHandle, %p] Handle(%d) closed. \n", hdisp, hdisp->index);

	free (handle);
}

Bool
fbdevVideoV4l2StreamOn (void *handle)
{
	FBDevDispHandle *hdisp = (FBDevDispHandle*)handle;
	uint phy_addrs[3];

	return_val_if_fail (hdisp != NULL, FALSE);

	hdisp->re_setting = TRUE;

	if (!_fbdevVideoV4l2OpenDevice (hdisp, hdisp->index))
		return FALSE;

	fbdevVideoV4l2SetFormat (handle,
	                         &hdisp->status.img,
	                         &hdisp->status.crop,
	                         &hdisp->status.dst,
	                         hdisp->status.id,
	                         hdisp->status.scn_rotate,
	                         hdisp->status.hw_rotate,
	                         hdisp->status.hflip,
	                         hdisp->status.vflip,
	                         BUF_NUM,
	                         hdisp->status.sync);

	phy_addrs[0] = hdisp->fimcbuf.base[0];
	phy_addrs[1] = hdisp->fimcbuf.base[1];
	phy_addrs[2] = hdisp->fimcbuf.base[2];

	if (hdisp->status.backup)
	{
		int size;

		size = fbdevVideoQueryImageAttributes (NULL, FOURCC_RGB32,
		                                       (unsigned short*)&hdisp->status.dst.width,
		                                       (unsigned short*)&hdisp->status.dst.height,
		                                       NULL, NULL);

		memcpy (hdisp->dst_buf[0].buf, hdisp->status.backup, size);

		free (hdisp->status.backup);
		hdisp->status.backup = NULL;
	}

	hdisp->re_setting = FALSE;

	DRVLOG ("%s \n", __FUNCTION__);

	return TRUE;
}

void
fbdevVideoV4l2StreamOff (void *handle)
{
	FBDevDispHandle *hdisp = (FBDevDispHandle*)handle;

	return_if_fail (hdisp != NULL);

	if (hdisp->memory == V4L2_MEMORY_MMAP)
	{
		int size;

		if (hdisp->status.backup)
		{
			free (hdisp->status.backup);
			hdisp->status.backup = NULL;
		}

		size = fbdevVideoQueryImageAttributes (NULL, FOURCC_RGB32,
		                                       (unsigned short*)&hdisp->status.dst.width,
		                                       (unsigned short*)&hdisp->status.dst.height,
		                                       NULL, NULL);

		if (size > 0)
			hdisp->status.backup = malloc (size);

		if (hdisp->status.backup &&
		        hdisp->dst_buf &&
		        hdisp->dst_buf[hdisp->status.queued_index].buf)
		{
			memcpy (hdisp->status.backup,
			        hdisp->dst_buf[hdisp->status.queued_index].buf,
			        size);
		}
	}

	_fbdevVideoV4l2CloseDevice (hdisp);

	DRVLOG ("%s \n", __FUNCTION__);
}

void
fbdevVideoV4l2VideoOffset (void *handle, int x, int y)
{
	FBDevDispHandle *hdisp = (FBDevDispHandle*)handle;

	return_if_fail (handle != NULL);

	DRVLOG ("[VideoOffset, %p] to %d,%d\n", hdisp, x, y);

	if (hdisp->offset_x == x && hdisp->offset_y == y)
		return;

	if (!_fbdevVideoV4l2SetDst (hdisp->video_fd, x, y, &hdisp->status.dst))
	{
		xf86DrvMsg (0, X_ERROR, "[VideoOffset, %p] _fbdevVideoV4l2SetDst is failed. \n", hdisp);
		return;
	}

	hdisp->offset_x = x;
	hdisp->offset_y = y;
}

/* img->x, img->y : not used. */
Bool
fbdevVideoV4l2SetFormat (void *handle,
                         xRectangle *img, xRectangle *crop, xRectangle *dst,
                         uint id,
                         int  scn_rotate,
                         int  hw_rotate,
                         int  hflip,
                         int  vflip,
                         int  requestbuffer,
                         Bool sync)
{
	FBDevDispHandle *hdisp = (FBDevDispHandle*)handle;
	Bool src_changed = FALSE;
	Bool dst_changed = FALSE;
	Bool buf_changed = FALSE;

	return_val_if_fail (handle != NULL, FALSE);
	return_val_if_fail (img != NULL, FALSE);
	return_val_if_fail (crop != NULL, FALSE);
	return_val_if_fail (dst != NULL, FALSE);

	DRVLOG ("[SetFormat, %p] try to set : img(%d,%d %dx%d) crop(%d,%d %dx%d) dst(%d,%d %dx%d) id(%x), rot(%d), flip(%d,%d) req(%d)\n", hdisp,
	        img->x, img->y, img->width, img->height,
	        crop->x, crop->y, crop->width, crop->height,
	        dst->x, dst->y, dst->width, dst->height,
	        id, hw_rotate, hflip, vflip, requestbuffer);

	if (memcmp (&hdisp->status.img, img, sizeof (xRectangle)) ||
	        memcmp (&hdisp->status.crop, crop, sizeof (xRectangle)) ||
	        hdisp->status.id != id)
		src_changed = TRUE;

	if (memcmp (&hdisp->status.dst, dst, sizeof (xRectangle)) ||
	        hdisp->status.hw_rotate != hw_rotate ||
	        hdisp->status.hflip != hflip ||
	        hdisp->status.vflip != vflip)
		dst_changed = TRUE;

	if (hdisp->status.init && (src_changed || dst_changed || buf_changed))
	{
		_fbdevVideoV4l2CloseDevice (hdisp);
		_fbdevVideoV4l2OpenDevice (hdisp, hdisp->index);

		DRVLOG ("[SetFormat, %p] changed : img(%d) dst(%d) buf(%d) \n", hdisp,
		        src_changed, dst_changed, buf_changed);

		/* After close device, below all steps should be done. */
		src_changed = dst_changed = buf_changed = TRUE;
		hdisp->status.init = 0;
	}

	if (hdisp->re_setting)
		src_changed = dst_changed = buf_changed = TRUE;

	if (src_changed || dst_changed || buf_changed)
	{
		fbdevVideoV4l2GetFormatInfo (id, NULL, &hdisp->pixfmt, &hdisp->memory);

		DRVLOG ("[SetFormat, %p] id(%c%c%c%c) => pixfmt(%d) memory(%d) \n", hdisp,
		        id & 0xFF, (id & 0xFF00) >> 8, (id & 0xFF0000) >> 16,  (id & 0xFF000000) >> 24,
		        hdisp->pixfmt, hdisp->memory);

		hdisp->status.img  = *img;
		hdisp->status.crop = *crop;
		hdisp->status.id   = id;
		hdisp->status.sync = sync;
		hdisp->status.init = 1;

		hdisp->status.cur_idx = 0;
		hdisp->status.hw_rotate = hw_rotate;
		hdisp->status.scn_rotate = scn_rotate;
		hdisp->status.hflip = hflip;
		hdisp->status.vflip = vflip;
		hdisp->status.dst = *dst;

		if (!_fbdevVideoV4l2SetDst (hdisp->video_fd, hdisp->offset_x, hdisp->offset_y, dst))
		{
			xf86DrvMsg (0, X_ERROR, "[SetFormat, %p] _fbdevVideoV4l2SetDst is failed. \n", hdisp);
			return FALSE;
		}

		if (!_fbdevVideoV4l2SetBuffer (hdisp->video_fd,
		                               V4L2_BUF_TYPE_VIDEO_OVERLAY,
		                               hdisp->memory,
		                               BUF_NUM,
		                               &hdisp->dst_buf))
		{
			xf86DrvMsg (0, X_ERROR, "[SetFormat, %p] _fbdevVideoV4l2SetBuffer is failed. \n", hdisp);
			return FALSE;
		}

		hdisp->status.cur_idx = 0;
	}

	if (!hdisp->status.streamon)
	{
		_fbdevVideoV4l2StreamOn (hdisp->video_fd);
		hdisp->status.streamon = TRUE;
	}

	return TRUE;
}

XF86ImagePtr
fbdevVideoV4l2SupportImages (int *count)
{
	if (count)
		*count = NUM_IMAGES;

	return Images;
}

int
fbdevVideoV4l2Draw (void *handle, uchar *buf, uint *phy_addrs)
{
	FBDevDispHandle *hdisp = (FBDevDispHandle*)handle;
	pixman_format_code_t src_format, dst_format;
	xRectangle img = hdisp->status.img;
	xRectangle src = hdisp->status.crop;
	xRectangle pxm = hdisp->status.dst;
	xRectangle dst = hdisp->status.dst;

	pxm.x = pxm.y = dst.x = dst.y = 0;

	switch (hdisp->pixfmt)
	{
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		src_format = PIXMAN_yv12;
		break;
	case V4L2_PIX_FMT_YUYV:
		src_format = PIXMAN_yuy2;
		break;
	case V4L2_PIX_FMT_RGB565:
		src_format = PIXMAN_r5g6b5;
		break;
	case V4L2_PIX_FMT_RGB24:
		src_format = PIXMAN_r8g8b8;
		break;
	case V4L2_PIX_FMT_RGB32:
		src_format = PIXMAN_a8r8g8b8;
		break;
	default:
		return FALSE;
	}

	switch (hdisp->pixfmt)
	{
	case V4L2_PIX_FMT_YUV420:
		dst_format = PIXMAN_x8b8g8r8;
		break;
	default:
		dst_format = PIXMAN_x8r8g8b8;
		break;
	}

	/* support only RGB  */
	fbdev_pixman_convert_image (PIXMAN_OP_SRC,
	                            buf, hdisp->dst_buf[0].buf,
	                            src_format, dst_format,
	                            &img, &pxm, &src, &dst,
	                            NULL, (hdisp->status.hw_rotate + (360 - hdisp->status.scn_rotate)) % 360,
	                            hdisp->status.hflip, hdisp->status.vflip);

	return TRUE;
}

Bool
fbdevVideoV4l2GetFormatInfo (int id, int *type, uint *pixelformat, FBDevV4l2Memory *memory)
{
	int i;

	for (i = 0; i < sizeof (format_infos) / sizeof (FormatInfo); i++)
		if (format_infos[i].id == id)
		{
			if (type)
				*type = format_infos[i].type;
			if (pixelformat)
				*pixelformat = format_infos[i].pixelformat;
			if (memory)
				*memory = format_infos[i].memory;
			return TRUE;
		}

	return FALSE;
}

/*  img : original src size
 *  src : real src size (cropped area inside img)
 *  dst : real dst size
 *        original dst size (in case that image is drawn on opened framebuffer)
 */
Bool
fbdevVideoV4l2CheckSize (void *handle, uint pixelformat,
                         xRectangle *img, xRectangle *src, xRectangle *dst,
                         int type, int memory)
{
	/* img */
	if (img)
	{
		if (img->width % 16)
			xf86DrvMsg (0, X_WARNING, "img's width(%d) is not multiple of 16!!!\n", img->width);

		if (type == TYPE_YUV420 && img->height % 2)
			xf86DrvMsg (0, X_WARNING, "img's height(%d) is not multiple of 2!!!\n", img->height);

		return_val_if_fail (img->width >= 16, FALSE);
		return_val_if_fail (img->height >= 16, FALSE);
	}

	/* src */
	if (src)
	{
		if (type == TYPE_YUV420 || type == TYPE_YUV422)
		{
			src->x = src->x & (~0x1);
			src->width = src->width & (~0x1);
		}

		if (type == TYPE_YUV420)
			src->height = src->height & (~0x1);

		return_val_if_fail (src->width >= 16, FALSE);
		return_val_if_fail (src->height >= 16, FALSE);
	}

	/* dst */
	if (dst)
	{
		dst->width = dst->width & (~0x1);
		dst->height = dst->height & (~0x1);

		return_val_if_fail (dst->width >= 8, FALSE);
		return_val_if_fail (dst->height >= 8, FALSE);
	}

	return TRUE;
}

void
fbdevVideoGetFBInfo (void *handle, void **base, xRectangle *pos)
{
	FBDevDispHandle *hdisp = (FBDevDispHandle*)handle;

	if (!hdisp)
		return;

	if (pos)
		*pos = hdisp->status.dst;
	if (base)
		*base = hdisp->dst_buf[0].buf;
}
