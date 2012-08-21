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

