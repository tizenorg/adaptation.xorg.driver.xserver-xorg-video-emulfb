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
#include <sys/ioctl.h>
#include <errno.h>

#include "xf86.h"

#include "fbdev_v4l2.h"
#include "fbdev_util.h"

typedef struct _CapInfo
{
	int   value;
	char *name;
} CapInfo;

static CapInfo cap_infos [] =
{
	{V4L2_CAP_VIDEO_CAPTURE,        "VIDEO_CAPTURE"},
	{V4L2_CAP_VIDEO_OUTPUT,         "VIDEO_OUTPUT"},
	{V4L2_CAP_VIDEO_OVERLAY,        "VIDEO_OVERLAY"},
	{V4L2_CAP_VBI_CAPTURE,          "VBI_CAPTURE"},
	{V4L2_CAP_VBI_OUTPUT,           "VBI_OUTPUT"},
	{V4L2_CAP_SLICED_VBI_CAPTURE,   "SLICED_VBI_CAPTURE"},
	{V4L2_CAP_SLICED_VBI_OUTPUT,    "SLICED_VBI_OUTPUT"},
	{V4L2_CAP_RDS_CAPTURE,          "RDS_CAPTURE"},
	{V4L2_CAP_VIDEO_OUTPUT_OVERLAY, "VIDEO_OUTPUT_OVERLAY"},
	{V4L2_CAP_HW_FREQ_SEEK,         "HW_FREQ_SEEK"},
	{V4L2_CAP_RDS_OUTPUT,           "RDS_OUTPUT"},
	{V4L2_CAP_TUNER,                "TUNER"},
	{V4L2_CAP_AUDIO,                "AUDIO"},
	{V4L2_CAP_RADIO,                "RADIO"},
	{V4L2_CAP_MODULATOR,            "MODULATOR"},
	{V4L2_CAP_READWRITE,            "READWRITE"},
	{V4L2_CAP_ASYNCIO,              "ASYNCIO"},
	{V4L2_CAP_STREAMING,            "STREAMING"}
};

static Bool _fbdev_v4l2_ioctl (int fd, int cmd, void *data, char *debug)
{
	int retry = 0;
	int ret;

try_again:
	ret = ioctl (fd, cmd, data);
	if (ret < 0)
	{
		if (errno == EINTR || errno == EAGAIN)
		{
			if (retry < 100)
			{
				retry++;
				goto try_again;
			}
		}

		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_querycap (int fd, int capabilities)
{
	struct v4l2_capability cap;
	int ret;

	CLEAR (cap);
	ret = ioctl (fd, VIDIOC_QUERYCAP, &cap);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[QUERYCAP] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	if (~(cap.capabilities) & capabilities)
	{
		int unsupport = ~(cap.capabilities) & capabilities;
		int i;

		for (i = 0; i < sizeof (cap_infos) / sizeof (CapInfo); i++)
			if (unsupport & cap_infos[i].value)
				xf86DrvMsg (0, X_ERROR, "[QUERYCAP] %s not support.\n", cap_infos[i].name);

		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_cropcap (int fd, FBDevV4l2BufType type, struct v4l2_rect *crop)
{
	struct v4l2_cropcap cropcap;
	int ret;

	CLEAR(cropcap);
	cropcap.type = type;
	ret = ioctl (fd, VIDIOC_CROPCAP, &cropcap);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[CROPCAP] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	/* check if crop_rect si valid */
	if ((crop->left < cropcap.bounds.left) &&
	        (crop->top < cropcap.bounds.top) &&
	        (crop->width > cropcap.bounds.width) &&
	        (crop->height > cropcap.bounds.height))
	{
		xf86DrvMsg (0, X_ERROR, "(%d,%d %dx%d) is out of bound(%d,%d %dx%d)\n",
		            crop->left, crop->top, crop->width, crop->height,
		            cropcap.bounds.left, cropcap.bounds.top,
		            cropcap.bounds.width, cropcap.bounds.height);
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_enum_std (int fd, struct v4l2_standard *std, v4l2_std_id std_id)
{
	std->index = 0;

	while (0 == ioctl (fd, VIDIOC_ENUMSTD, std))
	{
		/* return TRUE if std_id found */
		if (std->id & std_id)
		{
			xf86DrvMsg (0, X_ERROR, "[ENUMSTD] name(%s). (%s)\n", std->name, strerror(errno));
			return TRUE;
		}

		std->index++;
	}

	return FALSE;
}

Bool fbdev_v4l2_enum_output (int fd, struct v4l2_output *output, FBDevV4l2BufType type)
{
	output->index = 0;

	while (0 == ioctl (fd, VIDIOC_ENUMOUTPUT, output))
	{
		/* return TRUE if type found */
		if (output->type & type)
		{
			xf86DrvMsg (0, X_ERROR, "[ENUMOUTPUT] index(%d) type(0x%08x) name(%s). (%s)\n",
			            output->index,output->type,output->name, strerror(errno));
			return TRUE;
		}

		output->index++;
	}

	return FALSE;
}

Bool fbdev_v4l2_enum_fmt (int fd, struct v4l2_fmtdesc *desc, FBDevV4l2BufType type)
{
	desc->index = 0;

	while (0 == ioctl (fd, VIDIOC_ENUM_FMT, desc))
	{
		/* return TRUE if type found */
		if (desc->type & type)
		{
			xf86DrvMsg (0, X_ERROR, "[ENUM_FMT] index(%d) type(0x%08x) desc(%s) pxlfmt(0x%08x). (%s)\n",
			            desc->index, desc->type, desc->description, desc->pixelformat, strerror(errno));
			return TRUE;
		}

		desc->index++;
	}

	return FALSE;
}

Bool fbdev_v4l2_g_std (int fd, v4l2_std_id *std_id)
{
	int ret;

	ret = ioctl (fd, VIDIOC_G_STD, std_id);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[G_STD] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_s_std (int fd, v4l2_std_id std_id)
{
	int ret;

	ret = ioctl (fd, VIDIOC_S_STD, &std_id);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[S_STD] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_g_output (int fd, int *index)
{
	int ret;

	ret = ioctl (fd, VIDIOC_G_OUTPUT, index);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[G_OUTPUT] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_s_output (int fd, int index)
{
	int ret;

	ret = ioctl (fd, VIDIOC_S_OUTPUT, &index);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[S_OUTPUT] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_try_fmt (int fd, struct v4l2_format *format)
{
	int ret;

	ret = ioctl (fd, VIDIOC_TRY_FMT, format);
	if (ret < 0)
		return FALSE;

	return TRUE;
}

Bool fbdev_v4l2_g_fmt (int fd, struct v4l2_format *format)
{
	int ret;

	ret = ioctl (fd, VIDIOC_G_FMT, format);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[G_FMT] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_s_fmt (int fd, struct v4l2_format *format)
{
	int ret;

	ret = ioctl (fd, VIDIOC_S_FMT, format);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[S_FMT] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_g_parm (int fd, struct v4l2_streamparm *parm)
{
	int ret;

	ret = ioctl (fd, VIDIOC_G_PARM, parm);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[G_PARM] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_s_parm (int fd, struct v4l2_streamparm *parm)
{
	int ret;

	ret = ioctl (fd, VIDIOC_S_PARM, parm);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[S_PARM] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_g_fbuf (int fd, struct v4l2_framebuffer *frame)
{
	int ret;

	ret = ioctl (fd, VIDIOC_G_FBUF, frame);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[G_FBUF] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_s_fbuf (int fd, struct v4l2_framebuffer *frame)
{
	int ret;

	ret = ioctl (fd, VIDIOC_S_FBUF, frame);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[S_FBUF] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_g_crop (int fd, struct v4l2_crop *crop)
{
	int ret;

	ret = ioctl (fd, VIDIOC_G_CROP, crop);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[G_CROP] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_s_crop (int fd, struct v4l2_crop *crop)
{
	int ret;

	ret = ioctl (fd, VIDIOC_S_CROP, crop);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[S_CROP] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_g_ctrl (int fd, struct v4l2_control *ctrl)
{
	int ret;

	ret = ioctl (fd, VIDIOC_G_CTRL, ctrl);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[G_CTRL] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_s_ctrl (int fd, struct v4l2_control *ctrl)
{
	int ret;

	ret = ioctl (fd, VIDIOC_S_CTRL, ctrl);
	if (ret < 0)
	{
		xf86DrvMsg (0, X_ERROR, "[S_CTRL] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_streamon (int fd, FBDevV4l2BufType type)
{
	if (!_fbdev_v4l2_ioctl (fd, VIDIOC_STREAMON, (void*)&type, "STREAMON"))
	{
		xf86DrvMsg (0, X_ERROR, "[STREAMON] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_streamoff (int fd, FBDevV4l2BufType type)
{
	if (!_fbdev_v4l2_ioctl (fd, VIDIOC_STREAMOFF, (void*)&type, "STREAMOFF"))
	{
		xf86DrvMsg (0, X_ERROR, "[STREAMOFF] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}


Bool fbdev_v4l2_start_overlay (int fd)
{
	int start = 1;

	if (!_fbdev_v4l2_ioctl (fd, VIDIOC_OVERLAY, (void*)&start, "OVERLAY (start)"))
	{
		xf86DrvMsg (0, X_ERROR, "[OVERLAY] (start) failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_stop_overlay (int fd)
{
	int stop = 0;

	if (!_fbdev_v4l2_ioctl (fd, VIDIOC_OVERLAY, (void*)&stop, "OVERLAY (stop)"))
	{
		xf86DrvMsg (0, X_ERROR, "[OVERLAY] (stop) failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_reqbuf (int fd, struct v4l2_requestbuffers *req)
{
	if (!_fbdev_v4l2_ioctl (fd, VIDIOC_REQBUFS, (void*)req, "REQBUFS"))
	{
		xf86DrvMsg (0, X_ERROR, "[REQBUFS] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_querybuf (int fd, struct v4l2_buffer *set_buf)
{
	if (!_fbdev_v4l2_ioctl (fd, VIDIOC_QUERYBUF, (void*)set_buf, "QUERYBUF"))
	{
		xf86DrvMsg (0, X_ERROR, "[QUERYBUF] failed. (%s)\n", strerror(errno));
		return FALSE;
	}

	return TRUE;
}


Bool fbdev_v4l2_queue (int fd, struct v4l2_buffer *buf)
{
	if (!_fbdev_v4l2_ioctl (fd, VIDIOC_QBUF, (void*)buf, "QBUF"))
	{
		xf86DrvMsg (0, X_ERROR, "[QBUF] failed. (%s)\n", strerror(errno));

		return FALSE;
	}

	return TRUE;
}

Bool fbdev_v4l2_dequeue (int fd, struct v4l2_buffer *buf)
{
	if (!_fbdev_v4l2_ioctl (fd, VIDIOC_DQBUF, (void*)buf, "DQBUF"))
	{
		xf86DrvMsg (0, X_ERROR, "[DQBUF] failed. (%s)\n", strerror(errno));

		return FALSE;
	}

	return TRUE;
}
