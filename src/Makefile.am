AM_CFLAGS = -I@top_srcdir@/src @XORG_CFLAGS@ @EMULFB_CFLAGS@
emulfb_drv_la_LTLIBRARIES = emulfb_drv.la
#emulfb_drv_la_LDFLAGS = -module -avoid-version @EMULFB_LIBS@
emulfb_drv_la_LDFLAGS = -module -avoid-version
emulfb_drv_la_LIBADD = @EMULFB_LIBS@
emulfb_drv_ladir = @moduledir@/drivers

# common
emulfb_drv_la_SOURCES = \
	fbdev.c \
	fbdev_dpms.c

# v4l2
emulfb_drv_la_SOURCES += \
	v4l2/v4l2api.c \
	v4l2/v4l2_output_api.c
AM_CFLAGS += -I@top_srcdir@/src/v4l2

# xv
emulfb_drv_la_SOURCES += \
	xv/fbdev_video.c
AM_CFLAGS += -I@top_srcdir@/src/xv

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

