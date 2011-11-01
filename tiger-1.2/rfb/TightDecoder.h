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

    void tightDecode8(const Rect& r, rdr::InStream* is,
                      rdr::U8* buf, const PixelFormat &pf);
    void tightDecode16(const Rect& r, rdr::InStream* is,
                       rdr::U16* buf, const PixelFormat &pf);
    void tightDecode32(const Rect& r, rdr::InStream* is,
                       rdr::U32* buf, const PixelFormat &pf);

  private:
    void DecompressJpegRect8(const Rect& r, rdr::InStream* is,
                             rdr::U8* buf, const PixelFormat &pf);
    void DecompressJpegRect16(const Rect& r, rdr::InStream* is,
                              rdr::U16* buf, const PixelFormat &pf);
    void DecompressJpegRect32(const Rect& r, rdr::InStream* is,
                              rdr::U32* buf, const PixelFormat &pf);

    void FilterGradient8(const Rect& r, rdr::InStream* is, int dataSize,
                         rdr::U8* buf, const PixelFormat &pf);
    void FilterGradient16(const Rect& r, rdr::InStream* is, int dataSize,
                          rdr::U16* buf, const PixelFormat &pf);
    void FilterGradient24(const Rect& r, rdr::InStream* is, int dataSize,
		                      rdr::U32* buf, const PixelFormat &pf);
    void FilterGradient32(const Rect& r, rdr::InStream* is, int dataSize,
                          rdr::U32* buf, const PixelFormat &pf);

    JpegDecompressor jd;
    rdr::ZlibInStream zis[4];
  };
}

#endif
