/*
 * tight.c
 *
 * Routines to implement Tight Encoding
 */

/*
 *  Copyright (C) 2000 Const Kaplinsky.  All Rights Reserved.
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

#include <stdio.h>
#include "rfb.h"


/* Note: The following constant should not be changed. */
#define TIGHT_MIN_TO_COMPRESS 12

/* May be set to TRUE with "-lazytight" Xvnc option. */
Bool rfbTightDisableGradient = FALSE;


typedef struct COLOR_LIST_s {
    struct COLOR_LIST_s *next;
    int idx;
    CARD32 rgb;
} COLOR_LIST;

typedef struct PALETTE_ENTRY_s {
    COLOR_LIST *listNode;
    int numPixels;
} PALETTE_ENTRY;

typedef struct PALETTE_s {
    PALETTE_ENTRY entry[256];
    COLOR_LIST *hash[256];
    COLOR_LIST list[256];
} PALETTE;

typedef struct PALETTE8_s {
    CARD8 pixelValue[2];
    CARD8 colorIdx[256];
} PALETTE8;


static BOOL usePixelFormat24;

static int paletteMaxColors;
static int paletteNumColors;
static PALETTE palette;
static PALETTE8 palette8;

static int tightBeforeBufSize = 0;
static char *tightBeforeBuf = NULL;

static int tightAfterBufSize = 0;
static char *tightAfterBuf = NULL;

static int *prevRowBuf = NULL;


static int SendSubrect(rfbClientPtr cl, int x, int y, int w, int h);
static BOOL SendSolidRect(rfbClientPtr cl, int w, int h);
static BOOL SendIndexedRect(rfbClientPtr cl, int w, int h);
static BOOL SendFullColorRect(rfbClientPtr cl, int w, int h);
static BOOL SendGradientRect(rfbClientPtr cl, int w, int h);
static BOOL CompressData(rfbClientPtr cl, int streamId, int dataLen);

static void FillPalette8(int w, int h);
static void FillPalette16(int w, int h);
static void FillPalette32(int w, int h);

static void PaletteReset(void);
static int PaletteFind(CARD32 rgb);
static int PaletteInsert(CARD32 rgb, int numPixels);

static void Pack24(char *buf, rfbPixelFormat *fmt, int count);

static void EncodeIndexedRect8(CARD8 *buf, int w, int h);
static void EncodeIndexedRect16(CARD8 *buf, int w, int h);
static void EncodeIndexedRect32(CARD8 *buf, int w, int h);

static void FilterGradient24(char *buf, rfbPixelFormat *fmt, int w, int h);
static void FilterGradient16(CARD16 *buf, rfbPixelFormat *fmt, int w, int h);
static void FilterGradient32(CARD32 *buf, rfbPixelFormat *fmt, int w, int h);

static int DetectStillImage (rfbPixelFormat *fmt, int w, int h);
static int DetectStillImage24 (rfbPixelFormat *fmt, int w, int h);
static int DetectStillImage16 (rfbPixelFormat *fmt, int w, int h);
static int DetectStillImage32 (rfbPixelFormat *fmt, int w, int h);

/*
 * Tight encoding implementation.
 */

Bool
rfbSendRectEncodingTight(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    int maxBeforeSize, maxAfterSize;
    int subrectMaxWidth, subrectMaxHeight;
    int dx, dy;
    int rw, rh;

    if ( cl->format.depth == 24 && cl->format.redMax == 0xFF &&
         cl->format.greenMax == 0xFF && cl->format.blueMax == 0xFF ) {
        usePixelFormat24 = TRUE;
    } else {
        usePixelFormat24 = FALSE;
    }

    maxBeforeSize = TIGHT_MAX_RECT_SIZE * (cl->format.bitsPerPixel / 8);
    maxAfterSize = maxBeforeSize + (maxBeforeSize + 99) / 100 + 12;

    if (tightBeforeBufSize < maxBeforeSize) {
	tightBeforeBufSize = maxBeforeSize;
	if (tightBeforeBuf == NULL)
	    tightBeforeBuf = (char *)xalloc(tightBeforeBufSize);
	else
	    tightBeforeBuf = (char *)xrealloc(tightBeforeBuf,
                                              tightBeforeBufSize);
    }

    if (tightAfterBufSize < maxAfterSize) {
	tightAfterBufSize = maxAfterSize;
	if (tightAfterBuf == NULL)
	    tightAfterBuf = (char *)xalloc(tightAfterBufSize);
	else
	    tightAfterBuf = (char *)xrealloc(tightAfterBuf,
                                             tightAfterBufSize);
    }

    if (w > 2048 || w * h > TIGHT_MAX_RECT_SIZE) {
        subrectMaxWidth = (w > 2048) ? 2048 : w;
        subrectMaxHeight = TIGHT_MAX_RECT_SIZE / subrectMaxWidth;

        for (dy = 0; dy < h; dy += subrectMaxHeight) {
            for (dx = 0; dx < w; dx += 2048) {
                rw = (dx + 2048 < w) ? 2048 : w - dx;
                rh = (dy + subrectMaxHeight < h) ? subrectMaxHeight : h - dy;
                if (!SendSubrect(cl, x+dx, y+dy, rw, rh))
                    return FALSE;
            }
        }
    } else {
        if (!SendSubrect(cl, x, y, w, h))
            return FALSE;
    }

    return TRUE;
}

static int
SendSubrect(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    rfbFramebufferUpdateRectHeader rect;
    char *fbptr;
    int success = 0;

    if (ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingTight);

    memcpy(&updateBuf[ublen], (char *)&rect,
	   sz_rfbFramebufferUpdateRectHeader);
    ublen += sz_rfbFramebufferUpdateRectHeader;

    cl->rfbRectanglesSent[rfbEncodingTight]++;
    cl->rfbBytesSent[rfbEncodingTight] += sz_rfbFramebufferUpdateRectHeader;

    fbptr = (rfbScreen.pfbMemory + (rfbScreen.paddedWidthInBytes * y)
             + (x * (rfbScreen.bitsPerPixel / 8)));

    (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,
                       &cl->format, fbptr, tightBeforeBuf,
                       rfbScreen.paddedWidthInBytes, w, h);

    paletteMaxColors = w * h / 128;
    switch (cl->format.bitsPerPixel) {
    case 8:
        FillPalette8(w, h);
        break;
    case 16:
        FillPalette16(w, h);
        break;
    default:
        FillPalette32(w, h);
    }

    switch (paletteNumColors) {
    case 1:
        /* Solid rectangle */
        success = SendSolidRect(cl, w, h);
        break;
    case 0:
        /* Truecolor image */
        if (!rfbTightDisableGradient && DetectStillImage(&cl->format, w, h)) {
            success = SendGradientRect(cl, w, h);
        } else {
            success = SendFullColorRect(cl, w, h);
        }
        break;
    default:
        /* Up to 256 different colors */
        success = SendIndexedRect(cl, w, h);
    }
    return success;
}


/*
 * Subencoding implementations.
 */

static BOOL
SendSolidRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    int len;

    if (usePixelFormat24) {
        Pack24(tightBeforeBuf, &cl->format, 1);
        len = 3;
    } else
        len = cl->format.bitsPerPixel / 8;

    if (ublen + 1 + len > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    updateBuf[ublen++] = (char)(rfbTightFill << 4);
    memcpy (&updateBuf[ublen], tightBeforeBuf, len);
    ublen += len;

    cl->rfbBytesSent[rfbEncodingTight] += len + 1;

    return TRUE;
}

static BOOL
SendIndexedRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    int streamId, entryLen, dataLen;
    int x, y, i, width_bytes;

    if ( ublen + 6 + paletteNumColors * cl->format.bitsPerPixel / 8 >
         UPDATE_BUF_SIZE ) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    /* Prepare tight encoding header. */
    if (paletteNumColors == 2) {
        streamId = 1;
        dataLen = (w + 7) / 8;
        dataLen *= h;
    } else {
        streamId = 2;
        dataLen = w * h;
    }
    updateBuf[ublen++] = (streamId | rfbTightExplicitFilter) << 4;
    updateBuf[ublen++] = rfbTightFilterPalette;
    updateBuf[ublen++] = (char)(paletteNumColors - 1);

    /* Prepare palette, convert image. */
    switch (cl->format.bitsPerPixel) {
    case 32:
        for (i = 0; i < paletteNumColors; i++) {
            ((CARD32 *)tightAfterBuf)[i] =
                palette.entry[i].listNode->rgb;
        }

        if (usePixelFormat24) {
            Pack24(tightAfterBuf, &cl->format, paletteNumColors);
            entryLen = 3;
        } else
            entryLen = 4;

        memcpy(&updateBuf[ublen], tightAfterBuf, paletteNumColors * entryLen);
        ublen += paletteNumColors * entryLen;
        cl->rfbBytesSent[rfbEncodingTight] += paletteNumColors * entryLen + 3;

        EncodeIndexedRect32((CARD8 *)tightBeforeBuf, w, h);
        break;

    case 16:
        for (i = 0; i < paletteNumColors; i++) {
            ((CARD16 *)tightAfterBuf)[i] =
                (CARD16)palette.entry[i].listNode->rgb;
        }
        memcpy(&updateBuf[ublen], tightAfterBuf, paletteNumColors * 2);
        ublen += paletteNumColors * 2;
        cl->rfbBytesSent[rfbEncodingTight] += paletteNumColors * 2 + 3;

        EncodeIndexedRect16((CARD8 *)tightBeforeBuf, w, h);
        break;

    default:
        memcpy (&updateBuf[ublen], palette8.pixelValue, paletteNumColors);
        ublen += paletteNumColors;
        cl->rfbBytesSent[rfbEncodingTight] += paletteNumColors + 3;

        EncodeIndexedRect8((CARD8 *)tightBeforeBuf, w, h);
    }

    return CompressData(cl, streamId, dataLen);
}

static BOOL
SendFullColorRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    int len;

    if (ublen + TIGHT_MIN_TO_COMPRESS + 1 > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    updateBuf[ublen++] = 0x00;  /* stream id = 0, no flushing, no filter */
    cl->rfbBytesSent[rfbEncodingTight]++;

    if (usePixelFormat24) {
        Pack24(tightBeforeBuf, &cl->format, w * h);
        len = 3;
    } else
        len = cl->format.bitsPerPixel / 8;

    return CompressData(cl, 0, w * h * len);
}

static BOOL
SendGradientRect(cl, w, h)
    rfbClientPtr cl;
    int w, h;
{
    int len;

    if (cl->format.bitsPerPixel == 8)
        return SendFullColorRect(cl, w, h);

    if (ublen + TIGHT_MIN_TO_COMPRESS + 2 > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    if (prevRowBuf == NULL)
        prevRowBuf = (int *)xalloc(2048 * 3 * sizeof(int));

    updateBuf[ublen++] = (3 | rfbTightExplicitFilter) << 4;
    updateBuf[ublen++] = rfbTightFilterGradient;
    cl->rfbBytesSent[rfbEncodingTight] += 2;

    if (usePixelFormat24) {
        FilterGradient24(tightBeforeBuf, &cl->format, w, h);
        len = 3;
    } else if (cl->format.bitsPerPixel == 32) {
        FilterGradient32((CARD32 *)tightBeforeBuf, &cl->format, w, h);
        len = 4;
    } else {
        FilterGradient16((CARD16 *)tightBeforeBuf, &cl->format, w, h);
        len = 2;
    }

    return CompressData(cl, 3, w * h * len);
}

static BOOL
CompressData(cl, streamId, dataLen)
    rfbClientPtr cl;
    int streamId, dataLen;
{
    z_streamp pz;
    int compressedLen, portionLen;
    int i, err;

    if (dataLen < TIGHT_MIN_TO_COMPRESS) {
        memcpy(&updateBuf[ublen], tightBeforeBuf, dataLen);
        ublen += dataLen;
        cl->rfbBytesSent[rfbEncodingTight] += dataLen;
        return TRUE;
    }

    pz = &cl->zsStruct[streamId];

    /* Initialize compression stream. */
    if (!cl->zsActive[streamId]) {
        pz->zalloc = Z_NULL;
        pz->zfree = Z_NULL;
        pz->opaque = Z_NULL;

        if (streamId == 3) {
            err = deflateInit2 (pz, 6, Z_DEFLATED, MAX_WBITS,
                                MAX_MEM_LEVEL, Z_FILTERED);
        } else {
            err = deflateInit2 (pz, 9, Z_DEFLATED, MAX_WBITS,
                                MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
        }

        if (err != Z_OK)
            return FALSE;

        cl->zsActive[streamId] = TRUE;
    }

    /* Actual compression. */
    pz->next_in = (Bytef *)tightBeforeBuf;
    pz->avail_in = dataLen;
    pz->next_out = (Bytef *)tightAfterBuf;
    pz->avail_out = tightAfterBufSize;

    if ( deflate (pz, Z_SYNC_FLUSH) != Z_OK ||
         pz->avail_in != 0 || pz->avail_out == 0 ) {
        return FALSE;
    }

    compressedLen = (size_t)(tightAfterBufSize - pz->avail_out);
    cl->rfbBytesSent[rfbEncodingTight] += compressedLen + 1;

    /* Prepare compressed data size for sending. */
    updateBuf[ublen++] = compressedLen & 0x7F;
    if (compressedLen > 0x7F) {
        updateBuf[ublen-1] |= 0x80;
        updateBuf[ublen++] = compressedLen >> 7 & 0x7F;
        cl->rfbBytesSent[rfbEncodingTight]++;
        if (compressedLen > 0x3FFF) {
            updateBuf[ublen-1] |= 0x80;
            updateBuf[ublen++] = compressedLen >> 14 & 0xFF;
            cl->rfbBytesSent[rfbEncodingTight]++;
        }
    }

    /* Send update. */
    for (i = 0; i < compressedLen; ) {
        portionLen = compressedLen - i;
        if (portionLen > UPDATE_BUF_SIZE - ublen)
            portionLen = UPDATE_BUF_SIZE - ublen;

        memcpy(&updateBuf[ublen], &tightAfterBuf[i], portionLen);

        ublen += portionLen;
        i += portionLen;

        if (!rfbSendUpdateBuf(cl))
            return FALSE;
    }

    return TRUE;
}


/*
 * Code to determine how many different colors used in rectangle.
 */

static void
FillPalette8(w, h)
    int w, h;
{
    CARD8 *data = (CARD8 *)tightBeforeBuf;
    CARD8 c0, c1;
    int i, n0, n1;

    paletteNumColors = 0;

    c0 = data[0];
    for (i = 1; i < w * h && data[i] == c0; i++);
    if (i == w * h) {
        paletteNumColors = 1;
        return;                 /* Solid rectangle */
    }

    if (paletteMaxColors < 2)
        return;

    n0 = i;
    c1 = data[i];
    n1 = 0;
    for (i++; i < w * h; i++) {
        if (data[i] == c0) {
            n0++;
        } else if (data[i] == c1) {
            n1++;
        } else
            break;
    }
    if (i == w * h) {
        if (n1 > n0) {
            palette8.pixelValue[0] = c0;
            palette8.pixelValue[1] = c1;
            palette8.colorIdx[c0] = 0;
            palette8.colorIdx[c1] = 1;
        } else {
            palette8.pixelValue[0] = c1;
            palette8.pixelValue[1] = c0;
            palette8.colorIdx[c0] = 1;
            palette8.colorIdx[c1] = 0;
        }
        paletteNumColors = 2;   /* Two colors */
    }
}

#define DEFINE_FILL_PALETTE_FUNCTION(bpp)                               \
                                                                        \
static void                                                             \
FillPalette##bpp(w, h)                                                  \
    int w, h;                                                           \
{                                                                       \
    CARD##bpp *data = (CARD##bpp *)tightBeforeBuf;                      \
    CARD##bpp c0, c1, ci;                                               \
    int i, n0, n1, ni;                                                  \
                                                                        \
    PaletteReset();                                                     \
                                                                        \
    c0 = data[0];                                                       \
    for (i = 1; i < w * h && data[i] == c0; i++);                       \
    if (i == w * h) {                                                   \
        paletteNumColors = 1;                                           \
        return;                 /* Solid rectangle */                   \
    }                                                                   \
                                                                        \
    if (paletteMaxColors < 2)                                           \
        return;                                                         \
                                                                        \
    n0 = i;                                                             \
    c1 = data[i];                                                       \
    n1 = 0;                                                             \
    for (i++; i < w * h; i++) {                                         \
        ci = data[i];                                                   \
        if (ci == c0) {                                                 \
            n0++;                                                       \
        } else if (ci == c1) {                                          \
            n1++;                                                       \
        } else                                                          \
            break;                                                      \
    }                                                                   \
    PaletteInsert (c0, (CARD32)n0);                                     \
    PaletteInsert (c1, (CARD32)n1);                                     \
    if (i == w * h)                                                     \
        return;                 /* Two colors */                        \
                                                                        \
    ni = 1;                                                             \
    for (i++; i < w * h; i++) {                                         \
        if (data[i] == ci) {                                            \
            ni++;                                                       \
        } else {                                                        \
            if (!PaletteInsert (ci, (CARD32)ni))                        \
                return;                                                 \
            ci = data[i];                                               \
            ni = 1;                                                     \
        }                                                               \
    }                                                                   \
    PaletteInsert (ci, (CARD32)ni);                                     \
}

DEFINE_FILL_PALETTE_FUNCTION(16)
DEFINE_FILL_PALETTE_FUNCTION(32)


/*
 * Functions to operate with palette structures.
 */

static void
PaletteReset(void)
{
    int i;

    paletteNumColors = 0;
    for (i = 0; i < 256; i++)
        palette.hash[i] = NULL;
}

static int
PaletteFind(rgb)
    CARD32 rgb;
{
    COLOR_LIST *pnode;

    if (rgb & 0xFF000000) {
        pnode = palette.hash[(int)((rgb >> 24) + (rgb >> 16) & 0xFF)];
    } else {
        pnode = palette.hash[(int)((rgb >> 8) + rgb & 0xFF)];
    }

    while (pnode != NULL) {
        if (pnode->rgb == rgb)
            return pnode->idx;
        pnode = pnode->next;
    }
    return -1;
}

static int
PaletteInsert(rgb, numPixels)
    CARD32 rgb;
    int numPixels;
{
    COLOR_LIST *pnode;
    COLOR_LIST *prev_pnode = NULL;
    int hash_key, idx, new_idx, count;

    if (rgb & 0xFF000000) {
        hash_key = (int)((rgb >> 24) + (rgb >> 16) & 0xFF);
    } else {
        hash_key = (int)((rgb >> 8) + rgb & 0xFF);
    }
    pnode = palette.hash[hash_key];

    while (pnode != NULL) {
        if (pnode->rgb == rgb) {
            /* Such palette entry already exists. */
            new_idx = idx = pnode->idx;
            count = palette.entry[idx].numPixels + numPixels;
            if (new_idx && palette.entry[new_idx-1].numPixels < count) {
                do {
                    palette.entry[new_idx] = palette.entry[new_idx-1];
                    palette.entry[new_idx].listNode->idx = new_idx;
                    new_idx--;
                }
                while (new_idx && palette.entry[new_idx-1].numPixels < count);
                palette.entry[new_idx].listNode = pnode;
                pnode->idx = new_idx;
            }
            palette.entry[new_idx].numPixels = count;
            return paletteNumColors;
        }
        prev_pnode = pnode;
        pnode = pnode->next;
    }

    /* Check if palette is full. */
    if (paletteNumColors == 256 || paletteNumColors == paletteMaxColors) {
        paletteNumColors = 0;
        return 0;
    }

    /* Move palette entries with lesser pixel counts. */
    for ( idx = paletteNumColors;
          idx > 0 && palette.entry[idx-1].numPixels < numPixels;
          idx-- ) {
        palette.entry[idx] = palette.entry[idx-1];
        palette.entry[idx].listNode->idx = idx;
    }

    /* Add new palette entry into the freed slot. */
    pnode = &palette.list[paletteNumColors];
    if (prev_pnode != NULL) {
        prev_pnode->next = pnode;
    } else {
        palette.hash[hash_key] = pnode;
    }
    pnode->next = NULL;
    pnode->idx = idx;
    pnode->rgb = rgb;
    palette.entry[idx].listNode = pnode;
    palette.entry[idx].numPixels = numPixels;

    return (++paletteNumColors);
}


/*
 * Converting 32-bit color samples into 24-bit colors.
 * Should be called only when redMax, greenMax and blueMax are 256.
 * 8-bit samples assumed to be byte-aligned.
 */

static void Pack24(buf, fmt, count)
    char *buf;
    rfbPixelFormat *fmt;
    int count;
{
    int i;
    CARD32 pix;
    int r_shift, g_shift, b_shift;

    if (!rfbServerFormat.bigEndian == !fmt->bigEndian) {
        r_shift = fmt->redShift;
        g_shift = fmt->greenShift;
        b_shift = fmt->blueShift;
    } else {
        r_shift = 24 - fmt->redShift;
        g_shift = 24 - fmt->greenShift;
        b_shift = 24 - fmt->blueShift;
    }

    for (i = 0; i < count; i++) {
        pix = ((CARD32 *)buf)[i];
        buf[i*3]   = (char)(pix >> r_shift);
        buf[i*3+1] = (char)(pix >> g_shift);
        buf[i*3+2] = (char)(pix >> b_shift);
    }
}


/*
 * Converting truecolor samples into palette indices.
 */

#define PaletteFind8(c)  palette8.colorIdx[(c)]
#define PaletteFind16    PaletteFind
#define PaletteFind32    PaletteFind

#define DEFINE_IDX_ENCODE_FUNCTION(bpp)                                 \
                                                                        \
static void                                                             \
EncodeIndexedRect##bpp(buf, w, h)                                       \
    CARD8 *buf;                                                         \
    int w, h;                                                           \
{                                                                       \
    int x, y, i, width_bytes;                                           \
                                                                        \
    if (paletteNumColors != 2) {                                        \
      for (i = 0; i < w * h; i++)                                       \
        buf[i] = (CARD8)PaletteFind(((CARD##bpp *)buf)[i]);             \
      return;                                                           \
    }                                                                   \
                                                                        \
    width_bytes = (w + 7) / 8;                                          \
    for (y = 0; y < h; y++) {                                           \
      for (x = 0; x < w / 8; x++) {                                     \
        for (i = 0; i < 8; i++)                                         \
          buf[y*width_bytes+x] = (buf[y*width_bytes+x] << 1) |          \
            (PaletteFind##bpp (((CARD##bpp *)buf)[y*w+x*8+i]) & 1);     \
      }                                                                 \
      buf[y*width_bytes+x] = 0;                                         \
      for (i = 0; i < w % 8; i++) {                                     \
        buf[y*width_bytes+x] |=                                         \
          (PaletteFind##bpp (((CARD##bpp *)buf)[y*w+x*8+i]) & 1) <<     \
            (7 - i);                                                    \
      }                                                                 \
    }                                                                   \
}

DEFINE_IDX_ENCODE_FUNCTION(8)
DEFINE_IDX_ENCODE_FUNCTION(16)
DEFINE_IDX_ENCODE_FUNCTION(32)


/*
 * ``Gradient'' filter for 24-bit color samples.
 * Should be called only when redMax, greenMax and blueMax are 256.
 * 8-bit samples assumed to be byte-aligned.
 */

static void FilterGradient24(buf, fmt, w, h)
    char *buf;
    rfbPixelFormat *fmt;
    int w, h;
{
    CARD32 *buf32;
    CARD32 pix32;
    int *prevRowPtr;
    int shiftBits[3];
    int pixHere[3], pixUpper[3], pixLeft[3], pixUpperLeft[3];
    int prediction;
    int x, y, c;

    buf32 = (CARD32 *)buf;
    memset (prevRowBuf, 0, w * 3 * sizeof(int));

    if (!rfbServerFormat.bigEndian == !fmt->bigEndian) {
        shiftBits[0] = fmt->redShift;
        shiftBits[1] = fmt->greenShift;
        shiftBits[2] = fmt->blueShift;
    } else {
        shiftBits[0] = 24 - fmt->redShift;
        shiftBits[1] = 24 - fmt->greenShift;
        shiftBits[2] = 24 - fmt->blueShift;
    }

    for (y = 0; y < h; y++) {
        for (c = 0; c < 3; c++) {
            pixUpper[c] = 0;
            pixHere[c] = 0;
        }
        prevRowPtr = prevRowBuf;
        for (x = 0; x < w; x++) {
            pix32 = *buf32++;
            for (c = 0; c < 3; c++) {
                pixUpperLeft[c] = pixUpper[c];
                pixLeft[c] = pixHere[c];
                pixUpper[c] = *prevRowPtr;
                pixHere[c] = (int)(pix32 >> shiftBits[c] & 0xFF);
                *prevRowPtr++ = pixHere[c];

                prediction = pixLeft[c] + pixUpper[c] - pixUpperLeft[c];
                if (prediction < 0) {
                    prediction = 0;
                } else if (prediction > 0xFF) {
                    prediction = 0xFF;
                }
                *buf++ = (char)(pixHere[c] - prediction);
            }
        }
    }
}


/*
 * ``Gradient'' filter for other color depths.
 */

#define DEFINE_GRADIENT_FILTER_FUNCTION(bpp)                             \
                                                                         \
static void FilterGradient##bpp(buf, fmt, w, h)                          \
    CARD##bpp *buf;                                                      \
    rfbPixelFormat *fmt;                                                 \
    int w, h;                                                            \
{                                                                        \
    CARD##bpp pix, diff;                                                 \
    BOOL endianMismatch;                                                 \
    int *prevRowPtr;                                                     \
    int maxColor[3], shiftBits[3];                                       \
    int pixHere[3], pixUpper[3], pixLeft[3], pixUpperLeft[3];            \
    int prediction;                                                      \
    int x, y, c;                                                         \
                                                                         \
    memset (prevRowBuf, 0, w * 3 * sizeof(int));                         \
                                                                         \
    endianMismatch = (!rfbServerFormat.bigEndian != !fmt->bigEndian);    \
                                                                         \
    maxColor[0] = fmt->redMax;                                           \
    maxColor[1] = fmt->greenMax;                                         \
    maxColor[2] = fmt->blueMax;                                          \
    shiftBits[0] = fmt->redShift;                                        \
    shiftBits[1] = fmt->greenShift;                                      \
    shiftBits[2] = fmt->blueShift;                                       \
                                                                         \
    for (y = 0; y < h; y++) {                                            \
        for (c = 0; c < 3; c++) {                                        \
            pixUpper[c] = 0;                                             \
            pixHere[c] = 0;                                              \
        }                                                                \
        prevRowPtr = prevRowBuf;                                         \
        for (x = 0; x < w; x++) {                                        \
            pix = *buf;                                                  \
            if (endianMismatch) {                                        \
                pix = Swap##bpp(pix);                                    \
            }                                                            \
            diff = 0;                                                    \
            for (c = 0; c < 3; c++) {                                    \
                pixUpperLeft[c] = pixUpper[c];                           \
                pixLeft[c] = pixHere[c];                                 \
                pixUpper[c] = *prevRowPtr;                               \
                pixHere[c] = (int)(pix >> shiftBits[c] & maxColor[c]);   \
                *prevRowPtr++ = pixHere[c];                              \
                                                                         \
                prediction = pixLeft[c] + pixUpper[c] - pixUpperLeft[c]; \
                if (prediction < 0) {                                    \
                    prediction = 0;                                      \
                } else if (prediction > maxColor[c]) {                   \
                    prediction = maxColor[c];                            \
                }                                                        \
                diff |= ((pixHere[c] - prediction) & maxColor[c])        \
                    << shiftBits[c];                                     \
            }                                                            \
            if (endianMismatch) {                                        \
                diff = Swap##bpp(diff);                                  \
            }                                                            \
            *buf++ = diff;                                               \
        }                                                                \
    }                                                                    \
}

DEFINE_GRADIENT_FILTER_FUNCTION(16)
DEFINE_GRADIENT_FILTER_FUNCTION(32)


/*
 * Code to guess if given rectangle is suitable for still image
 * compression.
 */

#define DETECT_SUBROW_WIDTH   7
#define DETECT_MIN_WIDTH      8
#define DETECT_MIN_HEIGHT     8
#define DETECT_MIN_SIZE    8192

static int DetectStillImage (fmt, w, h)
    rfbPixelFormat *fmt;
    int w, h;
{
    if ( fmt->bitsPerPixel == 8 || w * h < DETECT_MIN_SIZE ||
         w < DETECT_MIN_WIDTH || h < DETECT_MIN_HEIGHT ) {
        return 0;
    }

    if (fmt->bitsPerPixel == 32) {
        if (usePixelFormat24) {
            return DetectStillImage24(fmt, w, h);
        } else {
            return DetectStillImage32(fmt, w, h);
        }
    } else {
        return DetectStillImage16(fmt, w, h);
    }
}

static int DetectStillImage24 (fmt, w, h)
    rfbPixelFormat *fmt;
    int w, h;
{
    int off;
    int x, y, d, dx, c;
    int diffStat[256];
    int pixelCount = 0;
    int pix, left[3];
    unsigned long avgError;

    /* If client is big-endian, color samples begin from the second
       byte (offset 1) of a 32-bit pixel value. */
    off = (fmt->bigEndian != 0);

    memset(diffStat, 0, 256*sizeof(int));

    y = 0, x = 0;
    while (y < h && x < w) {
        for (d = 0; d < h - y && d < w - x - DETECT_SUBROW_WIDTH; d++) {
            for (c = 0; c < 3; c++) {
                left[c] = (int)tightBeforeBuf[((y+d)*w+x+d)*4+off+c] & 0xFF;
            }
            for (dx = 1; dx <= DETECT_SUBROW_WIDTH; dx++) {
                for (c = 0; c < 3; c++) {
                    pix = (int)tightBeforeBuf[((y+d)*w+x+d+dx)*4+off+c] & 0xFF;
                    diffStat[abs(pix - left[c])]++;
                    left[c] = pix;
                }
                pixelCount++;
            }
        }
        if (w > h) {
            x += h;
            y = 0;
        } else {
            x = 0;
            y += w;
        }
    }

    if (diffStat[0] * 33 / pixelCount >= 95)
        return 0;

    avgError = 0;
    for (c = 1; c < 8; c++) {
        avgError += (unsigned long)diffStat[c] * (unsigned long)(c * c);
        if (diffStat[c] == 0 || diffStat[c] > diffStat[c-1] * 2)
            return 0;
    }
    for (; c < 256; c++) {
        avgError += (unsigned long)diffStat[c] * (unsigned long)(c * c);
    }
    avgError /= (pixelCount * 3 - diffStat[0]);

    return (avgError < 500);
}

#define DEFINE_DETECT_FUNCTION(bpp)                                          \
                                                                             \
static int DetectStillImage##bpp (fmt, w, h)                                 \
    rfbPixelFormat *fmt;                                                     \
    int w, h;                                                                \
{                                                                            \
    BOOL endianMismatch;                                                     \
    CARD##bpp pix;                                                           \
    int maxColor[3], shiftBits[3];                                           \
    int x, y, d, dx, c;                                                      \
    int diffStat[256];                                                       \
    int pixelCount = 0;                                                      \
    int sample, sum, left[3];                                                \
    unsigned long avgError;                                                  \
                                                                             \
    endianMismatch = (!rfbServerFormat.bigEndian != !fmt->bigEndian);        \
                                                                             \
    maxColor[0] = fmt->redMax;                                               \
    maxColor[1] = fmt->greenMax;                                             \
    maxColor[2] = fmt->blueMax;                                              \
    shiftBits[0] = fmt->redShift;                                            \
    shiftBits[1] = fmt->greenShift;                                          \
    shiftBits[2] = fmt->blueShift;                                           \
                                                                             \
    memset(diffStat, 0, 256*sizeof(int));                                    \
                                                                             \
    y = 0, x = 0;                                                            \
    while (y < h && x < w) {                                                 \
        for (d = 0; d < h - y && d < w - x - DETECT_SUBROW_WIDTH; d++) {     \
            pix = ((CARD##bpp *)tightBeforeBuf)[(y+d)*w+x+d];                \
            if (endianMismatch) {                                            \
                pix = Swap##bpp(pix);                                        \
            }                                                                \
            for (c = 0; c < 3; c++) {                                        \
                left[c] = (int)(pix >> shiftBits[c] & maxColor[c]);          \
            }                                                                \
            for (dx = 1; dx <= DETECT_SUBROW_WIDTH; dx++) {                  \
                pix = ((CARD##bpp *)tightBeforeBuf)[(y+d)*w+x+d+dx];         \
                if (endianMismatch) {                                        \
                    pix = Swap##bpp(pix);                                    \
                }                                                            \
                sum = 0;                                                     \
                for (c = 0; c < 3; c++) {                                    \
                    sample = (int)(pix >> shiftBits[c] & maxColor[c]);       \
                    sum += abs(sample - left[c]);                            \
                    left[c] = sample;                                        \
                }                                                            \
                if (sum > 255)                                               \
                    sum = 255;                                               \
                diffStat[sum]++;                                             \
                pixelCount++;                                                \
            }                                                                \
        }                                                                    \
        if (w > h) {                                                         \
            x += h;                                                          \
            y = 0;                                                           \
        } else {                                                             \
            x = 0;                                                           \
            y += w;                                                          \
        }                                                                    \
    }                                                                        \
                                                                             \
    if ((diffStat[0] + diffStat[1]) * 100 / pixelCount >= 90)                \
        return 0;                                                            \
                                                                             \
    avgError = 0;                                                            \
    for (c = 1; c < 8; c++) {                                                \
        avgError += (unsigned long)diffStat[c] * (unsigned long)(c * c);     \
        if (diffStat[c] == 0 || diffStat[c] > diffStat[c-1] * 2)             \
            return 0;                                                        \
    }                                                                        \
    for (; c < 256; c++) {                                                   \
        avgError += (unsigned long)diffStat[c] * (unsigned long)(c * c);     \
    }                                                                        \
    avgError /= (pixelCount - diffStat[0]);                                  \
                                                                             \
    return (avgError < 200);                                                 \
}

DEFINE_DETECT_FUNCTION(16)
DEFINE_DETECT_FUNCTION(32)

