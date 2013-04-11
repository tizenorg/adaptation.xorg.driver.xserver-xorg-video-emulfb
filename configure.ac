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
#
# Process this file with autoconf to produce a configure script

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

AC_ARG_ENABLE(arm,
	      AS_HELP_STRING([--enable-arm],
                             [Enable arm (default: enabled)]),
			     [ARM=$enableval], [ARM=no])

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
PKG_CHECK_MODULES(XDBG, xdbg)

EMULFB_CFLAGS="$EMULFB_CFLAGS $DLOG_CFLAGS $PIXMAN_CFLAGS $DRM_CFLAGS $XDBG_CFLAGS "
EMULFB_LIBS="$EMULFB_LIBS $DLOG_LIBS $PIXMAN_LIBS $DRM_LIBS $XDBG_LIBS "


AM_CONDITIONAL(ENABLE_ARM, [test "x$ARM" = xyes])
if test "x$ARM" = xyes; then
    AC_DEFINE(ENABLE_ARM, 1, [Enable arm])
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