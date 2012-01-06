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

#include "fbdev_util.h"
#include "fbdev_pixman.h"

int
fbdev_pixman_convert_image (int         xres,
                            int         yres,
                            unsigned char      *srcbuf,
                            unsigned char      *dstbuf,
                            pixman_format_code_t src_format,
                            pixman_format_code_t dst_format,
                            xRectangle *img,
                            xRectangle *pixmap,
                            xRectangle *draw,
                            xRectangle *src,
                            xRectangle *dst,
                            RegionPtr   clip_region,
                            int         rotate,
                            int         hflip,
                            int         vflip)
{
	pixman_image_t    *src_img;
	pixman_image_t    *dst_img;
	struct pixman_f_transform ft;
	pixman_transform_t transform;
	int                src_stride, dst_stride;
	int                src_bpp;
	int                dst_bpp;
	double             scale_x, scale_y;
	pixman_op_t        op;
	int                rotate_step;
	int                ret = FALSE;

	return_val_if_fail (srcbuf != NULL, FALSE);
	return_val_if_fail (dstbuf != NULL, FALSE);
	return_val_if_fail (img != NULL, FALSE);
	return_val_if_fail (pixmap != NULL, FALSE);
	return_val_if_fail (draw != NULL, FALSE);
	return_val_if_fail (src != NULL, FALSE);
	return_val_if_fail (dst != NULL, FALSE);
	return_val_if_fail (rotate <= 360 && rotate >= -360, FALSE);

	op = PIXMAN_OP_SRC;

	src_bpp = PIXMAN_FORMAT_BPP (src_format) / 8;
	return_val_if_fail (src_bpp > 0, FALSE);

	dst_bpp = PIXMAN_FORMAT_BPP (dst_format) / 8;
	return_val_if_fail (dst_bpp > 0, FALSE);

	rotate_step = (rotate + 360) / 90 % 4;

	src_stride = img->width * src_bpp;
	dst_stride = pixmap->width * dst_bpp;

	src_img = pixman_image_create_bits (src_format, img->width, img->height, (uint32_t*)srcbuf, src_stride);
	dst_img = pixman_image_create_bits (dst_format, pixmap->width, pixmap->height, (uint32_t*)dstbuf, dst_stride);

	goto_if_fail (src_img != NULL, CANT_CONVERT);
	goto_if_fail (dst_img != NULL, CANT_CONVERT);

	pixman_f_transform_init_identity (&ft);

	if (hflip)
	{
		pixman_f_transform_scale (&ft, NULL, 1, -1);
		pixman_f_transform_translate (&ft, NULL, 0, (rotate_step%2)?draw->width:draw->height);
	}

	if (vflip)
	{
		pixman_f_transform_scale (&ft, NULL, -1, 1);
		pixman_f_transform_translate (&ft, NULL, (rotate_step%2)?draw->height:draw->width, 0);
	}

	if (rotate_step > 0)
	{
		int c, s, tx = 0, ty = 0;
		switch (rotate_step)
		{
		case 1:
			/* 270 degrees */
			c = 0;
			s = -1;
			ty = draw->width;
			break;
		case 2:
			/* 180 degrees */
			c = -1;
			s = 0;
			tx = draw->width;
			ty = draw->height;
			break;
		case 3:
			/* 90 degrees */
			c = 0;
			s = 1;
			tx = draw->height;
			break;
		default:
			/* 0 degrees */
			c = 0;
			s = 0;
			break;
		}
		pixman_f_transform_rotate (&ft, NULL, c, s);
		pixman_f_transform_translate (&ft, NULL, tx, ty);
	}

	if (rotate_step % 2 == 0)
	{
		scale_x = (double)src->width / dst->width;
		scale_y = (double)src->height / dst->height;
	}
	else
	{
		scale_x = (double)src->width / dst->height;
		scale_y = (double)src->height / dst->width;
	}

	pixman_f_transform_scale (&ft, NULL, scale_x, scale_y);
	pixman_f_transform_translate (&ft, NULL, src->x, src->y);

	pixman_transform_from_pixman_f_transform (&transform, &ft);
	pixman_image_set_transform (src_img, &transform);

	pixman_image_composite (op, src_img, NULL, dst_img,
	                        0, 0, 0, 0, dst->x, dst->y, dst->width, dst->height);

	ret = TRUE;

CANT_CONVERT:
	if (src_img)
		pixman_image_unref (src_img);
	if (dst_img)
		pixman_image_unref (dst_img);

	return ret;
}

