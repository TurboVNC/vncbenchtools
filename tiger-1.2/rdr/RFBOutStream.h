/* Copyright (C) 2014 D. R. Commander.  All Rights Reserved.
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
// OutStream wrapper for the buffering code used by the TurboVNC benchmark
//

#ifndef __RDR_RFBOUTSTREAM_H__
#define __RDR_RFBOUTSTREAM_H__

#include <rdr/OutStream.h>
#include <rdr/Exception.h>
#include "rfb.h"

extern rfbClientPtr cl;

namespace rdr {

  class RFBOutStream : public OutStream {

  public:

    RFBOutStream() {
      ptr = (rdr::U8 *)&updateBuf[ublen];
      end = (rdr::U8 *)&updateBuf[UPDATE_BUF_SIZE];
    }

    virtual ~RFBOutStream() {
    }

    void writeBytes(const void* data, int length) {
      ublen = ptr - (rdr::U8 *)updateBuf;

      int portionLen = UPDATE_BUF_SIZE;
      int compressedLen = length;
      char *buf = (char *)data;
      for(int i = 0; i < compressedLen; i += portionLen) {
        if(i + portionLen > compressedLen) {
          portionLen = compressedLen - i;
        }
        if(ublen + portionLen > UPDATE_BUF_SIZE)
          if(!rfbSendUpdateBuf(cl))
            throw Exception("rfbSendUpdateBuf() failed");
        memcpy(&updateBuf[ublen], &buf[i], portionLen);
        ublen += portionLen;
      }

      ptr = (rdr::U8 *)&updateBuf[ublen];
    }

    int length() { return ublen; }

  protected:

    int overrun(int itemSize, int nItems) {
      ublen = ptr - (rdr::U8 *)updateBuf;

      if(ublen + itemSize * nItems > UPDATE_BUF_SIZE)
        if(!rfbSendUpdateBuf(cl))
          throw Exception("rfbSendUpdateBuf() failed");

      if(ublen + itemSize * nItems > UPDATE_BUF_SIZE)
        throw Exception("Not enough room in buffer");

      ptr = (rdr::U8 *)&updateBuf[ublen];

      return nItems;
    }

  };

}

#endif
