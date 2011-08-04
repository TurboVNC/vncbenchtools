/* Copyright (C) 2000-2003 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright (C) 2011 D. R. Commander.  All Rights Reserved.
 *    
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */
#include "tiger-1.2/rfb/encodings.h"
#include "tiger-1.2/rfb/TightEncoder.h"
#include "tiger-1.2/rdr/ZlibOutStream.cxx"
#include "tiger-1.2/rfb/PixelFormat.cxx"
#include "tiger-1.2/rfb/Rect.h"
#include "tiger-1.2/rdr/Exception.h"
#include "tiger-1.2/rdr/Exception.cxx"
extern "C" {
  #include "rfb.h"
}

using namespace rfb;

// Minimum amount of data to be compressed. This value should not be
// changed, doing so will break compatibility with existing clients.
#define TIGHT_MIN_TO_COMPRESS 12

// Adjustable parameters.
// FIXME: Get rid of #defines
#define TIGHT_JPEG_MIN_RECT_SIZE     1024
#define TIGHT_DETECT_MIN_WIDTH          8
#define TIGHT_DETECT_MIN_HEIGHT         8
#define TIGHT_MAX_SPLIT_TILE_SIZE      16
#define TIGHT_MIN_SPLIT_RECT_SIZE    4096
#define TIGHT_MIN_SOLID_SUBRECT_SIZE 2048

//
// Compression level stuff. The following array contains various
// encoder parameters for each of 10 compression levels (0..9).
// Last three parameters correspond to JPEG quality levels (0..9).
//
// NOTE: s_conf[9].maxRectSize should be >= s_conf[i].maxRectSize,
// where i in [0..8]. RequiredBuffSize() method depends on this.
// FIXME: Is this comment obsolete?
//

// NOTE:  The JPEG quality and subsampling levels below were obtained
// experimentally by the VirtualGL Project.  They represent the approximate
// average compression ratios listed below, as measured across the set of
// every 10th frame in the SPECviewperf 9 benchmark suite.
//
// 9 = JPEG quality 100, no subsampling (ratio ~= 10:1)
//     [this should be lossless, except for round-off error]
// 8 = JPEG quality 92,  no subsampling (ratio ~= 20:1)
//     [this should be perceptually lossless, based on current research]
// 7 = JPEG quality 86,  no subsampling (ratio ~= 25:1)
// 6 = JPEG quality 79,  no subsampling (ratio ~= 30:1)
// 5 = JPEG quality 77,  4:2:2 subsampling (ratio ~= 40:1)
// 4 = JPEG quality 62,  4:2:2 subsampling (ratio ~= 50:1)
// 3 = JPEG quality 42,  4:2:2 subsampling (ratio ~= 60:1)
// 2 = JPEG quality 41,  4:2:0 subsampling (ratio ~= 70:1)
// 1 = JPEG quality 29,  4:2:0 subsampling (ratio ~= 80:1)
// 0 = JPEG quality 15,  4:2:0 subsampling (ratio ~= 100:1)

static const TIGHT_CONF conf[10] = {
  {   512,   32,   6, 0, 0, 0,   4, 15, SUBSAMP_420 }, // 0
  {  2048,   64,   6, 1, 1, 1,   8, 29, SUBSAMP_420 }, // 1
  {  4096,  128,   8, 3, 3, 2,  24, 41, SUBSAMP_420 }, // 2
  {  8192,  256,  12, 5, 5, 2,  32, 42, SUBSAMP_422 }, // 3
  { 16384,  512,  12, 6, 7, 3,  32, 62, SUBSAMP_422 }, // 4
  { 32768,  512,  12, 7, 8, 4,  32, 77, SUBSAMP_422 }, // 5
  { 65536, 1024,  16, 7, 8, 5,  32, 79, SUBSAMP_NONE }, // 6
  { 65536, 1024,  16, 8, 9, 6,  64, 86, SUBSAMP_NONE }, // 7
  { 65536, 2048,  24, 9, 9, 7,  64, 92, SUBSAMP_NONE }, // 8
  { 65536, 2048,  32, 1, 1, 1,  96,100, SUBSAMP_NONE }  // 9
};

static const int compressLevel = 9;
static const int qualityLevel = 8;

static const TIGHT_CONF* s_pconf;
static const TIGHT_CONF* s_pjconf;
static rdr::MemOutStream mos;
static rdr::ZlibOutStream *zos = NULL;
static rfbClientPtr cl = NULL;
static rdr::U8* imageBuf = NULL;
static int imageBufSize = 0;

unsigned long solidrect=0, solidpixels=0, monorect=0, monopixels=0, ndxrect=0,
	ndxpixels=0, jpegrect=0, jpegpixels=0, fcrect=0, fcpixels=0, gradrect=0,
	gradpixels=0;

//
// Including BPP-dependent implementation of the encoder.
//

#define BPP 8
#include "tiger-1.2/rfb/tightEncode.h"
#undef BPP
#define BPP 16
#include "tiger-1.2/rfb/tightEncode.h"
#undef BPP
#define BPP 32
#include "tiger-1.2/rfb/tightEncode.h"
#undef BPP

rdr::U8* getImageBuf(int required, const PixelFormat& pf)
{
  int requiredBytes = required * (pf.bpp / 8);
  int size = requiredBytes;

  if (imageBufSize < size) {
    imageBufSize = size;
    delete [] imageBuf;
    imageBuf = new rdr::U8[imageBufSize];
  }
  return imageBuf;
}

static bool checkSolidTile(Rect& r, CARD32* colorPtr, bool needSameColor)
{
  switch(rfbServerFormat.bitsPerPixel) {
  case 32:
    return checkSolidTile32(r, colorPtr, needSameColor);
  case 16:
    return checkSolidTile16(r, colorPtr, needSameColor);
  default:
    return checkSolidTile8(r, colorPtr, needSameColor);
  }
}

static void findBestSolidArea(Rect& r, CARD32 colorValue, Rect& bestr)
{
  int dx, dy, dw, dh;
  int w_prev;
  Rect sr;
  int w_best = 0, h_best = 0;

  bestr.tl.x = bestr.br.x = r.tl.x;
  bestr.tl.y = bestr.br.y = r.tl.y;

  w_prev = r.width();

  for (dy = r.tl.y; dy < r.br.y; dy += TIGHT_MAX_SPLIT_TILE_SIZE) {

    dh = (dy + TIGHT_MAX_SPLIT_TILE_SIZE <= r.br.y) ?
      TIGHT_MAX_SPLIT_TILE_SIZE : (r.br.y - dy);
    dw = (w_prev > TIGHT_MAX_SPLIT_TILE_SIZE) ?
      TIGHT_MAX_SPLIT_TILE_SIZE : w_prev;

    sr.setXYWH(r.tl.x, dy, dw, dh);
    if (!checkSolidTile(sr, &colorValue, true))
      break;

    for (dx = r.tl.x + dw; dx < r.tl.x + w_prev;) {
      dw = (dx + TIGHT_MAX_SPLIT_TILE_SIZE <= r.tl.x + w_prev) ?
        TIGHT_MAX_SPLIT_TILE_SIZE : (r.tl.x + w_prev - dx);
      sr.setXYWH(dx, dy, dw, dh);
      if (!checkSolidTile(sr, &colorValue, true))
        break;
	    dx += dw;
    }

    w_prev = dx - r.tl.x;
    if (w_prev * (dy + dh - r.tl.y) > w_best * h_best) {
      w_best = w_prev;
      h_best = dy + dh - r.tl.y;
    }
  }

  bestr.br.x = bestr.tl.x + w_best;
  bestr.br.y = bestr.tl.y + h_best;
}

static void extendSolidArea(const Rect& r, CARD32 colorValue, Rect& er)
{
  int cx, cy;
  Rect sr;

  // Try to extend the area upwards.
  for (cy = er.tl.y - 1; ; cy--) {
    sr.setXYWH(er.tl.x, cy, er.width(), 1);
    if (cy < r.tl.y || !checkSolidTile(sr, &colorValue, true))
      break;
  }
  er.tl.y = cy + 1;

  // ... downwards.
  for (cy = er.br.y; ; cy++) {
    sr.setXYWH(er.tl.x, cy, er.width(), 1);
    if (cy >= r.br.y || !checkSolidTile(sr, &colorValue, true))
      break;
  }
  er.br.y = cy;

  // ... to the left.
  for (cx = er.tl.x - 1; ; cx--) {
    sr.setXYWH(cx, er.tl.y, 1, er.height());
    if (cx < r.tl.x || !checkSolidTile(sr, &colorValue, true))
      break;
  }
  er.tl.x = cx + 1;

  // ... to the right.
  for (cx = er.br.x; ; cx++) {
    sr.setXYWH(cx, er.tl.y, 1, er.height());
    if (cx >= r.br.x || !checkSolidTile(sr, &colorValue, true))
      break;
  }
  er.br.x = cx;
}


void writeSubrect(const Rect&, const PixelFormat&, bool forceSolid = false);

void sendRectSimple(const Rect &r, const PixelFormat& pf)
{
  unsigned int dx, dy, sw, sh;
  Rect sr;

  // Shortcuts to rectangle coordinates and dimensions.
  const int x = r.tl.x;
  const int y = r.tl.y;
  const unsigned int w = r.width();
  const unsigned int h = r.height();

  if (w > s_pconf->maxRectWidth || r.area() > s_pconf->maxRectSize) {

    // Compute max sub-rectangle size.
    const unsigned int subrectMaxWidth =
      (w > s_pconf->maxRectWidth) ? s_pconf->maxRectWidth : w;
    const unsigned int subrectMaxHeight =
      s_pconf->maxRectSize / subrectMaxWidth;

    for (dy = 0; dy < h; dy += subrectMaxHeight) {
      for (dx = 0; dx < w; dx += s_pconf->maxRectWidth) {
	      sw = (dx + s_pconf->maxRectWidth < w) ? s_pconf->maxRectWidth : w - dx;
        sh = (dy + subrectMaxHeight < h) ? subrectMaxHeight : h - dy;
        sr.setXYWH(x + dx, y + dy, sw, sh);
        writeSubrect(sr, pf);
      }
    }
  }
  else writeSubrect(r, pf);
}

bool writeRect(Rect& r, const PixelFormat& pf)
{
  // Shortcuts to rectangle coordinates and dimensions.
  int x = r.tl.x;
  int y = r.tl.y;
  unsigned int w = r.width();
  unsigned int h = r.height();

  // Copy members of current TightEncoder instance to static variables.
  s_pconf = &conf[compressLevel];
  if (qualityLevel >= 0) s_pjconf = &conf[qualityLevel];

  // Encode small rects as is.
  if (w * h < TIGHT_MIN_SPLIT_RECT_SIZE) {
    sendRectSimple(r, pf);
    return true;
  }

  // Split big rects into separately encoded subrects.
  Rect sr, bestr;
  unsigned int dx, dy, dw, dh;
  CARD32 colorValue;
  int maxRectSize = s_pconf->maxRectSize;
  int maxRectWidth = s_pconf->maxRectWidth;
  int nMaxWidth = (w > maxRectWidth) ? maxRectWidth : w;
  int nMaxRows = s_pconf->maxRectSize / nMaxWidth;

  // Try to find large solid-color areas and send them separately.
  for (dy = y; dy < y + h; dy += TIGHT_MAX_SPLIT_TILE_SIZE) {

    // If a rectangle becomes too large, send its upper part now.
    if (dy - y >= nMaxRows) {
      sr.setXYWH(x, y, w, nMaxRows);
      sendRectSimple(sr, pf);
      r.tl.y += nMaxRows;
      y = r.tl.y;
      h = r.height();
    }

    dh = (dy + TIGHT_MAX_SPLIT_TILE_SIZE <= y + h) ?
      TIGHT_MAX_SPLIT_TILE_SIZE : (y + h - dy);

    for (dx = x; dx < x + w; dx += TIGHT_MAX_SPLIT_TILE_SIZE) {

      dw = (dx + TIGHT_MAX_SPLIT_TILE_SIZE <= x + w) ?
        TIGHT_MAX_SPLIT_TILE_SIZE : (x + w - dx);
 
      sr.setXYWH(dx, dy, dw, dh);
      if (checkSolidTile(sr, &colorValue, false)) {

        // Get dimensions of solid-color area.
        sr.setXYWH(dx, dy, r.br.x - dx, r.br.y - dy);
        findBestSolidArea(sr, colorValue, bestr);

        // Make sure a solid rectangle is large enough
        // (or the whole rectangle is of the same color).
        if (bestr.area() != r.area()
          && bestr.area() < TIGHT_MIN_SOLID_SUBRECT_SIZE)
          continue;

        // Try to extend solid rectangle to maximum size.
        extendSolidArea(r, colorValue, bestr);
 
        // Send rectangles at top and left to solid-color area.
        if (bestr.tl.y != y) {
          sr.setXYWH(x, y, w, bestr.tl.y - y);
          sendRectSimple(sr, pf);
        }
        if (bestr.tl.x != x) {
          sr.setXYWH(x, bestr.tl.y, bestr.tl.x - x, bestr.height());
          writeRect(sr, pf);
        }

        // Send solid-color rectangle.
        writeSubrect(bestr, pf, true);

        // Send remaining rectangles (at right and bottom).
        if (bestr.br.x != r.br.x) {
          sr.setXYWH(bestr.br.x, bestr.tl.y, r.br.x - bestr.br.x,
            bestr.height());
          writeRect(sr, pf);
        }
        if (bestr.br.y != r.br.y) {
          sr.setXYWH(x, bestr.br.y, w, r.br.y - bestr.br.y);
          writeRect(sr, pf);
        }

        return true;
      }
    }
  }

  // No suitable solid-color rectangles found.
  sendRectSimple(r, pf);
  return true;
}

void writeSubrect(const Rect& r, const PixelFormat& pf,
  bool forceSolid)
{
  rdr::U8* imageBuf = getImageBuf(r.area(), pf);
  mos.clear();

    /* Send pending data if there is more than 128 bytes. */
  if(ublen > 128)
    if(!rfbSendUpdateBuf(cl)) throw Exception("rfbSendUpdateBuf() failed");

  switch (cl->format.bitsPerPixel) {
  case 8:
    tightEncode8(r, &mos, zos, imageBuf, pf, forceSolid);  break;
  case 16:
    tightEncode16(r, &mos, zos, imageBuf, pf, forceSolid); break;
  case 32:
    tightEncode32(r, &mos, zos, imageBuf, pf, forceSolid); break;
  }

  rfbFramebufferUpdateRectHeader rect;
  if(ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE)
    if(!rfbSendUpdateBuf(cl)) throw Exception("rfbSendUpdateBuf() failed");

  rect.r.x = Swap16IfLE(r.tl.x);
  rect.r.y = Swap16IfLE(r.tl.y);
  rect.r.w = Swap16IfLE(r.width());
  rect.r.h = Swap16IfLE(r.height());
  rect.encoding = Swap32IfLE(rfbEncodingTight);
  memcpy(&updateBuf[ublen], (char *)&rect, sz_rfbFramebufferUpdateRectHeader);
  ublen += sz_rfbFramebufferUpdateRectHeader;

  cl->rfbRectanglesSent[rfbEncodingTight]++;
  cl->rfbBytesSent[rfbEncodingTight] += sz_rfbFramebufferUpdateRectHeader;

  int portionLen = UPDATE_BUF_SIZE;
  int compressedLen = mos.length();
  char *buf = (char *)mos.data();
  for(int i = 0; i < compressedLen; i += portionLen) {
    if(i + portionLen > compressedLen) {
       portionLen = compressedLen - i;
    }
    if(ublen + portionLen > UPDATE_BUF_SIZE)
      if(!rfbSendUpdateBuf(cl)) throw Exception("rfbSendUpdateBuf() failed");
    memcpy(&updateBuf[ublen], &buf[i], portionLen);
    ublen += portionLen;
  }
  cl->rfbBytesSent[rfbEncodingTight] += compressedLen;
}

Bool rfbSendRectEncodingTight(rfbClientPtr _cl, int x, int y, int w, int h)
{
  try {
    cl=_cl;
    Rect r(x, y, x + w, y + h);
    PixelFormat pf(cl->format.bitsPerPixel, cl->format.depth,
      cl->format.bigEndian==1, cl->format.trueColour==1, cl->format.redMax,
      cl->format.greenMax, cl->format.blueMax, cl->format.redShift,
      cl->format.greenShift, cl->format.blueShift);

    int i;
    for(i=0; i<4; i++) {
      if(cl->zsActive[i]) break;
    }
    if(i==4 && zos) {delete [] zos;  zos=NULL;}
    if(!zos) zos = new ZlibOutStream[4];

    writeRect(r, pf);
  }
  catch(Exception e) {
    fprintf(stderr, "ERROR: %s\n", e.str());
    return FALSE;
  }
  return TRUE;
}
