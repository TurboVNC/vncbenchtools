/* Copyright (C) 2000-2003 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright 2004-2005 Cendio AB.
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

#include "tiger-1.2/rfb/TightDecoder.h"

using namespace rfb;

#if BPP == 8

#ifndef TIGER_BOTH
#include "tiger-1.2/rdr/Exception.cxx"
#include "tiger-1.2/rfb/PixelFormat.cxx"
#else
#include "tiger-1.2/rdr/Exception.h"
#include "tiger-1.2/rfb/PixelFormat.h"
#endif
#include "tiger-1.2/rfb/PixelBuffer.cxx"
#include "tiger-1.2/rdr/ZlibInStream.cxx"
#include "tiger-1.2/rdr/MemInStream.h"

#define TIGHT_MAX_WIDTH 2048

static FullFramePixelBuffer *pb = NULL;

#include "tiger-1.2/rfb/JpegDecompressor.cxx"	

#endif

#include "tiger-1.2/rfb/tightDecode.h"

#if BPP == 8

TightDecoder::TightDecoder()
{
}

TightDecoder::~TightDecoder()
{
}

static MemInStream *is = NULL;

void TightDecoder::readRect(const Rect& r, const PixelFormat &pf)
{
  this->pf = pf;
  switch (pf.bpp) {
  case 8:
    tightDecode8 (is, r); break;
  case 16:
    tightDecode16(is, r); break;
  case 32:
    tightDecode32(is, r); break;
  }
}

static TightDecoder *td = NULL;
extern XImage *image;

#endif

#define HandleTightBPP CONCAT2E(HandleTight,BPP)

static Bool HandleTightBPP(int rx, int ry, int rw, int rh)
{
  try {

    PixelFormat pf(myFormat.bitsPerPixel, myFormat.depth,
      myFormat.bigEndian==1, myFormat.trueColour==1, myFormat.redMax,
      myFormat.greenMax, myFormat.blueMax, myFormat.redShift,
      myFormat.greenShift, myFormat.blueShift);

    Rect r(rx, ry, rx + rw, ry + rh);
    int i;
    for (i = 0; i < 4; i++) {
      if (zlibStreamActive[i]) break;
    }
    if (i == 4) {
      if (td) { delete td;  td = NULL; }
      if (pb) { delete pb;  pb = NULL; }
      if (is) { delete is;  is = NULL; }
    }
    if (!td) td = new TightDecoder;
    if (!pb) pb = new FullFramePixelBuffer(pf, image->width, image->height,
      (rdr::U8 *)image->data, NULL);
    if (!is) {
      is = new MemInStream(sendBuf, SEND_BUF_SIZE);
    }

    is->reposition(sbptr);
    td->readRect(r, pf);
    sbptr = is->pos();
  }
  catch(Exception e) {
    fprintf(stderr, "ERROR: %s\n", e.str());
    return FALSE;
  }
  return TRUE;

}

#undef HandleTightBPP
