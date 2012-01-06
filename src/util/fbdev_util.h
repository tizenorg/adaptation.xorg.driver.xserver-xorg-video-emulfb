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
#include "property.h"

#define return_if_fail(cond)          {if (!(cond)) { ErrorF ("%s : '%s' failed.\n", __FUNCTION__, #cond); return; }}
#define return_val_if_fail(cond, val) {if (!(cond)) { ErrorF ("%s : '%s' failed.\n", __FUNCTION__, #cond); return val; }}
#define goto_if_fail(cond, dst)       {if (!(cond)) { ErrorF ("%s : '%s' failed.\n", __FUNCTION__, #cond); goto dst; }}

int fbdev_util_degree_to_rotate(int degree);
int fbdev_util_rotate_to_degree(int rotate);
int fbdev_util_rotate_add(int rot_a, int rot_b);

const PropertyPtr fbdev_util_get_window_property(WindowPtr pWin, const char* prop_name);

