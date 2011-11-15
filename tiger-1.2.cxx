/* Copyright (C) 2011 D. R. Commander.  All Rights Reserved.
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

extern "C" {
  #include "rfb.h"
}

static int compressLevel = 1;
static int qualityLevel = 8;

#include "tiger-1.2/rfb/PixelFormat.cxx"

static rfbClientPtr cl = NULL;
static rdr::U8* imageBuf = NULL;
static int imageBufSize = 0;

unsigned long solidrect=0, solidpixels=0, monorect=0, monopixels=0, ndxrect=0,
  ndxpixels=0, jpegrect=0, jpegpixels=0, fcrect=0, fcpixels=0, gradrect=0,
  gradpixels=0;

static rdr::U8* getImageBuf(int required, const PixelFormat& pf)
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

#include "tiger-1.2/rdr/Exception.cxx"
#include "tiger-1.2/rdr/ZlibOutStream.cxx"
#include "tiger-1.2/rfb/JpegCompressor.cxx"
#include "tiger-1.2/rfb/TightEncoder.cxx"
#include "tiger-1.2/rfb/ComparingUpdateTracker.cxx"

static TightEncoder *te = NULL;
static TransImageGetter image_getter;

Bool compareFB = FALSE;

static ComparingUpdateTracker *cut = NULL;
static FullFramePixelBuffer *fb = NULL;

Bool rfbSendRectEncodingTight(rfbClientPtr _cl, int x, int y, int w, int h)
{
  try {
    cl = _cl;
    Rect r(x, y, x + w, y + h);

    if (cl->reset) {
      if (te) { delete te;  te = NULL; }
      if (fb) { delete fb;  fb = NULL; }
      if (cut) { delete cut;  cut = NULL; }
      cl->reset = FALSE;
    }
    if (!te) te = new TightEncoder;

    te->setCompressLevel(compressLevel);
    te->setQualityLevel(qualityLevel);
    if (compareFB) {
      PixelFormat spf(rfbServerFormat.bitsPerPixel, rfbServerFormat.depth,
        rfbServerFormat.bigEndian==1, rfbServerFormat.trueColour==1,
        rfbServerFormat.redMax, rfbServerFormat.greenMax,
        rfbServerFormat.blueMax, rfbServerFormat.redShift,
        rfbServerFormat.greenShift, rfbServerFormat.blueShift);
      if (!fb) fb = new FullFramePixelBuffer(spf, rfbScreen.width,
        rfbScreen.height, (rdr::U8 *)rfbScreen.pfbMemory, NULL);
      if (!cut) cut = new ComparingUpdateTracker(fb);
      if (cut->compareRect(r)) te->writeRect(r, &image_getter, NULL);
    }
    else te->writeRect(r, &image_getter, NULL);
  }
  catch (Exception e) {
    fprintf(stderr, "ERROR: %s\n", e.str());
    return FALSE;
  }
  return TRUE;
}
