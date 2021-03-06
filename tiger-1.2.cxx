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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#include <rdr/Exception.h>
#include <rfb/ComparingUpdateTracker.h>
#include <rdr/RFBOutStream.h>
#include <rfb/TightEncoder.h>
#include "rfb.h"

static int compressLevel = 1;
static int qualityLevel = 8;

using namespace rfb;

rfbClientPtr cl = NULL;
rdr::RFBOutStream rfbos;
static rdr::U8* imageBuf = NULL;
static int imageBufSize = 0;

unsigned long solidrect=0, solidpixels=0, monorect=0, monopixels=0, ndxrect=0,
  ndxpixels=0, jpegrect=0, jpegpixels=0, fcrect=0, fcpixels=0, gradrect=0,
  gradpixels=0;

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

static TightEncoder *te = NULL;
static TransImageGetter image_getter;

PixelFormat clientPF;

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

    PixelFormat serverPF(rfbServerFormat.bitsPerPixel, rfbServerFormat.depth,
      rfbServerFormat.bigEndian==1, rfbServerFormat.trueColour==1,
      rfbServerFormat.redMax, rfbServerFormat.greenMax,
      rfbServerFormat.blueMax, rfbServerFormat.redShift,
      rfbServerFormat.greenShift, rfbServerFormat.blueShift);

    PixelFormat cpf(cl->format.bitsPerPixel, cl->format.depth,
      cl->format.bigEndian==1, cl->format.trueColour==1, cl->format.redMax,
      cl->format.greenMax, cl->format.blueMax, cl->format.redShift,
      cl->format.greenShift, cl->format.blueShift);
    clientPF = cpf;

    if (!fb) fb = new FullFramePixelBuffer(serverPF, rfbScreen.width,
      rfbScreen.height, (rdr::U8 *)rfbScreen.pfbMemory, NULL);

    image_getter.init(fb, clientPF, NULL);

    rfbos.setptr((rdr::U8 *)&updateBuf[ublen]);

    if (compareFB) {
      if (!cut) cut = new ComparingUpdateTracker(fb);
      rfb::Region changed;
      cut->compareRect(r, &changed);
      std::vector<Rect> rects;
      std::vector<Rect>::const_iterator i;
      if(changed.get_rects(&rects)) {
        for (i = rects.begin(); i < rects.end(); i++)
          te->writeRect(*i, &image_getter, NULL);
      }
    }
    else te->writeRect(r, &image_getter, NULL);

    ublen = rfbos.getptr() - (rdr::U8 *)updateBuf;
  }
  catch (rdr::Exception e) {
    fprintf(stderr, "ERROR: %s\n", e.str());
    return FALSE;
  }
  return TRUE;
}
