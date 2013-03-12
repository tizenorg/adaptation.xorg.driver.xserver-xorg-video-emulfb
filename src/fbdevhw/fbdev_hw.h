/**************************************************************************

xserver-xorg-video-emulfb

Copyright 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.

Contact: SooChan Lim <sc1.lim@samsung.com>

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

#ifndef FBDEV_HW_H
#define FBDEV_HW_H

#include <linux/fb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

void	FBDevGetVarScreenInfo(int fd, struct fb_var_screeninfo *fbVarInfo);
void	FBDevSetVarScreenInfo(int fd, struct fb_var_screeninfo *fbVarInfo);
void	FBDevGetFixScreenInfo(int fd, struct fb_fix_screeninfo *fbFixInfo);

Bool FBDevScreenAlphaInit(int fd);
Bool FBDevScreenAlphaDeinit(int fd);
Bool FBDevActivateFB(int fd);
Bool FBDevDeActivateFB(int fd);
Bool FBDevSetBaseFrameBuffer(int fd);
void FBDevHWPanDisplay(int fd, int x, int y);

#endif //FBDEV_HW_H

