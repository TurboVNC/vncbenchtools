/* Copyright (C) 2011, 2014 D. R. Commander.  All Rights Reserved.
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
#include <rdr/RFBOutStream.h>
#include <rfb/ComparingUpdateTracker.h>
#include <rfb/EncodeManager.h>
#include "rfb.h"

int compressLevel = 1;
int qualityLevel = 8;
int fineQualityLevel = -1;
int subsampling = -1;

using namespace rfb;

rfbClientPtr cl = NULL;
rdr::RFBOutStream rfbos;

unsigned long solidrect=0, solidpixels=0, monorect=0, monopixels=0, ndxrect=0,
  ndxpixels=0, jpegrect=0, jpegpixels=0, fcrect=0, fcpixels=0, gradrect=0,
  gradpixels=0;

Bool compareFB = FALSE;

static EncodeManager *em = NULL;
static ComparingUpdateTracker *cut = NULL;
static FullFramePixelBuffer *fb = NULL;

PixelFormat clientPF;

Bool rfbSendRectEncodingTight(rfbClientPtr _cl, int x, int y, int w, int h)
{
  try {
    cl = _cl;
    Rect r(x, y, x + w, y + h);

    if (cl->reset) {
      if (em) { delete em;  em = NULL; }
      if (fb) { delete fb;  fb = NULL; }
      if (cut) { delete cut;  cut = NULL; }
      cl->reset = FALSE;
    }
    if (!em) em = new EncodeManager;

    UpdateInfo ui;

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
      rfbScreen.height, (rdr::U8 *)rfbScreen.pfbMemory,
      rfbScreen.paddedWidthInBytes * 8 / rfbScreen.bitsPerPixel);

    int stride;

    if (compareFB) {
      if (!cut) cut = new ComparingUpdateTracker(fb);
      cut->compareRect(r, &ui.changed);
    } else
      ui.changed.reset(r);

    rfbos.setptr((rdr::U8 *)&updateBuf[ublen]);

    em->writeUpdate(ui, fb);

    ublen = rfbos.getptr() - (rdr::U8 *)updateBuf;
  }
  catch (rdr::Exception e) {
    fprintf(stderr, "ERROR: %s\n", e.str());
    return FALSE;
  }
  return TRUE;
}
