
AC_PREREQ(2.57)
AC_INIT([xserver-xorg-video-emulfb],
        0.1.0,
        [https://],
        xserver-xorg-video-emulfb)

AC_CONFIG_SRCDIR([Makefile.am])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR(.)

AM_INIT_AUTOMAKE([dist-bzip2])

AM_MAINTAINER_MODE

# Checks for programs.
AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC
AM_PROG_AS

AH_TOP([#include "xorg-server.h"])

AC_ARG_WITH(xorg-module-dir,
            AC_HELP_STRING([--with-xorg-module-dir=DIR],
                           [Default xorg module directory [[default=$libdir/xorg/modules]]]),
            		   [moduledir="$withval"],
            		   [moduledir="$libdir/xorg/modules"])

AC_ARG_ENABLE(pciaccess,
	      AS_HELP_STRING([--enable-pciaccess],
                             [Enable use of libpciaccess (default: disabled)]),
			     [PCIACCESS=$enableval], [PCIACCESS=no])

AC_ARG_ENABLE(v4l2,
	      AS_HELP_STRING([--enable-v4l2],
                             [Enable v4l2 (default: enabled)]),
			     [V4L2=$enableval], [V4L2=yes])

# Checks for extensions
XORG_DRIVER_CHECK_EXT(RANDR, randrproto)
XORG_DRIVER_CHECK_EXT(RENDER, renderproto)
XORG_DRIVER_CHECK_EXT(XV, videoproto)

# Checks for pkg-config packages
PKG_CHECK_MODULES(XORG, [xorg-server >= 1.0.99.901 xproto fontsproto $REQUIRED_MODULES])
sdkdir=$(pkg-config --variable=sdkdir xorg-server)

# check the conditions
EMULFB_CFALGS=""
EMULFB_LIBS=""

PKG_CHECK_MODULES(DLOG, dlog)
PKG_CHECK_MODULES(DRM, libdrm)
PKG_CHECK_MODULES(PIXMAN, pixman-1)

EMULFB_CFLAGS="$EMULFB_CFLAGS $DLOG_CFLAGS $PIXMAN_CFLAGS $DRM_CFLAGS "
EMULFB_LIBS="$EMULFB_LIBS $DLOG_LIBS $PIXMAN_LIBS $DRM_LIBS "

AM_CONDITIONAL(V4L2, test "x$V4L2" = xyes)
if test "x$V4L2" = xyes; then
    AC_DEFINE(ENABLE_V4L2, 1, [Enable v4l2])
fi

AM_CONDITIONAL(PCIACCESS, [test "x$PCIACCESS" = xyes])
if test "x$PCIACCESS" = xyes; then
    AC_DEFINE(PCIACCESS, 1, [Use libpciaccess])
    PKG_CHECK_MODULES([PCIACCESS], [pciaccess >= 0.8.0])
    XORG_CFLAGS="$XORG_CFLAGS $PCIACCESS_CFLAGS "
fi

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC

AC_SUBST([EMULFB_CFLAGS])
AC_SUBST([EMULFB_LIBS])

AC_SUBST([XORG_CFLAGS])
AC_SUBST([moduledir])

DRIVER_NAME=emulfb
AC_SUBST([DRIVER_NAME])

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([
	Makefile
	src/Makefile
	man/Makefile
])
