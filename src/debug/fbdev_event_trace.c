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

#include "fbdev.h"
#include "xace.h"
#include "xacestr.h"

#if 0
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/extensions/XI2proto.h>

#include "windowstr.h"

#define XREGISTRY
#include "registry.h"

#define __USE_GNU
#include <sys/socket.h>
#include <linux/socket.h>

#ifdef HAS_GETPEERUCRED
# include <ucred.h>
#endif
#endif

static void
_traceReceive (CallbackListPtr *pcbl, pointer unused, pointer calldata)
{
    XaceReceiveAccessRec *rec = calldata;

    if (rec->events->u.u.type != VisibilityNotify)
	return;

    rec->status = BadAccess;
}

Bool
fbdevTraceInstallHooks (void)
{
    int ret = TRUE;

    /*Disable Visibility Event*/
    ret &= XaceRegisterCallback (XACE_RECEIVE_ACCESS, _traceReceive, NULL);

    if (!ret)
    {
        ErrorF ("fbdevInstallHooks: Failed to register one or more callbacks\n");
        return BadAlloc;
    }

    return Success;
}


Bool
fbdevTraceUninstallHooks (void)
{
    XaceDeleteCallback (XACE_RECEIVE_ACCESS, _traceReceive, NULL);

    return Success;
}

