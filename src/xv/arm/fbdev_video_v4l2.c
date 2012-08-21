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

typedef struct _DeviceInfo
{
	char *video;     /* video */
	char *fb;        /* output frame buffer */
	int   type;
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

	int       fb_fd;
	char     *fb_base;
	Bool      fb_shown;

	struct
	{
		int init;
		xRectangle img;
		xRectangle crop;
		xRectangle pxm;
		xRectangle dst;
		int id;
		int hw_rotate;
		int scn_rotate;
		int hflip;
		int vflip;
		int requestbuffer;
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
	FBDevV4l2Memory     memory;

	int initial_dequeued_buf;
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
	{ FOURCC_S420,      TYPE_YUV420,    V4L2_PIX_FMT_YUV420,    V4L2_MEMORY_USERPTR },
	{ FOURCC_ST12,      TYPE_YUV420,    V4L2_PIX_FMT_NV12T,     V4L2_MEMORY_USERPTR },
	{ FOURCC_NV12,      TYPE_YUV420,    V4L2_PIX_FMT_NV12,      V4L2_MEMORY_MMAP },
	{ FOURCC_SN12,      TYPE_YUV420,    V4L2_PIX_FMT_NV12,      V4L2_MEMORY_USERPTR },
	{ FOURCC_YUY2,      TYPE_YUV422,    V4L2_PIX_FMT_YUYV,      V4L2_MEMORY_MMAP },
	{ FOURCC_SUYV,      TYPE_YUV422,    V4L2_PIX_FMT_YUYV,      V4L2_MEMORY_USERPTR },
};

static XF86ImageRec Images[] =
{
	XVIMAGE_YUY2,
	XVIMAGE_SUYV,
	XVIMAGE_I420,
	XVIMAGE_S420,
	XVIMAGE_ST12,
	XVIMAGE_NV12,
	XVIMAGE_SN12,
	XVIMAGE_RGB32,
	XVIMAGE_RGB565,
};

static DeviceInfo device_infos[] =
{
	{ "/dev/video2", "/dev/fb1", OUTPUT_PATH_DMA, FALSE },
	{ "/dev/video3", "/dev/fb2", OUTPUT_PATH_DMA, FALSE },
};

#define NUM_IMAGES        (sizeof(Images) / sizeof(XF86ImageRec))
#define DEVICE_NUMS (sizeof (device_infos) / sizeof (DeviceInfo))

static Bool
_fbdevVideoV4l2SetSrc (int         fd,
                       xRectangle *src_rect,
                       xRectangle *crop_rect,
                       uint        pixelformat)
{
	struct v4l2_format	format = {0,};
	struct v4l2_crop	crop = {0,};
	int capabilities;

	return_val_if_fail (fd >= 0, FALSE);

	DRVLOG ("[SetSrc] src_r(%d,%d %dx%d) crop_r(%d,%d %dx%d) pixfmt(0x%08x) \n",
	        src_rect->x, src_rect->y, src_rect->width, src_rect->height,
	        crop_rect->x, crop_rect->y, crop_rect->width, crop_rect->height,
	        pixelformat);

	/* check if capabilities is valid */
	capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OVERLAY;
	if (!fbdev_v4l2_querycap (fd, capabilities))
		return FALSE;

	/* set format */
	CLEAR (format);
	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	if (!fbdev_v4l2_g_fmt (fd, &format))
		return FALSE;

	format.fmt.pix.width       = src_rect->width;
	format.fmt.pix.height      = src_rect->height;
	format.fmt.pix.pixelformat = pixelformat;
	format.fmt.pix.field       = V4L2_FIELD_NONE;
	if (!fbdev_v4l2_s_fmt (fd, &format))
		return FALSE;

	/* set crop_rect */
	CLEAR (crop);
	crop.type     = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	crop.c.left   = crop_rect->x;
	crop.c.top    = crop_rect->y;
	crop.c.width  = crop_rect->width;
	crop.c.height = crop_rect->height;
	if (!fbdev_v4l2_cropcap (fd, crop.type, &crop.c))
		return FALSE;

	if (!fbdev_v4l2_s_crop (fd, &crop))
		return FALSE;

	return TRUE;
}

static Bool
_fbdevVideoV4l2SetDst (int          fd,
                       xRectangle  *dst_rect,
                       xRectangle  *win_rect,
                       int          rotation,
                       int          hflip,
                       int          vflip,
                       int          path,
                       uint         addr)
{
	struct v4l2_format		format = {0,};
	struct v4l2_control		ctrl = {0,};
	struct v4l2_framebuffer	fbuf = {0,};

	return_val_if_fail (fd >= 0, FALSE);

	DRVLOG ("[SetDst] dst_r(%d,%d %dx%d) win_r(%d,%d %dx%d) rot(%d) flip(%d,%d) path(%d) addr(%p) \n",
	        dst_rect->x, dst_rect->y, dst_rect->width, dst_rect->height,
	        win_rect->x, win_rect->y, win_rect->width, win_rect->height,
	        rotation, hflip, vflip, path, (void*)addr);

	/* set rotation */
	CLEAR (ctrl);
	ctrl.id    = V4L2_CID_ROTATION;
	ctrl.value = rotation;
	if (!fbdev_v4l2_s_ctrl (fd, &ctrl))
		return FALSE;

	CLEAR (ctrl);
	ctrl.id    = V4L2_CID_HFLIP;
	ctrl.value = hflip;
	if (!fbdev_v4l2_s_ctrl (fd, &ctrl))
		return FALSE;

	CLEAR (ctrl);
	ctrl.id    = V4L2_CID_VFLIP;
	ctrl.value = vflip;
	if (!fbdev_v4l2_s_ctrl (fd, &ctrl))
		return FALSE;

	ctrl.id    = V4L2_CID_OVLY_MODE;
	ctrl.value = 0x4;  //single buffer
	if (!fbdev_v4l2_s_ctrl (fd, &ctrl))
		return FALSE;

	/* set framebuffer */
	CLEAR (fbuf);
	if (!fbdev_v4l2_g_fbuf (fd, &fbuf))
		return FALSE;

	if (path == OUTPUT_PATH_DMA)
		fbuf.base = (void*)addr;

	fbuf.fmt.width  = dst_rect->width;
	fbuf.fmt.height = dst_rect->height;
	fbuf.fmt.pixelformat = V4L2_PIX_FMT_RGB32;
	if (!fbdev_v4l2_s_fbuf (fd, &fbuf))
		return FALSE;

	/* set format */
	CLEAR (format);
	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	if (!fbdev_v4l2_g_fmt (fd, &format))
		return FALSE;

	format.fmt.win.w.left   = win_rect->x;
	format.fmt.win.w.top    = win_rect->y;
	format.fmt.win.w.width  = win_rect->width;
	format.fmt.win.w.height = win_rect->height;
	if (!fbdev_v4l2_s_fmt (fd, &format))
		return FALSE;

	return TRUE;
}

static Bool
_fbdevVideoV4l2SetBuffer (int fd,
                          FBDevV4l2BufType type,
                          FBDevV4l2Memory memory,
                          int num_buf,
                          FBDevV4l2SrcBuffer **mem_buf)
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
		FBDevV4l2SrcBuffer *out_buf;
		int i;

		return_val_if_fail (mem_buf != NULL, FALSE);

		out_buf = calloc (req.count, sizeof (FBDevV4l2SrcBuffer));
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
			                          MAP_SHARED , fd, buffer.m.offset);
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
                            FBDevV4l2SrcBuffer *mem_buf)
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
_fbdevVideoV4l2StreamOn (int fd, FBDevV4l2BufType type)
{
	return_val_if_fail (fd >= 0, FALSE);

	DRVLOG ("[StreamOn] type(%d) \n", type);

	if (!fbdev_v4l2_streamon (fd, type))
		return FALSE;

	return TRUE;
}


static Bool
_fbdevVideoV4l2StreamOff (int fd, FBDevV4l2BufType type)
{
	return_val_if_fail (fd >= 0, FALSE);

	DRVLOG ("[StreamOff] type(%d) \n", type);

	if (!fbdev_v4l2_streamoff (fd, type))
		return FALSE;

	return TRUE;
}

static int
_fbdevVideoV4l2Dequeue (int fd, FBDevV4l2BufType type, FBDevV4l2Memory memory)
{
	struct v4l2_buffer buf = {0,};

	return_val_if_fail (fd >= 0, -1);

	CLEAR (buf);
	buf.type   = type;
	buf.memory = memory;

	if (!fbdev_v4l2_dequeue (fd, &buf))
		return FALSE;

	DRVLOG ("[Dequeue] index(%d) type(%d) memory(%d)\n",
	        buf.index, type, memory);

	return buf.index;
}

static int
_fbdevVideoV4l2Queue (int fd, FBDevV4l2BufType type, FBDevV4l2Memory memory, int index, FBDevV4l2FimcBuffer *fimc_buf)
{
	struct v4l2_buffer buf = {0,};

	return_val_if_fail (fd >= 0, -1);

	CLEAR (buf);
	buf.index  = index;
	buf.type   = type;
	buf.memory = memory;

	if (memory == V4L2_MEMORY_USERPTR)
		buf.m.userptr	= (unsigned long) fimc_buf;

	if (!fbdev_v4l2_queue (fd, &buf))
		return FALSE;

	DRVLOG ("[Queue] index(%d) type(%d) memory(%d)\n",
	        index, type, memory);

	return index;
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
_move_resize_fb (FBDevDispHandle *hdisp, int x, int y, int width, int height)
{
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;

	return_val_if_fail (hdisp->fb_fd >= 0, FALSE);

	DRVLOG ("[MoveResize] '%s' to (%d,%d %dx%d) \n",
	        device_infos[hdisp->index].fb, x, y, width, height);

	if (!fbdevFbGetVarScreenInfo (hdisp->fb_fd, &var))
		return FALSE;

	var.xres = var.xres_virtual = width;
	var.yres = var.yres_virtual = height;
	var.transp.length = 0;
	var.activate = FB_ACTIVATE_FORCE;
	if (!fbdevFbSetVarScreenInfo (hdisp->fb_fd, &var))
		return FALSE;

	if (!fbdevFbGetFixScreenInfo (hdisp->fb_fd, &fix))
		return FALSE;

	if (fix.smem_len == 0)
		return FALSE;

	hdisp->fb_base = (void*)fix.smem_start;  /* Physical address */
	if (!hdisp->fb_base)
		return FALSE;

	if (!fbdevFbSetWinPosition (hdisp->fb_fd, x, y)) /* 1 : auto */
		return FALSE;

	return TRUE;
}

static Bool
_fbdevVideoV4l2EnsureStreamOff (FBDevDispHandle *hdisp)
{
	if (!hdisp->status.streamon)
		return TRUE;

	if (!_fbdevVideoV4l2StreamOff (hdisp->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT))
		return FALSE;

	hdisp->status.streamon = FALSE;

	return TRUE;
}

static Bool
_fbdevVideoV4l2OpenDevice (FBDevDispHandle *hdisp, int index, int requestbuffer)
{
	if (device_infos[index].bOpened)
	{
		DRVLOG ("[OpenDevice, %p] Already opened : %s\n", hdisp, device_infos[index].video);
		return FALSE;
	}

	hdisp->video_fd = _open_device (device_infos[index].video);
	if (hdisp->video_fd < 0)
		return FALSE;

	if (device_infos[index].type == OUTPUT_PATH_DMA)
	{
		hdisp->fb_fd = _open_device (device_infos[index].fb);
		if (hdisp->fb_fd < 0)
		{
			_close_device (hdisp->video_fd);
			hdisp->video_fd = -1;
			return FALSE;
		}
	}

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

	if (hdisp->status.requestbuffer > 0)
	{
		if (hdisp->video_fd >= 0)
			if (!_fbdevVideoV4l2ClearBuffer (hdisp->video_fd,
			                                 V4L2_BUF_TYPE_VIDEO_OUTPUT,
			                                 hdisp->memory,
			                                 hdisp->status.requestbuffer,
			                                 hdisp->src_buf))
				xf86DrvMsg (0, X_WARNING, "[CloseDevice, %p] Warning : Cannot clear buffer!! \n", hdisp);

		hdisp->status.requestbuffer = 0;
		hdisp->src_buf = NULL;
	}

	if (hdisp->video_fd >= 0)
	{
		_close_device (hdisp->video_fd);
		hdisp->video_fd = -1;
	}

	if (hdisp->fb_fd >= 0)
	{
		fbdevFbDeActivate (hdisp->fb_fd);
		_close_device (hdisp->fb_fd);
		hdisp->fb_fd = -1;
		hdisp->fb_base = NULL;
		hdisp->fb_shown = FALSE;
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
	handle->initial_dequeued_buf = requestbuffer;
	handle->video_fd = -1;
	handle->fb_fd = -1;

	if (!_fbdevVideoV4l2OpenDevice (handle, index, requestbuffer))
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

	if (!_fbdevVideoV4l2OpenDevice (hdisp, hdisp->index, hdisp->status.requestbuffer))
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
	                         hdisp->status.requestbuffer,
	                         hdisp->status.sync);

	phy_addrs[0] = hdisp->fimcbuf.base[0];
	phy_addrs[1] = hdisp->fimcbuf.base[1];
	phy_addrs[2] = hdisp->fimcbuf.base[2];

	fbdevVideoV4l2Draw (handle, hdisp->status.backup, phy_addrs);

	if (hdisp->status.backup)
	{
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
	int requestbuffer;

	return_if_fail (hdisp != NULL);

	if (hdisp->memory == V4L2_MEMORY_MMAP)
	{
		int size;

		if (hdisp->status.backup)
		{
			free (hdisp->status.backup);
			hdisp->status.backup = NULL;
		}

		size = fbdevVideoQueryImageAttributes (NULL, hdisp->status.id,
		                                       (unsigned short*)&hdisp->status.img.width,
		                                       (unsigned short*)&hdisp->status.img.height,
		                                       NULL, NULL);

		if (size > 0)
			hdisp->status.backup = malloc (size);

		if (hdisp->status.backup &&
		        hdisp->src_buf &&
		        hdisp->src_buf[hdisp->status.queued_index].buf)
			memcpy (hdisp->status.backup,
			        hdisp->src_buf[hdisp->status.queued_index].buf,
			        size);
	}

	requestbuffer = hdisp->status.requestbuffer;

	_fbdevVideoV4l2CloseDevice (hdisp);

	hdisp->status.requestbuffer = requestbuffer;

	DRVLOG ("%s \n", __FUNCTION__);
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

	if (hdisp->status.requestbuffer != requestbuffer)
		buf_changed = TRUE;

	if (hdisp->status.init && (src_changed || dst_changed || buf_changed))
	{
		_fbdevVideoV4l2CloseDevice (hdisp);
		_fbdevVideoV4l2OpenDevice (hdisp, hdisp->index, requestbuffer);

		DRVLOG ("[SetFormat, %p] changed : img(%d) dst(%d) buf(%d) \n", hdisp,
		        src_changed, dst_changed, buf_changed);

		/* After close device, below all steps should be done. */
		src_changed = dst_changed = buf_changed = TRUE;
		hdisp->status.init = 0;
	}

	if (hdisp->re_setting)
		src_changed = dst_changed = buf_changed = TRUE;

	if (src_changed)
	{
		fbdevVideoV4l2GetFormatInfo (id, NULL, &hdisp->pixfmt, &hdisp->memory);

		DRVLOG ("[SetFormat, %p] id(%c%c%c%c) => pixfmt(%d) memory(%d) \n", hdisp,
		        id & 0xFF, (id & 0xFF00) >> 8, (id & 0xFF0000) >> 16,  (id & 0xFF000000) >> 24,
		        hdisp->pixfmt, hdisp->memory);

		if (img->width % 16)
			xf86DrvMsg (0, X_WARNING, "img->width(%d) is not multiple of 16!!!\n", img->width);

		if (!_fbdevVideoV4l2SetSrc (hdisp->video_fd, img, crop, hdisp->pixfmt))
		{
			xf86DrvMsg (0, X_ERROR, "[SetFormat, %p] _fbdevVideoV4l2SetSrc is failed. \n", hdisp);
			return FALSE;
		}

		hdisp->status.img  = *img;
		hdisp->status.crop = *crop;
		hdisp->status.id   = id;
		hdisp->status.sync = sync;

		hdisp->status.requestbuffer = 0;
		hdisp->status.init = 1;
	}

	if (dst_changed)
	{
		hdisp->status.cur_idx = 0;
		hdisp->status.hw_rotate = hw_rotate;
		hdisp->status.scn_rotate = scn_rotate;
		hdisp->status.hflip = hflip;
		hdisp->status.vflip = vflip;
		hdisp->status.dst = *dst;

		if (hdisp->fb_fd >= 0)
		{
			xRectangle fb_rect = *dst;
			if (!_move_resize_fb (hdisp, fb_rect.x, fb_rect.y, fb_rect.width, fb_rect.height))
			{
				xf86DrvMsg (0, X_ERROR, "[SetFormat, %p] _move_resize_fb is failed. \n", hdisp);
				return FALSE;
			}
		}

		fbdev_util_rotate_rect ((int)hdisp->pScreen->width,
		                        (int)hdisp->pScreen->height,
		                        0, (hw_rotate + (360 - scn_rotate)) % 360, dst);
		if (hdisp->fb_base)
		{
			xRectangle win_rect = {0, 0, dst->width, dst->height};

			if (!_fbdevVideoV4l2SetDst (hdisp->video_fd, dst, &win_rect, hw_rotate, hflip, vflip, OUTPUT_PATH_DMA, (uint)hdisp->fb_base))
			{
				xf86DrvMsg (0, X_ERROR, "[SetFormat, %p] _fbdevVideoV4l2SetDst is failed. \n", hdisp);
				return FALSE;
			}
		}
		else
		{
			if (!_fbdevVideoV4l2SetDst (hdisp->video_fd, dst, dst, hw_rotate, hflip, vflip, OUTPUT_PATH_FIMD, 0))
			{
				xf86DrvMsg (0, X_ERROR, "[SetFormat, %p] _fbdevVideoV4l2SetDst is failed. \n", hdisp);
				return FALSE;
			}
		}
	}

	if (buf_changed)
	{
		if (!_fbdevVideoV4l2SetBuffer (hdisp->video_fd,
		                               V4L2_BUF_TYPE_VIDEO_OUTPUT,
		                               hdisp->memory,
		                               requestbuffer,
		                               &hdisp->src_buf))
		{
			xf86DrvMsg (0, X_ERROR, "[SetFormat, %p] _fbdevVideoV4l2SetBuffer is failed. \n", hdisp);
			return FALSE;
		}

		hdisp->status.cur_idx = 0;
		hdisp->status.requestbuffer = requestbuffer;
		hdisp->initial_dequeued_buf = requestbuffer;
	}

	if (!hdisp->status.streamon)
	{
		_fbdevVideoV4l2StreamOn (hdisp->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT);
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
	int idx;

	return_val_if_fail (handle != NULL, FALSE);
//	return_val_if_fail (phy_addrs != NULL, FALSE);

	if (!hdisp->status.sync)
	{
		if (hdisp->initial_dequeued_buf > 0)
		{
			idx = hdisp->status.requestbuffer - hdisp->initial_dequeued_buf;
			hdisp->initial_dequeued_buf--;
		}
		else
		{
			if ((idx = _fbdevVideoV4l2Dequeue (hdisp->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, hdisp->memory)) < 0)
			{
				xf86DrvMsg (0, X_ERROR, "[Draw, %p] _fbdevVideoV4l2Dequeue is failed. \n", hdisp);
				return FALSE;
			}
		}
	}
	else
		idx = hdisp->status.queued_index;

	if (hdisp->memory == V4L2_MEMORY_MMAP)
	{
		uchar *destbuf = hdisp->src_buf[idx].buf;
		int size;

		return_val_if_fail (buf != NULL, FALSE);

		hdisp->status.last_buffer = destbuf;

		size = fbdevVideoQueryImageAttributes (NULL, hdisp->status.id,
		                                       (unsigned short*)&hdisp->status.img.width,
		                                       (unsigned short*)&hdisp->status.img.height,
		                                       NULL, NULL);
		memcpy (destbuf, buf, size);
	}

	hdisp->fimcbuf.base[0] = phy_addrs[0];
	hdisp->fimcbuf.base[1] = phy_addrs[1];
	hdisp->fimcbuf.base[2] = phy_addrs[2];
	hdisp->fimcbuf.length[0] = hdisp->status.img.width * hdisp->status.img.height;
	hdisp->fimcbuf.length[1] = hdisp->fimcbuf.length[0] >> 1;
	hdisp->fimcbuf.length[2] = hdisp->fimcbuf.base[1] + hdisp->fimcbuf.length[1];

	if ((idx = _fbdevVideoV4l2Queue (hdisp->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT,
	                                 hdisp->memory, idx, &hdisp->fimcbuf)) < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[Draw, %p] _fbdevVideoV4l2Queue is failed. \n", hdisp);
		return FALSE;
	}

	if (hdisp->status.sync)
	{
		if ((idx = _fbdevVideoV4l2Dequeue (hdisp->video_fd, V4L2_BUF_TYPE_VIDEO_OUTPUT, hdisp->memory)) < 0)
		{
			xf86DrvMsg (0, X_ERROR, "[Draw, %p] _fbdevVideoV4l2Dequeue is failed. \n", hdisp);
			return FALSE;
		}
	}

	if (hdisp->fb_fd >= 0 && !hdisp->fb_shown)
	{
		if (!fbdevFbActivate (hdisp->fb_fd))
		{
			xf86DrvMsg (0, X_ERROR, "[%s, %p] %d failed. \n", __FUNCTION__, hdisp, __LINE__);
			return FALSE;
		}

		hdisp->fb_shown = TRUE;
	}

	hdisp->status.queued_index = idx + 1;
	if (hdisp->status.queued_index > hdisp->status.requestbuffer - 1)
		hdisp->status.queued_index = 0;

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
