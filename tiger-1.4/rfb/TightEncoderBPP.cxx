/* Copyright (C) 2000-2003 Constantin Kaplinsky.  All Rights Reserved.
 * Copyright (C) 2011, 2014 D. R. Commander.  All Rights Reserved.
 * Copyright 2014 Pierre Ossman for Cendio AB
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

#define CONCAT2(a,b) a##b
#define CONCAT2E(a,b) CONCAT2(a,b)

#define UBPP CONCAT2E(U,BPP)

void TightEncoder::writeMonoRect(int width, int height,
                                 const rdr::UBPP* buffer, int stride,
                                 const PixelFormat& pf,
                                 const Palette& palette)
{
  monorect++;  monopixels += width * height;

  rdr::OutStream* os;

  const int streamId = 1;
  rdr::UBPP pal[2];

  int length;
  rdr::OutStream* zos;

  assert(palette.size() == 2);

  os = &rfbos;

  os->writeU8((streamId | tightExplicitFilter) << 4);
  os->writeU8(tightFilterPalette);
  cl->rfbBytesSent[encodingTight] += 2;

  // Write the palette
  pal[0] = (rdr::UBPP)palette.getColour(0);
  pal[1] = (rdr::UBPP)palette.getColour(1);

  os->writeU8(1);
  cl->rfbBytesSent[encodingTight]++;
  writePixels((rdr::U8*)pal, pf, 2, os);

  // Set up compression
  length = (width + 7)/8 * height;
  zos = getZlibOutStream(streamId, monoZlibLevel, length);

  // Encode the data
  rdr::UBPP bg;
  unsigned int value, mask;
  int pad, aligned_width;
  int x, y, bg_bits;

  bg = pal[0];
  aligned_width = width - width % 8;
  pad = stride - width;

  for (y = 0; y < height; y++) {
    for (x = 0; x < aligned_width; x += 8) {
      for (bg_bits = 0; bg_bits < 8; bg_bits++) {
        if (*buffer++ != bg)
          break;
      }
      if (bg_bits == 8) {
        zos->writeU8(0);
        if (zos == &rfbos) cl->rfbBytesSent[encodingTight]++;
        continue;
      }
      mask = 0x80 >> bg_bits;
      value = mask;
      for (bg_bits++; bg_bits < 8; bg_bits++) {
        mask >>= 1;
        if (*buffer++ != bg) {
          value |= mask;
        }
      }
      zos->writeU8(value);
      if (zos == &rfbos) cl->rfbBytesSent[encodingTight]++;
    }

    if (x < width) {
      mask = 0x80;
      value = 0;

      for (; x < width; x++) {
        if (*buffer++ != bg) {
          value |= mask;
        }
        mask >>= 1;
      }
      zos->writeU8(value);
      if (zos == &rfbos) cl->rfbBytesSent[encodingTight]++;
    }

    buffer += pad;
  }

  // Finish the zlib stream
  flushZlibOutStream(zos);
}

#if (BPP != 8)
void TightEncoder::writeIndexedRect(int width, int height,
                                    const rdr::UBPP* buffer, int stride,
                                    const PixelFormat& pf,
                                    const Palette& palette)
{
  ndxrect++;  ndxpixels += width * height;

  rdr::OutStream* os;

  const int streamId = 2;
  rdr::UBPP pal[256];

  rdr::OutStream* zos;

  int pad;
  rdr::UBPP prevColour;
  unsigned char idx;

  assert(palette.size() > 0);
  assert(palette.size() <= 256);

  os = &rfbos;

  os->writeU8((streamId | tightExplicitFilter) << 4);
  os->writeU8(tightFilterPalette);
  cl->rfbBytesSent[encodingTight] += 2;

  // Write the palette
  for (int i = 0; i < palette.size(); i++)
    pal[i] = (rdr::UBPP)palette.getColour(i);

  os->writeU8(palette.size() - 1);
  cl->rfbBytesSent[encodingTight]++;
  writePixels((rdr::U8*)pal, pf, palette.size(), os);

  // Set up compression
  zos = getZlibOutStream(streamId, idxZlibLevel, width * height);

  // Encode the data
  pad = stride - width;

  prevColour = *buffer;
  idx = palette.lookup(*buffer);

  while (height--) {
    int w = width;
    while (w--) {
      if (*buffer != prevColour) {
        prevColour = *buffer;
        idx = palette.lookup(*buffer);
      }
      zos->writeU8(idx);
      if (zos == &rfbos) cl->rfbBytesSent[encodingTight]++;
      buffer++;
    }
    buffer += pad;
  }

  // Finish the zlib stream
  flushZlibOutStream(zos);
}
#endif  // #if (BPP != 8)
