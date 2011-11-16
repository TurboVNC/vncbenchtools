/* Copyright (C) 2000-2003 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright 2004-2005 Cendio AB.
 * Copyright (C) 2011 D. R. Commander
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

#include "tiger-1.1/rfb/TightDecoder.h"
#include <stdio.h> /* jpeglib.h needs FILE */
extern "C" {
#include <jpeglib.h>
}

using namespace rfb;

#if BPP == 8

#ifndef TIGER_BOTH
#include "tiger-1.1/rdr/Exception.cxx"
#include "tiger-1.1/rfb/PixelFormat.cxx"
#else
#include "tiger-1.1/rdr/Exception.h"
#include "tiger-1.1/rfb/PixelFormat.h"
#endif
#include "tiger-1.1/rfb/PixelBuffer.cxx"
#include "tiger-1.1/rdr/ZlibInStream.cxx"
#include "tiger-1.1/rdr/MemInStream.h"

#define TIGHT_MAX_WIDTH 2048

static void JpegSetSrcManager(j_decompress_ptr cinfo, char *compressedData,
			      int compressedLen);
static bool jpegError;

#define EXTRA_ARGS const PixelFormat &pf
#define FILL_RECT(r, p) pb->fillRect(r, p)
#define IMAGE_RECT(r, p) pb->imageRect(r, p)

static FullFramePixelBuffer *pb = NULL;

#endif

#include "tiger-1.1/rfb/tightDecode.h"

#if BPP == 8

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

TightDecoder::TightDecoder()
{
}

TightDecoder::~TightDecoder()
{
}

//
// A "Source manager" for the JPEG library.
//

static struct jpeg_source_mgr jpegSrcManager;
static JOCTET *jpegBufferPtr;
static size_t jpegBufferLen;

static void JpegInitSource(j_decompress_ptr cinfo);
static boolean JpegFillInputBuffer(j_decompress_ptr cinfo);
static void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes);
static void JpegTermSource(j_decompress_ptr cinfo);

static void
JpegInitSource(j_decompress_ptr cinfo)
{
  jpegError = false;
}

static boolean
JpegFillInputBuffer(j_decompress_ptr cinfo)
{
  jpegError = true;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;
  jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;

  return TRUE;
}

static void
JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes)
{
  if (num_bytes < 0 || (size_t)num_bytes > jpegSrcManager.bytes_in_buffer) {
    jpegError = true;
    jpegSrcManager.bytes_in_buffer = jpegBufferLen;
    jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;
  } else {
    jpegSrcManager.next_input_byte += (size_t) num_bytes;
    jpegSrcManager.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
JpegTermSource(j_decompress_ptr cinfo)
{
  /* No work necessary here. */
}

static void
JpegSetSrcManager(j_decompress_ptr cinfo, char *compressedData, int compressedLen)
{
  jpegBufferPtr = (JOCTET *)compressedData;
  jpegBufferLen = (size_t)compressedLen;

  jpegSrcManager.init_source = JpegInitSource;
  jpegSrcManager.fill_input_buffer = JpegFillInputBuffer;
  jpegSrcManager.skip_input_data = JpegSkipInputData;
  jpegSrcManager.resync_to_restart = jpeg_resync_to_restart;
  jpegSrcManager.term_source = JpegTermSource;
  jpegSrcManager.next_input_byte = jpegBufferPtr;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;

  cinfo->src = &jpegSrcManager;
}

static TightDecoder *td = NULL;
extern XImage *image;
static MemInStream *is = NULL;

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

    /* Uncompressed RGB24 JPEG data, before translated, can be up to 3
       times larger, if VNC bpp is 8. */
    rdr::U8* buf = getImageBuf(r.area()*3, pf);
    is->reposition(sbptr);
    TIGHT_DECODE (r, is, td->zis, (PIXEL_T *) buf, pf);
    sbptr = is->pos();
  }
  catch(Exception e) {
    fprintf(stderr, "ERROR: %s\n", e.str());
    return FALSE;
  }
  return TRUE;

}

#undef PIXEL_T
#undef TIGHT_DECODE
