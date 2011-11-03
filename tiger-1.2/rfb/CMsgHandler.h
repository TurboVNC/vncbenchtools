/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2009 Pierre Ossman for Cendio AB
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
// CMsgHandler - class to handle incoming messages on the client side.
//

#ifndef __RFB_CMSGHANDLER_H__
#define __RFB_CMSGHANDLER_H__

#include "../rdr/types.h"
#include "Pixel.h"
#include "PixelFormat.h"
#include "Rect.h"

namespace rdr { class InStream; }

namespace rfb {

  class CMsgHandler {
  public:
    CMsgHandler() {}
    virtual ~CMsgHandler() {}

    virtual void fillRect(const Rect& r, Pixel pix) = 0;
    virtual void imageRect(const Rect& r, void* pixels) = 0;

    virtual rdr::U8* getRawPixelsRW(const Rect& r, int* stride) = 0;
    virtual void releaseRawPixels(const Rect& r) = 0;

    virtual const PixelFormat &getPreferredPF(void) = 0;
  };
}
#endif
