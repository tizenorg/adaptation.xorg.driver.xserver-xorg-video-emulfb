/**************************************************************************

xserver-xorg-video-emulfb

Copyright 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: YoungHoon Jung <yhoon.jung@samsung.com>

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/vt.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include "fbdev_pixman.h"
#include "v4l2config.h"
#include "v4l2_output_api.h"

typedef struct _VideoInfo
{
	char *video;     /* video */
	char *fb;        /* output frame buffer */
	int   bOpened;
} VideoInfo;

typedef struct _FbPos
{
	int x;
	int y;
} FbPos;

typedef struct _FbGeometry
{
	FbPos pos;
	int   width;
	int   height;
} FbGeometry;

typedef struct
{
	int index;

	int         video_fd;
	int         fb_fd;
	char       *fb_base;
	FbGeometry  fb_geometry;

	struct
	{
		int init;
		int w;
		int h;
		CRECT dest;
		CRECT crop;
		V4L2Videoformat format;
		int rotate;
		int hflip;
		int vflip;
		int requestbuffer;
		int streamon;
		int cur_idx;

		void * last_buffer;
	} status;

	struct v4l2_src_buffer *src_buf;
	struct v4l2_dst_buffer *dst_buf;
	int ismmap;
} DispHandle;

static VideoInfo video_infos[] =
{
	{ "/dev/video1", NULL, FALSE },
	{ "/dev/video2", NULL, FALSE },
};

#define INVALID_INDEX      -1

#ifndef SWAP
#define SWAP(a, b) {int t; t = a; a = b; b = t;}
#endif

static int EnsureOverlayOff (int video_fd, int * streamon);

int num_of_v4l2_devices = -1;

static int
OpenDisplay (void *pphandle, int index, int requestbuffer)
{
	DispHandle *hdisp = (DispHandle *)pphandle;

	if (video_infos[index].bOpened)
	{
		ErrorF ("V4L2: Already opened : %s\n", video_infos[index].video);
		return INVALID_INDEX;
	}

	if ((hdisp->video_fd = open (video_infos[index].video, O_RDWR)) <= 0)
	{
		ErrorF("V4L2: Cannot open device : %s\n", video_infos[index].video);
		return INVALID_INDEX;
	}

	video_infos[index].bOpened = TRUE;

	hdisp->status.rotate = -1;
	hdisp->status.format = -1;
	hdisp->status.cur_idx = -1;
	hdisp->status.init = 0;
	hdisp->ismmap = 1;

	hdisp->index = index;

	return index;
}

static int
CloseDisplay (DispHandle *hdisp)
{
	if (hdisp->index < 0 || hdisp->index >= num_of_v4l2_devices)
		return FALSE;

	if (!EnsureOverlayOff (hdisp->video_fd, &hdisp->status.streamon)) // We will consider this as a LCD Off case.
	{
		return FALSE;
	}

	if (hdisp->status.requestbuffer > 0 &&
	    v4l2_clr_buf (hdisp->video_fd, hdisp->status.requestbuffer, hdisp->dst_buf, V4L2_BUF_TYPE_VIDEO_OVERLAY, hdisp->ismmap) < 0 )
	{
		ErrorF ("V4L2: v4l2_clr_buf error\n");
		return FALSE;
	}

	if (v4l2_close_fd (hdisp->video_fd) < 0)
	{
		ErrorF("V4L2: Cannot close device\n");
		return FALSE;
	}

	video_infos[hdisp->index].bOpened = FALSE;

	return TRUE;
}

int
GetDisplayNums (void)
{
#if ENABLE_V4L2
	return (sizeof(video_infos) / sizeof(VideoInfo));
#else
	return 0;
#endif
}

int
AvailableDisplay (int index)
{
	return !video_infos[index].bOpened;
}

int
CreateDisplay (void **pphandle, int index, int requestbuffer)
{
	if (num_of_v4l2_devices == -1)
		num_of_v4l2_devices = sizeof(video_infos) / sizeof(VideoInfo);

	if (pphandle == NULL)
		return INVALID_INDEX;

	if (index >= num_of_v4l2_devices)
	{
		ErrorF ("V4L2: Out of range : %d\n", index);
		return INVALID_INDEX;
	}

	*pphandle = calloc(sizeof(DispHandle), 1);

	if (*pphandle == NULL)
		return INVALID_INDEX;

	index = OpenDisplay(*pphandle, index, requestbuffer);
	if (index == INVALID_INDEX)
	{
		free(*pphandle);
		*pphandle = NULL;

		return INVALID_INDEX;
	}

	return index;
}

int
DeleteDisplay (void *phandle, int exit)
{
	DispHandle *hdisp = (DispHandle *)phandle;

	if (hdisp == NULL)
		return FALSE;

	if (CloseDisplay(hdisp) == FALSE)
		return FALSE;

	free(hdisp);

	return TRUE;
}

static int
EnsureOverlayOff (int video_fd, int * streamon)
{
	if (*streamon == TRUE)
	{
		if (v4l2_overlay_off (video_fd) < 0)
		{
			ErrorF ("V4L2: v4l2_streamoff : %s\n", strerror(errno));
			return FALSE;
		}
		*streamon = FALSE;
	}
	return TRUE;
}

static int
CheckFormatChanged (void *phandle, int w, int h, CRECT *dest, CRECT *crop, V4L2Videoformat format, int rotate, int hflip, int vflip)
{
	DispHandle *hdisp = (DispHandle *)phandle;

	if (hdisp == NULL)
		return FALSE;

	if (!hdisp->status.init)
		return FALSE;

	if (hdisp->status.w != w || hdisp->status.h != h || hdisp->status.format != format)
		return TRUE;

	if (hdisp->status.rotate != rotate)
		return TRUE;

	if (hdisp->status.hflip != hflip || hdisp->status.vflip != vflip)
		return TRUE;

	if (crop && memcmp(&hdisp->status.crop, crop, sizeof(CRECT)))
		return TRUE;

	if (dest && memcmp(&hdisp->status.dest, dest, sizeof(CRECT)))
		return TRUE;

	return FALSE;
}

static void
ConvertToV4L2PixmapFormat (DispHandle *hdisp,
                           V4L2Videoformat format,
                           int *v4l2_format,
                           int *ismmap)
{
	if (!v4l2_format || !ismmap)
		return;

	*ismmap = 1;

	switch(format)
	{
	case V4L2_VIDEO_FMT_RGB565:
		*v4l2_format = V4L2_PIX_FMT_RGB565;
		break;

	case V4L2_VIDEO_FMT_RGB24:
		*v4l2_format = V4L2_PIX_FMT_RGB24;
		break;

	case V4L2_VIDEO_FMT_RGB32:
		*v4l2_format = V4L2_PIX_FMT_RGB32;
		break;

	case V4L2_VIDEO_FMT_UYVY:
		*v4l2_format = V4L2_PIX_FMT_UYVY;
		break;

	case V4L2_VIDEO_FMT_YUY2:
	case V4L2_VIDEO_FMT_YUYV:
		*v4l2_format = V4L2_PIX_FMT_YUYV;
		break;

	case V4L2_VIDEO_FMT_SUY2:
		*ismmap = 0;
		*v4l2_format = V4L2_PIX_FMT_YUYV;
		break;

	case V4L2_VIDEO_FMT_YV12:
		*v4l2_format = V4L2_PIX_FMT_YVU420;
		break;

	case V4L2_VIDEO_FMT_I420:
	case V4L2_VIDEO_FMT_YU12:
		*v4l2_format = V4L2_PIX_FMT_YUV420;
		break;

	case V4L2_VIDEO_FMT_S420:
		*ismmap = 0;
		*v4l2_format = V4L2_PIX_FMT_YUV420;
		break;

	case V4L2_VIDEO_FMT_NV12:
		*ismmap = 0;
		*v4l2_format = V4L2_PIX_FMT_NV12;
		break;

	default:
		*v4l2_format = V4L2_PIX_FMT_UYVY;
		ErrorF ("V4L2: Unsupported pixel format, So using UVYU format\n");
		break;
	}
}

int
SetDisplayFormat (void *phandle,
                  int w, int h,
                  CRECT *dest, CRECT *crop, CRECT *win,
                  V4L2Videoformat format,
                  int rotate,
                  int hflip, int vflip,
                  int requestbuffer,
                  int *imagesize)
{
	DispHandle *hdisp = (DispHandle *)phandle;

	if (CheckFormatChanged (hdisp, w, h, dest, crop, format, rotate, hflip, vflip))
	{
		CloseDisplay (hdisp);
		OpenDisplay (hdisp, hdisp->index, requestbuffer);
	}

	if (hdisp->status.w != w || hdisp->status.h != h || hdisp->status.format != format)
	{
		struct v4l2_rect src_rect = { 0, 0, w, h };
		struct v4l2_rect crop_rect = { crop->x, crop->y, crop->w, crop->h };
		int v4l2_format;

		EnsureOverlayOff (hdisp->video_fd, &hdisp->status.streamon);

		ConvertToV4L2PixmapFormat (hdisp, format, &v4l2_format, &hdisp->ismmap);

		if (hdisp->ismmap)
			crop_rect = src_rect;
		else
		{
			ErrorF ("V4L2: Emulfb doesn't support userptr. \n");
			return FALSE;
		}

		if (v4l2_set_src (hdisp->video_fd, &src_rect, &crop_rect, v4l2_format) < 0)
		{
			ErrorF ("V4L2: v4l2_set_videoformat : %s | W(%d) H(%d) Format(%d)\n", strerror(errno), w, h, format);
			return FALSE;
		}
		hdisp->status.w = w;
		hdisp->status.h = h;
		hdisp->status.format = format;
		hdisp->status.crop = *crop;

		hdisp->status.requestbuffer = 0;
		hdisp->status.init = 1;
	}

	if (hdisp->status.rotate != rotate ||
	    (dest && memcmp(&hdisp->status.dest, dest, sizeof(CRECT))) ||
	    (hdisp->status.hflip != hflip || hdisp->status.vflip != vflip))
	{
		struct v4l2_rect dst = { dest->x, dest->y, dest->w, dest->h };
		struct v4l2_rect winr = { win->x, win->y, win->w, win->h };

		EnsureOverlayOff (hdisp->video_fd, &hdisp->status.streamon);

		hdisp->status.cur_idx = 0;

		if (v4l2_set_dst (hdisp->video_fd, &dst, &winr, rotate, 0, 0 ) < 0)
		{
			ErrorF ("V4L2: v4l2_set_dst : %s \n", strerror(errno));
			return FALSE;
		}

		hdisp->status.rotate = rotate;
		hdisp->status.hflip = hflip;
		hdisp->status.vflip = vflip;
		hdisp->status.dest = *dest;
	}

	if ( hdisp->status.requestbuffer != requestbuffer )
	{
		EnsureOverlayOff (hdisp->video_fd, &hdisp->status.streamon);
		hdisp->status.cur_idx = 0;

		if ( v4l2_set_buf(hdisp->video_fd, requestbuffer, &hdisp->dst_buf, hdisp->ismmap) < 0 )
		{
			ErrorF ("V4L2: v4l2_set_buf : %s \n", strerror(errno));
			return FALSE;
		}
		hdisp->status.requestbuffer = requestbuffer;
	}

	if (hdisp->status.streamon == FALSE)
	{
		v4l2_overlay_on(hdisp->video_fd);
		hdisp->status.streamon = TRUE;
	}

	return TRUE;
}

int
DrawDisplay (void *phandle,
             unsigned char *buf,
             xRectangle *img,
             xRectangle *pixmap,
             xRectangle *draw,
             xRectangle *src,
             xRectangle *dst,
             RegionPtr   clip_region)
{
	DispHandle *hdisp = (DispHandle *)phandle;
	pixman_format_code_t src_format, dst_format;
	int i;

	switch (hdisp->status.format)
	{
	case V4L2_VIDEO_FMT_I420:
	case V4L2_VIDEO_FMT_YV12:
		src_format = PIXMAN_yv12;
		break;
	case V4L2_VIDEO_FMT_YUY2:
		src_format = PIXMAN_yuy2;
		break;
	case V4L2_VIDEO_FMT_RGB565:
		src_format = PIXMAN_r5g6b5;
		break;
	case V4L2_VIDEO_FMT_RGB24:
		src_format = PIXMAN_r8g8b8;
		break;
	case V4L2_VIDEO_FMT_RGB32:
		src_format = PIXMAN_a8r8g8b8;
		break;
	default:
		return FALSE;
	}

	switch (hdisp->status.format)
	{
	case V4L2_VIDEO_FMT_I420:
		dst_format = PIXMAN_x8b8g8r8;
		break;
	default:
		dst_format = PIXMAN_x8r8g8b8;
		break;
	}

	/* support only RGB  */
	for(i = 0; i < hdisp->status.requestbuffer; i++)
	fbdev_pixman_convert_image (img->width, img->height,
	                            buf, hdisp->dst_buf[0].buf,
	                            src_format, dst_format,
	                            img, pixmap, draw, src, dst,
	                            clip_region, hdisp->status.rotate,
	                            hdisp->status.hflip, hdisp->status.vflip);

	return TRUE;
}

void * GetCapture(void * phandle, unsigned short * width, unsigned short * height,  int * rotation, V4L2Videoformat * format, int bUseRpScaler)
{
	DispHandle *hdisp = (DispHandle *)phandle;

	*width = hdisp->status.w;
	*height = hdisp->status.h;
	*rotation = hdisp->status.rotate;
	*format = hdisp->status.format;

	return hdisp->status.last_buffer;
}
