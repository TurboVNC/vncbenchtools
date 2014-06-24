/*
 * h264.c
 *
 * Routines to implement H.264 Encoding
 */

/*
 *  Copyright (C) 2014 D. R. Commander.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 *  USA.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "rfb.h"
#include "x264.h"
#include "turbojpeg.h"
#include "output.h"


extern int decompress, ublen;
extern char *outfilename;


#define PAD(v, p) ((v+(p)-1)&(~((p)-1)))


/* Globals */

x264_picture_t pic_in, pic_out;
Bool pic_init = FALSE;
x264_t *encoder = NULL;
unsigned char *yuvImage = NULL;
unsigned long solidrect=0, solidpixels=0, monorect=0, monopixels=0, ndxrect=0,
	ndxpixels=0, jpegrect=0, jpegpixels=0, fcrect=0, fcpixels=0, gradrect=0,
	gradpixels=0;
tjhandle tj = NULL;
hnd_t output_handle = 0;
int frames = 0;


/*
 * H.264 encoding implementation.
 */

static Bool CheckUpdateBuf(rfbClientPtr cl, int bytes)
{
  if (ublen + bytes > UPDATE_BUF_SIZE) {
    if (!rfbSendUpdateBuf(cl))
      return FALSE;
  }
  return TRUE;
}


static Bool SendCompressedData(rfbClientPtr cl, char *buf, int compressedLen)
{
  int i, portionLen;

  portionLen = UPDATE_BUF_SIZE;
  for (i = 0; i < compressedLen; i += portionLen) {
    if (i + portionLen > compressedLen)
      portionLen = compressedLen - i;
    if (!CheckUpdateBuf(cl, portionLen))
      return FALSE;
    memcpy(&updateBuf[ublen], &buf[i], portionLen);
    ublen += portionLen;
  }
  cl->rfbBytesSent[rfbEncodingTight] += compressedLen;
  cl->rfbRectanglesSent[rfbEncodingTight]++;

  return TRUE;
}


Bool rfbSendRectEncodingTight(rfbClientPtr cl, int x, int y, int w, int h)
{
  int frame_size;
  x264_nal_t* nals = NULL;
  int i_nals = 0;
  x264_picture_t pic_out;
  int pixelFormat = TJPF_RGB, pixelSize = rfbServerFormat.bitsPerPixel / 8;
  x264_param_t param;

  if (!encoder) {
    x264_param_default_preset(&param, "veryfast", "zerolatency");
    param.i_threads = 1;
    param.i_width = rfbScreen.width;
    param.i_height = rfbScreen.height;
    param.i_fps_num = param.i_timebase_den = 30000;
    param.i_fps_den = param.i_timebase_num = 1001;
    param.rc.i_rc_method = X264_RC_CQP;
    param.rc.i_qp_constant = 10;
    param.b_repeat_headers = 0;
    param.b_annexb = 0;
    x264_param_apply_profile(&param, "baseline");
    encoder = x264_encoder_open(&param);
    x264_encoder_parameters(encoder, &param);
  }
  if (!yuvImage) {
    yuvImage = (unsigned char *)malloc(tjBufSizeYUV(rfbScreen.width,
      rfbScreen.height, TJSAMP_420));
    if (!yuvImage) {
      rfbLog("Memory allocation error\n");
      return FALSE;
    }
  }
  if (!tj) {
    tj = tjInitCompress();
    if (!tj) {
      rfbLog("TurboJPEG error: %s\n", tjGetErrorStr());
      return FALSE;
    }
  }
  if (!pic_init) {
    int pw = PAD(rfbScreen.width, 2), ph = PAD(rfbScreen.height, 2);
    x264_picture_init(&pic_in);
    pic_in.img.i_csp = X264_CSP_I420;
    pic_in.img.i_plane = 3;
    pic_in.img.i_stride[0] = PAD(pw, 4);
    pic_in.img.i_stride[1] = pic_in.img.i_stride[2] = PAD(pw / 2, 4);
    pic_in.img.plane[0] = yuvImage;
    pic_in.img.plane[1] = pic_in.img.plane[0] +
      pic_in.img.i_stride[0] * ph;
    pic_in.img.plane[2] = pic_in.img.plane[1] +
      pic_in.img.i_stride[1] * (ph / 2);
  }
  if (outfilename && !output_handle) {
    cli_output_opt_t output_opt;
    x264_nal_t *headers;
    int i_nal;

    memset(&output_opt, 0, sizeof(cli_output_opt_t));
    if (flv_output.open_file(outfilename, &output_handle, &output_opt) == -1) {
      rfbLog("Could not open output file\n");
      return FALSE;
    }
    if (flv_output.set_param(output_handle, &param) == -1) {
      rfbLog("Could not set FLV parameters\n");
      return FALSE;
    }
    if (x264_encoder_headers(encoder, &headers, &i_nal) == -1 ||
        flv_output.write_headers(output_handle, headers) == -1) {
      rfbLog("Could not write FLV headers\n");
      return FALSE;
    }
  }

  if (rfbServerFormat.redShift == 16 && rfbServerFormat.blueShift == 0)
    pixelFormat = TJPF_BGR;
  if (pixelSize == 4)
    pixelFormat += 2;
  if(rfbServerFormat.bigEndian && pixelSize == 4) {
    if (pixelFormat == TJPF_RGBX) pixelFormat = TJPF_XRGB;
    else if (pixelFormat == TJPF_BGRX) pixelFormat = TJPF_XBGR;
  }

  if (tjEncodeYUV2(tj, rfbScreen.pfbMemory, rfbScreen.width,
    rfbScreen.paddedWidthInBytes, rfbScreen.height, pixelFormat, yuvImage,
    TJSAMP_420, 0) < 0) {
    rfbLog("TurboJPEG Error: %s\n", tjGetErrorStr());
    return FALSE;
  }

  pic_in.i_pts = frames;
  frame_size = x264_encoder_encode(encoder, &nals, &i_nals, &pic_in, &pic_out);
  if (frame_size < 0) {
    rfbLog("x264 Error\n");
    return FALSE;
  }

  if (output_handle) {
    if (flv_output.write_frame(output_handle, nals[0].p_payload, frame_size,
                               &pic_out) != frame_size) {
      rfbLog("Could not write output file\n");
      return FALSE;
    }
  }
  frames++;

  return SendCompressedData(cl, nals[0].p_payload, frame_size);
}


void ResetH264Encoder(rfbClientPtr cl)
{
  if (encoder) {
    x264_encoder_close(encoder);
    encoder = NULL;
  }
  if (yuvImage) {
    free(yuvImage);
    yuvImage = NULL;
  }
  if (pic_init) {
    pic_init = FALSE;
  }
  if (tj) {
    tjDestroy(tj);
    tj = NULL;
  }
  if (output_handle)
    flv_output.close_file(output_handle, frames - 1, 0);
  frames = 0;
}
