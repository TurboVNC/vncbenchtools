/*
 * translate.c - translate between different pixel formats
 */

/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 *  USA.
 */

#include <stdio.h>
#include <string.h>
#include "rfb.h"

static void PrintPixelFormat(rfbPixelFormat *pf);
static Bool rfbSetClientColourMapBGR233();

Bool rfbEconomicTranslate = FALSE;

/*
 * Structure representing pixel format for RFB server (i.e. us).
 */

extern rfbPixelFormat rfbServerFormat;


/*
 * Some standard pixel formats.
 */

static const rfbPixelFormat BGR233Format = {
    8, 8, 0, 1, 7, 7, 3, 0, 3, 6
};


/*
 * Macro to compare pixel formats.
 */

#define PF_EQ(x,y)							\
	((x.bitsPerPixel == y.bitsPerPixel) &&				\
	 (x.depth == y.depth) &&					\
	 ((x.bigEndian == y.bigEndian) || (x.bitsPerPixel == 8)) &&	\
	 (x.trueColour == y.trueColour) &&				\
	 (!x.trueColour || ((x.redMax == y.redMax) &&			\
			    (x.greenMax == y.greenMax) &&		\
			    (x.blueMax == y.blueMax) &&			\
			    (x.redShift == y.redShift) &&		\
			    (x.greenShift == y.greenShift) &&		\
			    (x.blueShift == y.blueShift))))

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)
#define CONCAT4(a,b,c,d) a##b##c##d
#define CONCAT4E(a,b,c,d) CONCAT4(a,b,c,d)

#define OUT 8
#include "tableinittctemplate.c"
#define IN 8
#include "tabletranstemplate.c"
#undef IN
#define IN 16
#include "tabletranstemplate.c"
#undef IN
#define IN 32
#include "tabletranstemplate.c"
#undef IN
#undef OUT

#define OUT 16
#include "tableinittctemplate.c"
#define IN 8
#include "tabletranstemplate.c"
#undef IN
#define IN 16
#include "tabletranstemplate.c"
#undef IN
#define IN 32
#include "tabletranstemplate.c"
#undef IN
#undef OUT

#define OUT 32
#include "tableinittctemplate.c"
#define IN 8
#include "tabletranstemplate.c"
#undef IN
#define IN 16
#include "tabletranstemplate.c"
#undef IN
#define IN 32
#include "tabletranstemplate.c"
#undef IN
#undef OUT

typedef void (*rfbInitTableFnType)(char **table, rfbPixelFormat *in,
				   rfbPixelFormat *out);

rfbInitTableFnType rfbInitTrueColourSingleTableFns[3] = {
    rfbInitTrueColourSingleTable8,
    rfbInitTrueColourSingleTable16,
    rfbInitTrueColourSingleTable32
};

rfbInitTableFnType rfbInitTrueColourRGBTablesFns[3] = {
    rfbInitTrueColourRGBTables8,
    rfbInitTrueColourRGBTables16,
    rfbInitTrueColourRGBTables32
};

rfbTranslateFnType rfbTranslateWithSingleTableFns[3][3] = {
    { rfbTranslateWithSingleTable8to8,
      rfbTranslateWithSingleTable8to16,
      rfbTranslateWithSingleTable8to32 },
    { rfbTranslateWithSingleTable16to8,
      rfbTranslateWithSingleTable16to16,
      rfbTranslateWithSingleTable16to32 },
    { rfbTranslateWithSingleTable32to8,
      rfbTranslateWithSingleTable32to16,
      rfbTranslateWithSingleTable32to32 }
};

rfbTranslateFnType rfbTranslateWithRGBTablesFns[3][3] = {
    { rfbTranslateWithRGBTables8to8,
      rfbTranslateWithRGBTables8to16,
      rfbTranslateWithRGBTables8to32 },
    { rfbTranslateWithRGBTables16to8,
      rfbTranslateWithRGBTables16to16,
      rfbTranslateWithRGBTables16to32 },
    { rfbTranslateWithRGBTables32to8,
      rfbTranslateWithRGBTables32to16,
      rfbTranslateWithRGBTables32to32 }
};



/*
 * rfbTranslateNone is used when no translation is required.
 */

void
rfbTranslateNone(char *table, rfbPixelFormat *in, rfbPixelFormat *out,
		 char *iptr, char *optr, int bytesBetweenInputLines,
		 int width, int height)
{
    int bytesPerOutputLine = width * (out->bitsPerPixel / 8);

    while (height > 0) {
	memcpy(optr, iptr, bytesPerOutputLine);
	iptr += bytesBetweenInputLines;
	optr += bytesPerOutputLine;
	height--;
    }
}


/*
 * rfbSetTranslateFunction sets the translation function.
 */

Bool
rfbSetTranslateFunction(cl)
    rfbClientPtr cl;
{
    printf("Pixel format for client:\n");
    PrintPixelFormat(&cl->format);

    /*
     * Check that bits per pixel values are valid
     */

    if ((rfbServerFormat.bitsPerPixel != 8) &&
	(rfbServerFormat.bitsPerPixel != 16) &&
	(rfbServerFormat.bitsPerPixel != 32))
    {
	printf("%s: server bits per pixel not 8, 16 or 32\n",
		"rfbSetTranslateFunction");
	return FALSE;
    }

    if ((cl->format.bitsPerPixel != 8) &&
	(cl->format.bitsPerPixel != 16) &&
	(cl->format.bitsPerPixel != 32))
    {
	printf("%s: client bits per pixel not 8, 16 or 32\n",
		"rfbSetTranslateFunction");
	return FALSE;
    }

    if (!rfbServerFormat.trueColour && (rfbServerFormat.bitsPerPixel != 8)) {
	printf("rfbSetTranslateFunction: server has colour map "
		"but %d-bit - can only cope with 8-bit colour maps\n",
		rfbServerFormat.bitsPerPixel);
	return FALSE;
    }

    if (!cl->format.trueColour && (cl->format.bitsPerPixel != 8)) {
	printf("rfbSetTranslateFunction: client has colour map "
		"but %d-bit - can only cope with 8-bit colour maps\n",
		cl->format.bitsPerPixel);
	return FALSE;
    }

    /*
     * bpp is valid, now work out how to translate
     */

    if (!cl->format.trueColour) {

	/* ? -> colour map */

	if (!rfbServerFormat.trueColour) {

	    /* colour map -> colour map */

	    printf("rfbSetTranslateFunction: both 8-bit colour map: "
		    "no translation needed\n");
	    cl->translateFn = rfbTranslateNone;
	    return TRUE;//rfbSetClientColourMap(cl, 0, 0);
	}

	/*
	 * truecolour -> colour map
	 *
	 * Set client's colour map to BGR233, then effectively it's
	 * truecolour as well
	 */

	if (!rfbSetClientColourMapBGR233(cl))
	    return FALSE;

	cl->format = BGR233Format;
    }

    /* ? -> truecolour */

    if (!rfbServerFormat.trueColour) {

	/* colour map -> truecolour */

	printf("rfbSetTranslateFunction: client is %d-bit trueColour,"
		" server has colour map\n",cl->format.bitsPerPixel);

	cl->translateFn = rfbTranslateWithSingleTableFns
			      [rfbServerFormat.bitsPerPixel / 16]
				  [cl->format.bitsPerPixel / 16];

	return TRUE;//rfbSetClientColourMap(cl, 0, 0);
    }

    /* truecolour -> truecolour */

    if (PF_EQ(cl->format,rfbServerFormat)) {

	/* client & server the same */

	printf("  no translation needed\n");
	cl->translateFn = rfbTranslateNone;
	return TRUE;
    }

    if ((rfbServerFormat.bitsPerPixel < 16) ||
	(!rfbEconomicTranslate && (rfbServerFormat.bitsPerPixel == 16))) {

	/* we can use a single lookup table for <= 16 bpp */

	cl->translateFn = rfbTranslateWithSingleTableFns
			      [rfbServerFormat.bitsPerPixel / 16]
				  [cl->format.bitsPerPixel / 16];

	(*rfbInitTrueColourSingleTableFns
	    [cl->format.bitsPerPixel / 16]) (&cl->translateLookupTable,
					     &rfbServerFormat, &cl->format);

    } else {

	/* otherwise we use three separate tables for red, green and blue */

      printf("TranslateFn = rfbTranslateWithRGBTablesFns[%d][%d]\n",
             rfbServerFormat.bitsPerPixel / 16,
             cl->format.bitsPerPixel / 16);

	cl->translateFn = rfbTranslateWithRGBTablesFns
			      [rfbServerFormat.bitsPerPixel / 16]
				  [cl->format.bitsPerPixel / 16];

	(*rfbInitTrueColourRGBTablesFns
	    [cl->format.bitsPerPixel / 16]) (&cl->translateLookupTable,
					     &rfbServerFormat, &cl->format);
    }

    return TRUE;
}


/*
 * rfbSetClientColourMapBGR233 sets the client's colour map so that it's
 * just like an 8-bit BGR233 true colour client.
 */

static Bool
rfbSetClientColourMapBGR233(cl)
    rfbClientPtr cl;
{
    char buf[sz_rfbSetColourMapEntriesMsg + 256 * 3 * 2];
    rfbSetColourMapEntriesMsg *scme = (rfbSetColourMapEntriesMsg *)buf;
    CARD16 *rgb = (CARD16 *)(&buf[sz_rfbSetColourMapEntriesMsg]);
    int i, len;
    int r, g, b;

    if (cl->format.bitsPerPixel != 8) {
	printf("%s: client not 8 bits per pixel\n",
		"rfbSetClientColourMapBGR233");
	return FALSE;
    }

    scme->type = rfbSetColourMapEntries;

    scme->firstColour = Swap16IfLE(0);
    scme->nColours = Swap16IfLE(256);

    len = sz_rfbSetColourMapEntriesMsg;

    i = 0;

    for (b = 0; b < 4; b++) {
	for (g = 0; g < 8; g++) {
	    for (r = 0; r < 8; r++) {
		rgb[i++] = Swap16IfLE(r * 65535 / 7);
		rgb[i++] = Swap16IfLE(g * 65535 / 7);
		rgb[i++] = Swap16IfLE(b * 65535 / 3);
	    }
	}
    }

    len += 256 * 3 * 2;

    return TRUE;
}


static void
PrintPixelFormat(pf)
    rfbPixelFormat *pf;
{
    if (pf->bitsPerPixel == 1) {
	printf("  1 bpp, %s sig bit in each byte is leftmost on the screen.\n",
	       (pf->bigEndian ? "most" : "least"));
    } else {
	printf("  %d bpp, depth %d%s\n",pf->bitsPerPixel,pf->depth,
	       ((pf->bitsPerPixel == 8) ? ""
		: (pf->bigEndian ? ", big endian" : ", little endian")));
	if (pf->trueColour) {
	    printf("  true colour: max r %d g %d b %d, shift r %d g %d b %d\n",
		   pf->redMax, pf->greenMax, pf->blueMax,
		   pf->redShift, pf->greenShift, pf->blueShift);
	} else {
	    printf("  uses a colour map (not true colour).\n");
	}
    }
}
