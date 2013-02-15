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

#ifndef __FBDEV_V4L2_TYPES_H__
#define __FBDEV_V4L2_TYPES_H__

#include <sys/types.h>
#include <X11/Xdefs.h>

#include <linux/videodev2.h>

enum
{
	OUTPUT_PATH_DMA,	/* FrameBuffer */
	OUTPUT_PATH_FIMD,	/* LCD */
};

/************************************************************
 * TYPE DEFINITION
 ************************************************************/
typedef enum v4l2_buf_type FBDevV4l2BufType;
typedef enum v4l2_memory   FBDevV4l2Memory;

#ifndef uint
typedef unsigned int uint;
#endif

#ifndef uchar
typedef unsigned char uchar;
#endif

#ifndef ushort
typedef unsigned short ushort;
#endif

/************************************************************
 * STRUCTURE
 ************************************************************/
typedef struct _FBDevV4l2Data
{
	__u32	path;
	__u32	in_format;
	__u32	out_format;
	__u32	field;
	char	in_file_name[50];

	struct v4l2_rect src;
	struct v4l2_rect crop;
	struct v4l2_rect dst;
	struct v4l2_rect win;

	__u32	s_size[3];
	__u32	src_size;
	__u32	d_size[3];
	__u32	dst_size;

	int	    rotation;
} FBDevV4l2Data;

typedef struct _FBDevV4l2FimcBuffer
{
	uint    base[3];
	size_t  length[3];
} FBDevV4l2FimcBuffer;

typedef struct _FBDevV4l2SrcBuffer
{
	int     index;
	int     size;
	uchar  *buf;
} FBDevV4l2SrcBuffer;

typedef struct _FBDevV4l2DstBuffer
{
	int     index;
	int     size;
	uchar  *buf;
} FBDevV4l2DstBuffer;

#endif
