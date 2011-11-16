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
#include <rfb/encodings.h>
#include <rfb/TightEncoder.h>
#include <rdr/ZlibOutStream.h>
#include <rfb/PixelFormat.h>
#include <rfb/Rect.h>
#include <rdr/Exception.h>
#include "rfb.h"

using namespace rfb;

// Minimum amount of data to be compressed. This value should not be
// changed, doing so will break compatibility with existing clients.
#define TIGHT_MIN_TO_COMPRESS 12

// Adjustable parameters.
// FIXME: Get rid of #defines
#define TIGHT_JPEG_MIN_RECT_SIZE 1024
#define TIGHT_DETECT_MIN_WIDTH      8
#define TIGHT_DETECT_MIN_HEIGHT     8

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
  { 65536, 2048,  32, 9, 9, 9,  96,100, SUBSAMP_NONE }  // 9
};

static const int compressLevel = 6;
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
#include <rfb/tightEncode.h>
#undef BPP
#define BPP 16
#include <rfb/tightEncode.h>
#undef BPP
#define BPP 32
#include <rfb/tightEncode.h>
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

void writeSubrect(const Rect&, const PixelFormat&);

bool writeRect(const Rect& r, const PixelFormat& pf)
{
  // Shortcuts to rectangle coordinates and dimensions.
  const int x = r.tl.x;
  const int y = r.tl.y;
  const unsigned int w = r.width();
  const unsigned int h = r.height();
  const TIGHT_CONF* pconf;

  // Copy members of current TightEncoder instance to static variables.
  s_pconf = pconf = &conf[compressLevel];
  if (qualityLevel >= 0) s_pjconf = &conf[qualityLevel];

  // Encode small rects as is.
  bool rectTooBig = w > pconf->maxRectWidth || w * h > pconf->maxRectSize;
  if (!rectTooBig) {
    writeSubrect(r, pf);
    return true;
  }

  // Compute max sub-rectangle size.
  const unsigned int subrectMaxWidth =
    (w > pconf->maxRectWidth) ? pconf->maxRectWidth : w;
  const unsigned int subrectMaxHeight =
    pconf->maxRectSize / subrectMaxWidth;

  // Split big rects into separately encoded subrects.
  Rect sr;
  unsigned int dx, dy, sw, sh;
  for (dy = 0; dy < h; dy += subrectMaxHeight) {
    for (dx = 0; dx < w; dx += pconf->maxRectWidth) {
      sw = (dx + pconf->maxRectWidth < w) ? pconf->maxRectWidth : w - dx;
      sh = (dy + subrectMaxHeight < h) ? subrectMaxHeight : h - dy;
      sr.setXYWH(x + dx, y + dy, sw, sh);
      writeSubrect(sr, pf);
    }
  }
  return true;
}

void writeSubrect(const Rect& r, const PixelFormat& pf)
{
  rdr::U8* imageBuf = getImageBuf(r.area(), pf);
  mos.clear();

    /* Send pending data if there is more than 128 bytes. */
  if(ublen > 128)
    if(!rfbSendUpdateBuf(cl)) throw rdr::Exception("rfbSendUpdateBuf() failed");

  switch (cl->format.bitsPerPixel) {
  case 8:
    tightEncode8(r, &mos, zos, imageBuf, pf);  break;
  case 16:
    tightEncode16(r, &mos, zos, imageBuf, pf); break;
  case 32:
    tightEncode32(r, &mos, zos, imageBuf, pf); break;
  }

  rfbFramebufferUpdateRectHeader rect;
  if(ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE)
    if(!rfbSendUpdateBuf(cl)) throw rdr::Exception("rfbSendUpdateBuf() failed");

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
      if(!rfbSendUpdateBuf(cl)) throw rdr::Exception("rfbSendUpdateBuf() failed");
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
    if(!zos) zos = new rdr::ZlibOutStream[4];

    writeRect(r, pf);
  }
  catch(rdr::Exception e) {
    fprintf(stderr, "ERROR: %s\n", e.str());
    return FALSE;
  }
  return TRUE;
}
