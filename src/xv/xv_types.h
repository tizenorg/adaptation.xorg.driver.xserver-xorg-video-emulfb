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

/*                                                              */
/* File name : xv_types.h                                       */
/* Author : YoungHoon Jung (yhoon.jung@samsung.com)             */
/* Protocol Version : 1.0.1 (Dec 16th 2009)                       */
/* This file is for describing Xv APIs' buffer encoding method. */
/*                                                              */

#define XV_PUTIMAGE_HEADER	0xDEADCD01
#define XV_PUTIMAGE_VERSION	0x00010001

/* Return Values */
#define XV_OK 0
#define XV_HEADER_ERROR -1
#define XV_VERSION_MISMATCH -2

/* Data structure for XvPutImage / XvShmPutImage */
typedef struct {
	unsigned int _header; /* for internal use only */
	unsigned int _version; /* for internal use only */

	unsigned int YPhyAddr;
	unsigned int CbPhyAddr;
	unsigned int CrPhyAddr;
	unsigned int RotAngle;
	unsigned int VideoMode;
} XV_PUTIMAGE_DATA, * XV_PUTIMAGE_DATA_PTR;

static void
#ifdef __GNUC__
__attribute__ ((unused))
#endif
XV_PUTIMAGE_INIT_DATA(XV_PUTIMAGE_DATA_PTR data)
{
	data->_header = XV_PUTIMAGE_HEADER;
	data->_version = XV_PUTIMAGE_VERSION;
}


static int
#ifdef __GNUC__
__attribute__ ((unused))
#endif
XV_PUTIMAGE_VALIDATE_DATA(XV_PUTIMAGE_DATA_PTR data)
{
	if (data->_header != XV_PUTIMAGE_HEADER)
		return XV_HEADER_ERROR;
	if (data->_version != XV_PUTIMAGE_VERSION)
		return XV_VERSION_MISMATCH;
	return XV_OK;
}
