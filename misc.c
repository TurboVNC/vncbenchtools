/* misc functions */

#include <string.h>
#include "rfb.h"

char updateBuf[UPDATE_BUF_SIZE];
int ublen;

rfbClientRec rfbClient;
rfbScreenInfo rfbScreen;
rfbPixelFormat rfbServerFormat;

void InitEverything (int color_depth)
{
  int i;

  rfbClient.translateFn = rfbTranslateNone;
  for (i = 0; i < MAX_ENCODINGS; i++) {
    rfbClient.rfbRectanglesSent[i] = 0;
    rfbClient.rfbBytesSent[i] = 0;
  }
  rfbClient.rfbFramebufferUpdateMessagesSent = 0;
  rfbClient.rfbRawBytesEquivalent = 0;

  rfbClient.compStreamInited = 0;
  for (i = 0; i < MAX_ENCODINGS; i++) {
    rfbClient.zsActive[i] = FALSE;
  }

  rfbClient.format.depth = color_depth;
  rfbClient.format.bitsPerPixel = color_depth;
  if (color_depth == 24)
    rfbClient.format.bitsPerPixel = 32;

  rfbServerFormat.depth = color_depth;
  rfbServerFormat.bitsPerPixel = rfbClient.format.bitsPerPixel;
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

  ublen = 0;
}

/*
 * rfbTranslateNone is used when no translation is required.
 */

void
rfbTranslateNone(char *table, rfbPixelFormat *in, rfbPixelFormat *out,
		 char *iptr, char *optr, int bytesBetweenInputLines,
		 int width, int height)
{
    int bytesPerOutputLine = width * (out->bitsPerPixel / 8);

    while (height > 0) {
	memcpy(optr, iptr, bytesPerOutputLine);
	iptr += bytesBetweenInputLines;
	optr += bytesPerOutputLine;
	height--;
    }
}

BOOL rfbSendUpdateBuf(rfbClientPtr cl)
{
  ublen = 0;
  return TRUE;
}

int rfbLog (char *fmt, ...)
{
  return 0;
}

Bool
rfbSendRectEncodingRaw(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
  rfbClient.rfbBytesSent[rfbEncodingZlib] +=
    12 + w * h * (cl->format.bitsPerPixel / 8);
}

