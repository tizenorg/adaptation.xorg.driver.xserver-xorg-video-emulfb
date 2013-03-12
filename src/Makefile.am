#  Copyright 2005 Adam Jackson.
#  Copyright (C) 2010 - 2011 Samsung Electronics co., Ltd. All Rights Reserved.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# this is obnoxious:
# -module lets us name the module exactly how we want
# -avoid-version prevents gratuitous .0.0.0 version numbers on the end
# _ladir passes a dummy rpath to libtool so the thing will actually link
# TODO: -nostdlib/-Bstatic/-lgcc platform magic, not installing the .a, etc.

AM_CFLAGS = -I@top_srcdir@/src @XORG_CFLAGS@ @EMULFB_CFLAGS@
emulfb_drv_la_LTLIBRARIES = emulfb_drv.la
#emulfb_drv_la_LDFLAGS = -module -avoid-version @EMULFB_LIBS@
emulfb_drv_la_LDFLAGS = -module -avoid-version
emulfb_drv_la_LIBADD = @EMULFB_LIBS@
emulfb_drv_ladir = @moduledir@/drivers

# common
emulfb_drv_la_SOURCES = \
	fbdev.c \
	fb/fbdev_fb.c \
	fbdev_dpms.c
AM_CFLAGS += -I@top_srcdir@/src/fb

if ENABLE_ARM
    xvtarget = arm
else
    xvtarget = i386
endif

# xv
emulfb_drv_la_SOURCES += \
	xv/fbdev_video.c \
	xv/fbdev_video_virtual.c \
	xv/fbdev_v4l2.c \
	xv/${xvtarget}/fbdev_video_v4l2.c
AM_CFLAGS += -I@top_srcdir@/src/xv
AM_CFLAGS += -I@top_srcdir@/src/xv/${xvtarget}

# fbdevhw
emulfb_drv_la_SOURCES += \
	fbdevhw/fbdev_hw.c
AM_CFLAGS += -I@top_srcdir@/src/fbdevhw

# util
emulfb_drv_la_SOURCES += \
	util/fbdev_util.c \
	util/fbdev_pixman.c
AM_CFLAGS += -I@top_srcdir@/src/util

# crtcconfig
emulfb_drv_la_SOURCES += \
	crtcconfig/fbdev_crtcconfig.c \
	crtcconfig/fbdev_crtc.c \
	crtcconfig/lcd_output.c \
	crtcconfig/fbdev_mode.c
AM_CFLAGS += -I@top_srcdir@/src/crtcconfig

# crtcconfig
emulfb_drv_la_SOURCES += \
	debug/fbdev_event_trace.c
AM_CFLAGS += -I@top_srcdir@/src/debug

