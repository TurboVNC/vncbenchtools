/*
 * rfb.h - header file for RFB DDX implementation.
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include <zlib.h>

typedef unsigned char  CARD8;
typedef unsigned short CARD16;
typedef unsigned int   CARD32;

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)

#include "rfbproto.h"

#define MAX_ENCODINGS 10

typedef int BOOL, Bool;
#define FALSE  0
#define TRUE   1
#define False  0
#define True   1

#define xalloc malloc
#define xrealloc realloc

/*
 * Per-screen (framebuffer) structure.  There is only one of these, since we
 * don't allow the X server to have multiple screens.
 */

typedef struct
{
    int width;
    int paddedWidthInBytes;
    int height;
    int sizeInBytes;
    int bitsPerPixel;
    char pfbMemory[1280*1024*4];
} rfbScreenInfo, *rfbScreenInfoPtr;


typedef struct
{
  int width, height;
  char *data;
  int bytes_per_line;
  int bits_per_pixel;
} XImage;


/*
 * Per-client structure.
 */

struct rfbClientRec;
typedef void (*rfbTranslateFnType)(char *table, rfbPixelFormat *in,
				   rfbPixelFormat *out,
				   char *iptr, char *optr,
				   int bytesBetweenInputLines,
				   int width, int height);

typedef struct rfbClientRec {

    rfbPixelFormat format;

    /* statistics */

    int rfbBytesSent[MAX_ENCODINGS];
    int rfbRectanglesSent[MAX_ENCODINGS];
    int rfbFramebufferUpdateMessagesSent;
    int rfbRawBytesEquivalent;

    rfbTranslateFnType translateFn;

    char *translateLookupTable;

    /* tight encoding -- preserve zlib streams' state for each client */

    z_stream zsStruct[4];
    Bool zsActive[4];

    /* For the zlib encoding, necessary compression state info per client. */
    struct z_stream_s compStream;
    Bool compStreamInited;

    int zsLevel[4];

} rfbClientRec, *rfbClientPtr;

extern rfbClientRec rfbClient;

/*
 * Macros for endian swapping.
 */

#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))

#define Swap32(l) (((l) >> 24) | \
		   (((l) & 0x00ff0000) >> 8)  | \
		   (((l) & 0x0000ff00) << 8)  | \
		   ((l) << 24))


/* init.c */

static const int rfbEndianTest = 1;

#define Swap16IfLE(s) (*(const char *)&rfbEndianTest ? Swap16(s) : (s))

#define Swap32IfLE(l) (*(const char *)&rfbEndianTest ? Swap32(l) : (l))

extern rfbScreenInfo rfbScreen;

/* rfbserver.c */

/*
 * UPDATE_BUF_SIZE must be big enough to send at least one whole line of the
 * framebuffer.  So for a max screen width of say 2K with 32-bit pixels this
 * means 8K minimum.
 */

#define UPDATE_BUF_SIZE 30000
extern char updateBuf[UPDATE_BUF_SIZE];
extern int ublen;

extern Bool rfbSendUpdateBuf(rfbClientPtr cl);

/*
 * This must be big enough to hold a single 1280x1024 raw rectangle
 */
#define SEND_BUF_SIZE (5*1024*1024)
extern char *sendBuf;
extern int sblen, sbptr;

/* translate.c */

extern rfbPixelFormat rfbServerFormat;

extern void rfbTranslateNone(char *table, rfbPixelFormat *in,
			     rfbPixelFormat *out,
			     char *iptr, char *optr,
			     int bytesBetweenInputLines,
			     int width, int height);

/* hextile.c */

extern Bool rfbSendRectEncodingHextile(rfbClientPtr cl, int x, int y, int w,
				       int h);


/* tight.c */

/*** For tight-1.0 only ***/
#define TIGHT_MAX_RECT_WIDTH 512
#define TIGHT_MAX_RECT_HEIGHT 128
/*** For tight-1.0 only ***/

#define TIGHT_MAX_RECT_SIZE  65536

extern Bool rfbTightDisableGradient;

extern Bool rfbSendRectEncodingTight(rfbClientPtr cl, int x,int y,int w,int h);


/* zlib.h */

/* Minimum zlib rectangle size in bytes.  Anything smaller will
 * not compress well due to overhead.
 */
#define VNC_ENCODE_ZLIB_MIN_COMP_SIZE (17)

/* Set maximum zlib rectangle size in pixels.  Always allow at least
 * two scan lines.
 */
#define ZLIB_MAX_RECT_SIZE (128*256)
#define ZLIB_MAX_SIZE(min) ((( min * 2 ) > ZLIB_MAX_RECT_SIZE ) ? ( min * 2 ) : ZLIB_MAX_RECT_SIZE )

extern Bool rfbSendRectEncodingZlib(rfbClientPtr cl, int x, int y, int w,
				       int h);


/*  */

extern void InitEverything (int color_depth);

extern int rfbLog (char *fmt, ...);

extern Bool rfbSendRectEncodingRaw(rfbClientPtr cl, int x,int y,int w,int h);

extern Bool ReadFromRFBServer(char *out, unsigned int n);
