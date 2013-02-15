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

#include <linux/fb.h>
#include "fbdev_hw.h"
#include "fbdev_mode.h"

typedef struct _FBDevDisplayModeRec
{
	char 		name[32];		/* identifier for the mode */

	/* These are the values that the user sees/provides */
	int				Clock;		/* pixel clock freq (kHz) */
	int				HDisplay;	/* horizontal timing */
	int				HSyncStart;
	int				HSyncEnd;
	int				HTotal;
	int				HSkew;
	int				VDisplay;	/* vertical timing */
	int				VSyncStart;
	int				VSyncEnd;
	int				VTotal;
	int				VScan;
	int				Flags;

} FBDevDisplayModeRec, *FBDevDisplayModePtr;

/* [soolim: 20100206][TODO] : it has to be recalculated when it comes to change the target framebuffer
  *												 The display mode data has to be changed whenever the target is changed
  *												 Always check it out.
  */
const FBDevDisplayModeRec fbdevDisplayModes[] =
{
	/* CGA 		( 320 x 200 ) 	*/
	{"320x200", 38, 320, 336, 338, 354, 0, 200, 203, 205, 233, 0, 0,},
	/* QVGA 	( 320 x 240 ) 	*/
	{"320x240", 38, 320, 336, 338, 354, 0, 240, 243, 245, 273, 0, 0,},
	/* XX 	( 240 x 400 ) 	*/
	{"240x400", 38, 240, 256, 258, 274, 0, 400, 428, 430, 433, 0, 0,},
	/* HVGA 		(320 x 480 )	*/
	{"320x480", 38, 320, 336, 338, 354, 0, 480, 483, 485, 513, 0, 0,},
	/* WVGA 	( 480 x 720 ) 	*/
	{"480x720", 38, 480, 496, 498, 514, 0, 720, 723, 725, 753, 0, 0},
	/* WVGA 	( 480 x 800 ) 	*/
	{"480x800", 38, 480, 496, 498, 514, 0, 800, 803, 805, 833, 0, 0},
	/* VGA 		( 640 x 480 ) 	*/
	{"640x480", 19, 640, 840, 880, 959, 0, 480, 491, 501, 511, 0, 0,},
	/* NTSC 		( 720 x 480 ) 	*/
	{"720x480", 19, 720, 920, 960, 1039, 0, 480, 491, 501, 511, 0, 0,},
	/* PAL 		( 768 x 576 ) 	*/
	{"768x576", 19, 768, 968, 1008, 1087, 0, 576, 587, 597, 607, 0, 0,},
	/* WVGA 	( 800 x 480 ) 	*/
	{"800x480", 19, 800, 1000, 1040, 1119, 0, 480, 491, 501, 511, 0, 0},
	/* WVGA 	( 854 x 480 ) 	*/
	{"854x480", 19, 854, 1054, 1094, 1173, 0, 480, 491, 501, 511, 0, 0,},
	/* WSVGA 	( 600 x 1024 ) */
	{"600x1024", 19, 600, 611, 621, 631, 0, 1024, 1224, 1264, 1343, 0, 0},
	/* WSVGA 	( 1024 x 600 ) */
	{"1024x600", 19, 1024, 1224, 1264, 1343, 0, 600, 611, 621, 631, 0, 0},
#if 0
	/* XGA 		( 1024 x 768 ) */
	{"1024x768", 19, 1024, 1224, 1264, 1343, 600, 611, 621, 631, 0,},
#endif
};


#define NUM_DISPLAY_MODES (sizeof(fbdevDisplayModes)/sizeof(fbdevDisplayModes[0]))

const int fbdevNumDisplayModes = NUM_DISPLAY_MODES;


DisplayModePtr
FBDevGetSupportModes(DisplayModePtr builtin_mode)
{
	DisplayModePtr pMode = NULL;
	DisplayModePtr prev_pMode = NULL;
	DisplayModePtr ret_pMode = NULL;
	int i;
	int clock = 0;
	if(builtin_mode)
		clock = builtin_mode->Clock;
	else
		clock = 0;

	for(i=0; i< fbdevNumDisplayModes; i++)
	{
		pMode = calloc(1, sizeof(DisplayModeRec));
		pMode->next = NULL;
		pMode->prev = NULL;
		pMode->name = calloc(1, sizeof(char)*32);
		sprintf(pMode->name,"%s", fbdevDisplayModes[i].name);
		pMode->status = MODE_OK;
		pMode->type = M_T_DRIVER | M_T_PREFERRED;

		pMode->Clock = clock;
		pMode->HDisplay = fbdevDisplayModes[i].HDisplay;
		pMode->HSyncStart = fbdevDisplayModes[i].HSyncStart;
		pMode->HSyncEnd = fbdevDisplayModes[i].HSyncEnd;
		pMode->HTotal = fbdevDisplayModes[i].HTotal;
		pMode->HSkew = fbdevDisplayModes[i].HSkew;
		pMode->VDisplay = fbdevDisplayModes[i].VDisplay;
		pMode->VSyncStart = fbdevDisplayModes[i].VSyncStart;
		pMode->VSyncEnd = fbdevDisplayModes[i].VSyncEnd;
		pMode->VTotal = fbdevDisplayModes[i].VTotal;
		pMode->VScan = fbdevDisplayModes[i].VScan;
		pMode->Flags = fbdevDisplayModes[i].Flags;

		if(prev_pMode)
		{
			pMode->prev = prev_pMode;
			prev_pMode->next = pMode;
			prev_pMode = prev_pMode->next;
		}
		else
		{
			prev_pMode = pMode;
			ret_pMode = pMode;
		}

		pMode = NULL;
	}

	return ret_pMode;
}

#define OPTION_PREFERRED_MODE 0

char *
FBDevCheckPreferredMode(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
	char *preferred_mode = NULL;

	/* Check for a configured preference for a particular mode */
	preferred_mode = xf86GetOptValString (output->options,
	                                      OPTION_PREFERRED_MODE);
	if (preferred_mode)
		return preferred_mode;

	if (pScrn->display->modes && *pScrn->display->modes)
		preferred_mode = *pScrn->display->modes;

	return preferred_mode;
}
