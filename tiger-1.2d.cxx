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

#include <rfb/CMsgHandler.h>
#include <rfb/TightDecoder.h>

using namespace rfb;

#if BPP == 8

#include <rdr/Exception.h>
#include <rfb/PixelFormat.h>
#include <rfb/PixelBuffer.h>
#include <rdr/MemInStream.h>

#define TIGHT_MAX_WIDTH 2048

#define FILL_RECT(r, p) handler->fillRect(r, p)
#define IMAGE_RECT(r, p) handler->imageRect(r, p)

static rdr::U8* imageBuf = NULL;
static int imageBufSize = 0;

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

#endif

#include <rfb/tightDecode.h>

#if BPP == 8

TightDecoder::TightDecoder()
{
}

TightDecoder::~TightDecoder()
{
}

static rdr::MemInStream *mis = NULL;

class FrameBuffer : public CMsgHandler
{
  public:

    FrameBuffer(int w, int h, rdr::U8 *data)
    {
      PixelFormat cpf(myFormat.bitsPerPixel, myFormat.depth,
        myFormat.bigEndian==1, myFormat.trueColour==1, myFormat.redMax,
        myFormat.greenMax, myFormat.blueMax, myFormat.redShift,
        myFormat.greenShift, myFormat.blueShift);
      pf = cpf;
      pb = new FullFramePixelBuffer(pf, w, h, data, NULL);
    }

    ~FrameBuffer()
    {
      delete pb;
    }

    void fillRect(const Rect& r, Pixel pix)
    {
      pb->fillRect(r, pix);
    }

    void imageRect(const Rect& r, void* pixels)
    {
      pb->imageRect(r, pixels);
    }

    rdr::U8* getRawPixelsRW(const Rect& r, int* stride)
    {
      return pb->getPixelsRW(r, stride);
    }

    void releaseRawPixels(const Rect& r) {}

    const PixelFormat &getPreferredPF(void) { return pf; }

  private:

    FullFramePixelBuffer *pb;
    PixelFormat pf;
};

void TightDecoder::readRect(const Rect& r, CMsgHandler* handler)
{
  is = mis;
  this->handler = handler;
  clientpf = handler->getPreferredPF();
  PixelFormat spf(rfbServerFormat.bitsPerPixel, rfbServerFormat.depth,
    rfbServerFormat.bigEndian==1, rfbServerFormat.trueColour==1,
    rfbServerFormat.redMax, rfbServerFormat.greenMax,
    rfbServerFormat.blueMax, rfbServerFormat.redShift,
    rfbServerFormat.greenShift, rfbServerFormat.blueShift);
  serverpf = spf;

  if (clientpf.equal(serverpf)) {
    /* Decode directly into the framebuffer (fast path) */
    directDecode = true;
  } else {
    /* Decode into an intermediate buffer and use pixel translation */
    directDecode = false;
  }

  switch (serverpf.bpp) {
  case 8:
    tightDecode8 (r); break;
  case 16:
    tightDecode16(r); break;
  case 32:
    tightDecode32(r); break;
  }
}

static TightDecoder *td = NULL;
static FrameBuffer *fb = NULL;
extern XImage *image;

#endif

#define HandleTightBPP CONCAT2E(HandleTight,BPP)

static Bool HandleTightBPP(int rx, int ry, int rw, int rh)
{
  try {

    Rect r(rx, ry, rx + rw, ry + rh);
    int i;
    for (i = 0; i < 4; i++) {
      if (zlibStreamActive[i]) break;
    }
    if (i == 4) {
      if (td) { delete td;  td = NULL; }
      if (fb) { delete fb;  fb = NULL; }
      if (mis) { delete mis; mis = NULL; }
    }
    if (!td) td = new TightDecoder;
    if (!fb) fb = new FrameBuffer(image->width, image->height,
      (rdr::U8 *)image->data);
    if (!mis) {
      mis = new rdr::MemInStream(sendBuf, SEND_BUF_SIZE);
    }

    mis->reposition(sbptr);
    td->readRect(r, fb);
    sbptr = mis->pos();
  }
  catch(Exception e) {
    fprintf(stderr, "ERROR: %s\n", e.str());
    return FALSE;
  }
  return TRUE;

}

#undef HandleTightBPP
