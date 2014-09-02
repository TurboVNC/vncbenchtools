/* Copyright (C) 2000-2003 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright 2004-2005 Cendio AB.
 * Copyright (C) 2011, 2014 D. R. Commander.  All Rights Reserved.
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

#include <rfb/TightDecoder.h>

using namespace rfb;

#if BPP == 8

#include <rdr/Exception.h>
#include <rfb/PixelFormat.h>
#include <rfb/PixelBuffer.h>
#include <rdr/MemInStream.h>

static rdr::U8* imageBuf = NULL;
static int imageBufSize = 0;

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

static rdr::MemInStream *mis = NULL;
static TightDecoder *td = NULL;
static FullFramePixelBuffer *fb = NULL;
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

    PixelFormat clientPF(myFormat.bitsPerPixel, myFormat.depth,
      myFormat.bigEndian==1, myFormat.trueColour==1, myFormat.redMax,
      myFormat.greenMax, myFormat.blueMax, myFormat.redShift,
      myFormat.greenShift, myFormat.blueShift);
    if (!fb) fb = new FullFramePixelBuffer(clientPF, image->width,
      image->height, (rdr::U8 *)image->data, image->bytes_per_line);
    if (!mis) {
      mis = new rdr::MemInStream(sendBuf, SEND_BUF_SIZE);
    }

    mis->reposition(sbptr);
    td->readRect(r, fb, mis);
    sbptr = mis->pos();
  }
  catch(rdr::Exception e) {
    fprintf(stderr, "ERROR: %s\n", e.str());
    return FALSE;
  }
  return TRUE;

}

#undef HandleTightBPP
