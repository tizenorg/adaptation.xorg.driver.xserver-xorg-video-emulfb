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

#include <linux/videodev2.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIDEO_OUT_BUF_NUM	3

#define CLEAR(x) memset (&(x), 0, sizeof (x))

struct v4l2_src_buffer {
	unsigned int  base[3];
	size_t	      length[3];
};

struct v4l2_dst_buffer{
    int                 index;
    int                 size;
    unsigned char       *buf;
};

/** --function define for display -------------------------**/
int v4l2_close_fd(int fd);
int v4l2_set_src(int fd, struct v4l2_rect *src, struct v4l2_rect *crop, unsigned int pixel_format);
int v4l2_set_buf(int fd, int num_buf, struct v4l2_dst_buffer **mem_buf, int is_mmap);
int v4l2_clr_buf(int fd, int num_buf, struct v4l2_dst_buffer *mem_buf, enum v4l2_buf_type type, int is_mmap);

int v4l2_set_dst(int fd, struct v4l2_rect *dst, struct v4l2_rect *win, int rotation, int xres, int yres);
int v4l2_stream_off(int fd, enum v4l2_buf_type type);
int v4l2_stream_on(int fd, enum v4l2_buf_type type);
int v4l2_queue(int fd, enum v4l2_buf_type type, int index, struct v4l2_src_buffer *src_buf, int is_mmap);
int v4l2_dequeue(int fd, enum v4l2_buf_type type, int is_mmap);
int v4l2_overlay_on(int fd);
int v4l2_overlay_off(int fd);


#ifdef __cplusplus
}
#endif
