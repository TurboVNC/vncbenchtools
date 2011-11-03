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
#ifndef __RFB_TIGHTDECODER_H__
#define __RFB_TIGHTDECODER_H__

#include "../rdr/ZlibInStream.h"
#include "JpegDecompressor.h"
#include "Rect.h"
#include "PixelFormat.h"

namespace rfb {

  class TightDecoder {

  public:
    TightDecoder();
    void readRect(const Rect& r, const PixelFormat &pf);
    virtual ~TightDecoder();

  private:
    void tightDecode8(rdr::InStream* is, const Rect& r);
    void tightDecode16(rdr::InStream* is, const Rect& r);
    void tightDecode32(rdr::InStream* is, const Rect& r);

    void DecompressJpegRect8(rdr::InStream* is, rdr::U8* buf, int stride,
                             const Rect& r);
    void DecompressJpegRect16(rdr::InStream* is, rdr::U16* buf, int stride,
                             const Rect& r);
    void DecompressJpegRect32(rdr::InStream* is, rdr::U32* buf, int stride,
                             const Rect& r);

    void FilterGradient8(rdr::InStream* is, rdr::U8* buf, int stride, 
                         const Rect& r, int dataSize);
    void FilterGradient16(rdr::InStream* is, rdr::U16* buf, int stride, 
                          const Rect& r, int dataSize);
    void FilterGradient24(rdr::InStream* is, rdr::U32* buf, int stride, 
                          const Rect& r, int dataSize);
    void FilterGradient32(rdr::InStream* is, rdr::U32* buf, int stride, 
                          const Rect& r, int dataSize);

    void fillRect8(rdr::U8 *buf, int stride, const Rect& r, Pixel pix);
    void fillRect16(rdr::U16 *buf, int stride, const Rect& r, Pixel pix);
    void fillRect32(rdr::U32 *buf, int stride, const Rect& r, Pixel pix);

    rdr::ZlibInStream zis[4];
    JpegDecompressor jd;
    PixelFormat pf;
  };
}

#endif
