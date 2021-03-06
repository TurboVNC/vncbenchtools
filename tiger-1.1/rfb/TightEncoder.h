/* Copyright (C) 2000-2003 Constantin Kaplinsky.  All Rights Reserved.
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
#ifndef __RFB_TIGHTENCODER_H__
#define __RFB_TIGHTENCODER_H__

#include <rdr/MemOutStream.h>
#include <rdr/ZlibOutStream.h>

// FIXME: Check if specifying extern "C" is really necessary.
#include <stdio.h>
extern "C" {
#include <jpeglib.h>
}

namespace rfb {

  enum subsampEnum {
    SUBSAMP_NONE,
    SUBSAMP_422,
    SUBSAMP_420
  };

  struct TIGHT_CONF {
    unsigned int maxRectSize, maxRectWidth;
    unsigned int monoMinRectSize;
    int idxZlibLevel, monoZlibLevel, rawZlibLevel;
    int idxMaxColorsDivisor;
    int jpegQuality;
    subsampEnum jpegSubSample;
  };

}

#endif
