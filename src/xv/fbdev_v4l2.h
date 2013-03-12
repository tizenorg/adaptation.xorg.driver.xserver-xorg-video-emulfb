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

#ifndef __FBDEV_V4L2_H__
#define __FBDEV_V4L2_H__

#include "fbdev_video_types.h"


#define V4L2_PIX_FMT_NV12T      v4l2_fourcc('T', 'V', '1', '2') /* 12  Y/CbCr 4:2:0 64x32 macroblocks */
#define V4L2_CID_ROTATION       (V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_PADDR_Y        (V4L2_CID_PRIVATE_BASE + 1)
#define V4L2_CID_PADDR_CB       (V4L2_CID_PRIVATE_BASE + 2)
#define V4L2_CID_PADDR_CR       (V4L2_CID_PRIVATE_BASE + 3)
#define V4L2_CID_PADDR_CBCR     (V4L2_CID_PRIVATE_BASE + 4)
#define V4L2_CID_OVERLAY_AUTO   (V4L2_CID_PRIVATE_BASE + 5)
#define V4L2_CID_OVERLAY_VADDR0 (V4L2_CID_PRIVATE_BASE + 6)
#define V4L2_CID_OVERLAY_VADDR1 (V4L2_CID_PRIVATE_BASE + 7)
#define V4L2_CID_OVERLAY_VADDR2 (V4L2_CID_PRIVATE_BASE + 8)
#define V4L2_CID_OVLY_MODE      (V4L2_CID_PRIVATE_BASE + 9)


Bool    fbdev_v4l2_querycap       (int fd, int capabilities);
Bool    fbdev_v4l2_cropcap        (int fd, FBDevV4l2BufType type, struct v4l2_rect *crop);

Bool    fbdev_v4l2_enum_std       (int fd, struct v4l2_standard *std, v4l2_std_id std_id);
Bool    fbdev_v4l2_enum_output    (int fd, struct v4l2_output *output, FBDevV4l2BufType type);
Bool    fbdev_v4l2_enum_fmt       (int fd, struct v4l2_fmtdesc *desc, FBDevV4l2BufType type);

Bool    fbdev_v4l2_g_std          (int fd, v4l2_std_id *std_id);
Bool    fbdev_v4l2_s_std          (int fd, v4l2_std_id std_id);
Bool    fbdev_v4l2_g_output       (int fd, int *index);
Bool    fbdev_v4l2_s_output       (int fd, int index);
Bool    fbdev_v4l2_try_fmt        (int fd, struct v4l2_format *format);
Bool    fbdev_v4l2_g_fmt          (int fd, struct v4l2_format *format);
Bool    fbdev_v4l2_s_fmt          (int fd, struct v4l2_format *format);
Bool    fbdev_v4l2_g_parm         (int fd, struct v4l2_streamparm *parm);
Bool    fbdev_v4l2_s_parm         (int fd, struct v4l2_streamparm *parm);
Bool    fbdev_v4l2_g_fbuf         (int fd, struct v4l2_framebuffer *frame);
Bool    fbdev_v4l2_s_fbuf         (int fd, struct v4l2_framebuffer *frame);
Bool    fbdev_v4l2_g_crop         (int fd, struct v4l2_crop *crop);
Bool    fbdev_v4l2_s_crop         (int fd, struct v4l2_crop *crop);
Bool    fbdev_v4l2_g_ctrl         (int fd, struct v4l2_control *ctrl);
Bool    fbdev_v4l2_s_ctrl         (int fd, struct v4l2_control *ctrl);

Bool    fbdev_v4l2_reqbuf         (int fd, struct v4l2_requestbuffers *req);
Bool    fbdev_v4l2_querybuf       (int fd, struct v4l2_buffer *set_buf);
Bool    fbdev_v4l2_queue          (int fd, struct v4l2_buffer *buf);
Bool    fbdev_v4l2_dequeue        (int fd, struct v4l2_buffer *buf);

Bool    fbdev_v4l2_streamon       (int fd, FBDevV4l2BufType type);
Bool    fbdev_v4l2_streamoff      (int fd, FBDevV4l2BufType type);
Bool    fbdev_v4l2_start_overlay  (int fd);
Bool    fbdev_v4l2_stop_overlay   (int fd);


#endif
