/*  Copyright (C) 2000 Const Kaplinsky <const@ce.cctpu.edu.ru>
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2012, 2014 D. R. Commander
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 */

/* misc functions */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "rfb.h"

extern Bool rfbSetTranslateFunction(rfbClientPtr cl);
extern FILE *out;

char updateBuf[UPDATE_BUF_SIZE], *sendBuf=NULL;
int ublen, sblen=0, sbptr=0;

rfbClientRec rfbClient;
rfbScreenInfo rfbScreen;
rfbPixelFormat rfbServerFormat;

XImage _image, *image=&_image;

#ifdef ICE_SUPPORTED
Bool InterframeOn(rfbClientPtr cl)
{
  if (!cl->compareFB) {
    if (!(cl->compareFB = (char *)malloc(rfbScreen.paddedWidthInBytes *
                                         rfbScreen.height))) {
      rfbLogPerror("InterframeOn: couldn't allocate comparison buffer");
      return FALSE;
    }
    memset(cl->compareFB, 0, rfbScreen.paddedWidthInBytes * rfbScreen.height);
    cl->firstCompare = TRUE;
    rfbLog("Interframe comparison enabled\n");
  }
  cl->fb = cl->compareFB;
  return TRUE;
}
#endif

void InitEverything (int color_depth)
{
  memset(&rfbClient, 0, sizeof(rfbClient));

  rfbClient.reset = TRUE;

  rfbClient.format.depth = color_depth;
  rfbClient.format.bitsPerPixel = color_depth;
  if (color_depth == 24)
    rfbClient.format.bitsPerPixel = 32;

  rfbServerFormat.depth = color_depth;
  rfbServerFormat.bitsPerPixel = rfbClient.format.bitsPerPixel;
  #ifdef _BIG_ENDIAN
  rfbServerFormat.bigEndian = 1;
  #else
  rfbServerFormat.bigEndian = 0;
  #endif
  rfbServerFormat.trueColour = 1;
  rfbScreen.bitsPerPixel = rfbClient.format.bitsPerPixel;

  switch (color_depth) {
  case 8:
    rfbClient.format.redMax = 0x07;
    rfbClient.format.greenMax = 0x07;
    rfbClient.format.blueMax = 0x03;
    rfbClient.format.redShift = 0;
    rfbClient.format.greenShift = 3;
    rfbClient.format.blueShift = 6;
    break;
  case 16:
    rfbClient.format.redMax = 0x1F;
    rfbClient.format.greenMax = 0x3F;
    rfbClient.format.blueMax = 0x1F;
    rfbClient.format.redShift = 11;
    rfbClient.format.greenShift = 5;
    rfbClient.format.blueShift = 0;
    break;
  default:                      /* 24 */
    rfbClient.format.redMax = 0xFF;
    rfbClient.format.greenMax = 0xFF;
    rfbClient.format.blueMax = 0xFF;
    rfbClient.format.redShift = 16;
    rfbClient.format.greenShift = 8;
    rfbClient.format.blueShift = 0;
  }
  rfbClient.format.bigEndian = 0;
  rfbClient.format.trueColour = 1;

  rfbServerFormat.redMax = rfbClient.format.redMax;
  rfbServerFormat.greenMax = rfbClient.format.greenMax;
  rfbServerFormat.blueMax = rfbClient.format.blueMax;
  rfbServerFormat.redShift = rfbClient.format.redShift;
  rfbServerFormat.greenShift = rfbClient.format.greenShift;
  rfbServerFormat.blueShift = rfbClient.format.blueShift;
  if (rfbServerFormat.bigEndian) {
     rfbServerFormat.redShift = rfbServerFormat.bitsPerPixel - 8 -
                                (rfbServerFormat.redShift & (~7)) +
                                (rfbServerFormat.redShift & 7);
     rfbServerFormat.greenShift = rfbServerFormat.bitsPerPixel - 8 -
                                  (rfbServerFormat.greenShift & (~7)) +
                                  (rfbServerFormat.greenShift & 7);
     rfbServerFormat.blueShift = rfbServerFormat.bitsPerPixel - 8 -
                                 (rfbServerFormat.blueShift & (~7)) +
                                 (rfbServerFormat.blueShift & 7);
  }

  if (out) {
    // Assume the benchmark will be played back on a 24-bpp display
    rfbClient.format.depth = 24;
    rfbClient.format.bitsPerPixel = 32;
    rfbClient.format.redMax = 0xFF;
    rfbClient.format.greenMax = 0xFF;
    rfbClient.format.blueMax = 0xFF;
    rfbClient.format.redShift = 16;
    rfbClient.format.greenShift = 8;
    rfbClient.format.blueShift = 0;
  }

  rfbSetTranslateFunction(&rfbClient);

  sendBuf = (char *)malloc(SEND_BUF_SIZE);
  if (!sendBuf) {
    printf("ERROR: Could not allocate send buffer.\n");
    exit(1);
  }

  ublen = 0;

  image->width = 1280;
  image->height = 1024;
  image->bits_per_pixel = rfbServerFormat.bitsPerPixel;
  image->bytes_per_line = ((image->width * image->bits_per_pixel / 8) + 3) & (~3);
  image->data = (char *)malloc(image->width * image->bytes_per_line);

  rfbScreen.width = image->width;
  rfbScreen.height = image->height;
  rfbScreen.paddedWidthInBytes = image->bytes_per_line;
  rfbScreen.sizeInBytes = image->height * image->bytes_per_line;

  if (out) {
    rfbServerInitMsg si;
    char *name = "TurboVNC Benchmark";
    si.framebufferWidth = Swap16IfLE(image->width);
    si.framebufferHeight = Swap16IfLE(image->height);
    memcpy(&si.format, &rfbServerFormat, sizeof(rfbPixelFormat));
    si.format.redMax = Swap16IfLE(si.format.redMax);
    si.format.greenMax = Swap16IfLE(si.format.greenMax);
    si.format.blueMax = Swap16IfLE(si.format.blueMax);
    si.nameLength = Swap32IfLE(strlen(name));
    if (!WriteToSessionCapture((char *)&si, sz_rfbServerInitMsg)) exit(1);
    if (!WriteToSessionCapture(name, strlen(name))) exit(1);
  }
}

extern int decompress;

BOOL rfbSendUpdateBuf(rfbClientPtr cl)
{
  if(decompress) {
    if (sblen + ublen > SEND_BUF_SIZE) {
      printf("ERROR: Send buffer overrun.\n");
      return False;
    }
    memcpy(&sendBuf[sblen], updateBuf, ublen);
    sblen += ublen;
  }
  if (!WriteToSessionCapture(updateBuf, ublen)) return False;
  ublen = 0;
  return TRUE;
}

Bool
ReadFromRFBServer(char *out, unsigned int n)
{
  if (sbptr + n > SEND_BUF_SIZE) {
    printf("ERROR: Send buffer overrun. %d %d %d\n", sbptr, n, SEND_BUF_SIZE);
    return False;
  }
  memcpy(out, &sendBuf[sbptr], n);
  sbptr += n;
  return True;
};

int rfbLog (char *fmt, ...)
{
  va_list arglist;
  va_start(arglist, fmt);
  vfprintf(stdout, fmt, arglist);
  return 0;
}

void rfbLogPerror(char *str)
{
  rfbLog("");
  perror(str);
}

Bool
rfbSendRectEncodingRaw(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
  rfbClient.rfbBytesSent[rfbEncodingZlib] +=
    12 + w * h * (cl->format.bitsPerPixel / 8);
  return True;
}

Bool
WriteToSessionCapture(char *buf, int len)
{
  if (out && len > 0) {
    int written = -1;
    if ((written = fwrite(buf, len, 1, out)) < 1) {
      perror("Cannot write to output file");
      return False;
    }
  }
  return True;
}
