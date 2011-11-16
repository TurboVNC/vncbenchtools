/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
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
//
// TransImageGetter - class to perform translation between pixel formats,
// implementing the ImageGetter interface.
//

#ifndef __RFB_TRANSIMAGEGETTER_H__
#define __RFB_TRANSIMAGEGETTER_H__

#include <rfb/Rect.h>
#include "rfb.h"

extern rfbClientPtr cl;

namespace rfb {

  class TransImageGetter {
  public:

    TransImageGetter(void) {};
    ~TransImageGetter(void) {};

    void getImage(void* outPtr, const Rect& r, int outStride=0) {
      int inStride;
      const rdr::U8* inPtr = getRawPixelsRW(r, &inStride);
      if (!outStride) outStride = r.width();
      translateRect((void*)inPtr, inStride, Rect(0, 0, r.width(), r.height()),
                    outPtr, outStride, Point(0, 0));
    }

    rdr::U8 *getRawPixelsRW(const Rect &r, int *stride) {
      *stride = rfbScreen.paddedWidthInBytes / (rfbScreen.bitsPerPixel/8);
      return (rdr::U8 *)(rfbScreen.pfbMemory
        + (rfbScreen.paddedWidthInBytes * r.tl.y)
        + (r.tl.x * (rfbScreen.bitsPerPixel / 8)));
    }

    void translateRect(void* inPtr, int inStride, Rect inRect,
                       void* outPtr, int outStride, Point outCoord) {
      char *in, *out;

      in = (char*)inPtr;
      in += rfbServerFormat.bitsPerPixel/8 * inRect.tl.x;
      in += (inStride * rfbServerFormat.bitsPerPixel/8) * inRect.tl.y;

      out = (char*)outPtr;
      out += cl->format.bitsPerPixel/8 * outCoord.x;
      out += (outStride * cl->format.bitsPerPixel/8) * outCoord.y;

      (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,
        &cl->format, (char *)in, (char *)out,
        inStride * rfbServerFormat.bitsPerPixel/8, inRect.width(),
        inRect.height());
    }

    inline void translatePixels(void* inPtr, void* outPtr, int nPixels) {
      (*cl->translateFn)(cl->translateLookupTable, &rfbServerFormat,
        &cl->format, (char *)inPtr, (char *)outPtr, nPixels, nPixels, 1);
    }

    bool willTransform(void) {
      return cl->translateFn != rfbTranslateNone;
    }

  };
}
#endif
