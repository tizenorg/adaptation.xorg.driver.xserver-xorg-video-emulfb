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
#include <linux/videodev2.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include "v4l2config.h"
#include "v4l2_output_api.h"

#ifdef __ASSEMBLY__
#define _AC(X,Y)    X
#define _AT(T,X)    X
#else
#define __AC(X,Y)   (X##Y)
#define _AC(X,Y)    __AC(X,Y)
#define _AT(T,X)    ((T)(X))
#endif

#define PAGE_SHIFT       12
#define PAGE_SIZE        (_AC(1,UL) << PAGE_SHIFT)

int v4l2_close_fd(int fd)
{
	int     ret_val;
	if (-1 != fd)
	{
		ret_val = close(fd);

		if(ret_val < 0)
		{
			fprintf(stderr,"Cannot close (%d)'%d' : (%s)\n", fd, ret_val, strerror(errno));
			return -1;
		}
	}
	return 0;
}

int v4l2_set_src(int fd, struct v4l2_rect *src, struct v4l2_rect *crop_rect, unsigned int pixel_format)
{
	return 0;
}

int v4l2_set_buf(int fd, int num_buf, struct v4l2_dst_buffer **mem_buf, int is_mmap)
{
	struct v4l2_requestbuffers	req;
	struct v4l2_dst_buffer		*out_buf;
	int				idx;

	if(fd<0)
	{
		fprintf(stderr,"device not opened : %d\n",fd);
		return -1;
	}

	CLEAR (req);

	req.count       = num_buf;
	req.type        = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	if (is_mmap == 0)
		req.memory      = V4L2_MEMORY_USERPTR;
	else
		req.memory      = V4L2_MEMORY_MMAP;
	if (ioctl (fd, VIDIOC_REQBUFS, &req) < 0)
	{
		if (EINVAL == errno)
		{
			fprintf(stderr,"Not Support memory mapping\n");
			return -1;
		}
		else
		{
			fprintf(stderr,"Error in VIDIOC_REQBUFS (%s)\n",strerror(errno));
			return -1;
		}
	}

	if (is_mmap)
	{
		out_buf = calloc(req.count, sizeof(struct v4l2_dst_buffer));
		if(out_buf == NULL)
		{
			fprintf(stderr,"calloc fail (num:%d, size:%d)\n",
			        req.count,
			        sizeof(struct v4l2_dst_buffer));
			return -1;
		}

		for(idx = 0; idx < num_buf; idx++)
		{
			struct v4l2_buffer set_buf;

			CLEAR(set_buf);
			set_buf.type        = V4L2_BUF_TYPE_VIDEO_OVERLAY;
			set_buf.memory      = V4L2_MEMORY_MMAP;
			set_buf.index       = idx;

			if(-1 == ioctl(fd, VIDIOC_QUERYBUF, &set_buf))
			{
				fprintf(stderr,"Error in VIDIOC_QUERYBUF (%s)\n",
				        strerror(errno));
				if (out_buf)
					free(out_buf);
				return -1;
			}

			out_buf[idx].index  = idx;
			out_buf[idx].size   = set_buf.length;
		    out_buf[idx].buf    = mmap (NULL /* start anywhere */,
			                            set_buf.length,
			                            PROT_READ | PROT_WRITE /* required */,
			                            MAP_SHARED /* recommended */,
			                            fd, set_buf.m.offset & ~(PAGE_SIZE - 1 ));

		    out_buf[idx].buf += set_buf.m.offset & (PAGE_SIZE - 1);
			if(out_buf[idx].buf == MAP_FAILED)
			{
				fprintf(stderr,"mmap fail (%d)\n",idx);
				free(out_buf);
				return -1;
			}
			else
			{
				fprintf(stderr,"mmaped addr (%p)\n", out_buf[idx].buf);
			}
		}

		*mem_buf = out_buf;
	}
	return 0;
}

int v4l2_clr_buf(int fd, int num_buf, struct v4l2_dst_buffer *mem_buf, enum v4l2_buf_type type, int is_mmap)
{
	unsigned int i;
	struct v4l2_requestbuffers req;

	if(fd < 0)
	{
		fprintf(stderr,"device not opened : %d\n",fd);
		return -1;
	}

	if (is_mmap)
	{
		for (i = 0; i < num_buf; ++i)
		{
			if (mem_buf[i].buf)
			{
				if (-1 == munmap (mem_buf[i].buf,
				                  mem_buf[i].size))
					printf ("Failed to unmap v4l2 buffer \
						at index %d\n", i);
			}
		}
		free(mem_buf);
	}

	CLEAR (req);
	req.count   = 0;
	req.type    = type;
	if (is_mmap == 0)
		req.memory      = V4L2_MEMORY_USERPTR;
	else
		req.memory      = V4L2_MEMORY_MMAP;
	if (ioctl (fd, VIDIOC_REQBUFS, &req) == -1)
	{
		if (EINVAL == errno)
			fprintf(stderr,"%d does not support memory mapping\n", fd);
		else
			fprintf(stderr,"Error in VIDIOC_REQBUFS (%s)\n\n", strerror (errno));
	}

	return 0;
}

int v4l2_set_dst(int fd, struct v4l2_rect *dst, struct v4l2_rect *win, int rotation, int xres, int yres)
{
	struct v4l2_capability	cap;
	int	   ret_val;
	struct v4l2_format		fmt;

	if(fd < 0)
	{
		fprintf(stderr, "device not opened : %d\n",fd);
		return -1;
	}

	CLEAR(cap);
	if (-1 == ioctl (fd, VIDIOC_QUERYCAP, &cap))
	{
		fprintf(stderr,"Failed to query capabilities of device %d (%s)\n", fd, strerror (errno));
		return -1;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY))
	{
		fprintf(stderr,"%d is no video overlay\n", fd);
		return -1;
	}

	/* Video Overlay setting ============================================================== */
	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.win.w.left = win->left;
	fmt.fmt.win.w.top = win->top;
	fmt.fmt.win.w.width = win->width;
	fmt.fmt.win.w.height = win->height;

	ret_val = ioctl(fd, VIDIOC_S_FMT, &fmt);
	if (ret_val < 0)
	{
		fprintf(stderr, "Error in video VIDIOC_S_FMT (%s)\n",strerror(errno));
		return -1;
	}

	return 0;
}

int v4l2_stream_on(int fd, enum v4l2_buf_type type)
{
	if (-1 == ioctl (fd, VIDIOC_STREAMON, &type))
	{
		fprintf(stderr, "Error in VIDIOC_STREAMON (%s)\n", strerror(errno));
		return -1;
	}

	return 0;
}


int v4l2_stream_off(int fd, enum v4l2_buf_type type)
{
	if (-1 == ioctl (fd, VIDIOC_STREAMOFF, &type))
	{
		fprintf(stderr, "Error in VIDIOC_STREAMOFF (%s)\n", strerror(errno));
		return -1;
	}

	return 0;
}

int v4l2_overlay_on(int fd)
{
    int power = 1;

    if (-1 == ioctl (fd, VIDIOC_OVERLAY, &power))
    {
		fprintf(stderr, "Error in VIDIOC_OVERLAY : %d (%s)\n", power, strerror(errno));
		return -1;
    }
    return 0;
}


int v4l2_overlay_off(int fd)
{
    int power = 0;

    if (-1 == ioctl (fd, VIDIOC_OVERLAY, &power))
    {
		fprintf(stderr, "Error in VIDIOC_OVERLAY : %d (%s)\n", power, strerror(errno));
		return -1;
    }
    return 0;
}

int v4l2_dequeue(int fd, enum v4l2_buf_type type, int is_mmap)
{
	struct v4l2_buffer          buf;
	int retry = 0;

	CLEAR (buf);
	buf.type        = type;
	if (is_mmap == 0)
		buf.memory      = V4L2_MEMORY_USERPTR;
	else
		buf.memory      = V4L2_MEMORY_MMAP;
again:
	if (-1 == ioctl (fd, VIDIOC_DQBUF, &buf))
	{
		if (errno == EINTR)
		{
			if (retry < 100)
			{
				retry++;
				goto again;
			}
		}
		fprintf(stderr, "Error in VIDIOC_DQBUF : %s \n", strerror(errno));
		return -1;
	}

	return buf.index;
}

int v4l2_queue(int fd, enum v4l2_buf_type type, int index, struct v4l2_src_buffer *src_buf, int is_mmap)
{
	struct v4l2_buffer	buf;

	CLEAR (buf);
	buf.type        = type;
	if (is_mmap == 0)
	{
		buf.memory      = V4L2_MEMORY_USERPTR;
		buf.m.userptr	= (unsigned long)src_buf;
		buf.length	= 0;
	}
	else
		buf.memory      = V4L2_MEMORY_MMAP;
	buf.index       = index;

	if (-1 == ioctl (fd, VIDIOC_QBUF, &buf))
	{
		fprintf(stderr, "Error in VIDIOC_QBUF : %s \n", strerror(errno));
		return -1;
	}

	return 0;
}
