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
#include <stdio.h>
#include "rfb.h"
#include "x264.h"
#include "turbojpeg.h"
#include "output.h"


extern int decompress, ublen;
extern char *outfilename;


#define PAD(v, p) ((v+(p)-1)&(~((p)-1)))


/* Globals */

x264_picture_t pic_in;
Bool pic_init = FALSE;
x264_t *encoder = NULL;
unsigned char *yuvImage = NULL;
unsigned long solidrect=0, solidpixels=0, monorect=0, monopixels=0, ndxrect=0,
	ndxpixels=0, jpegrect=0, jpegpixels=0, fcrect=0, fcpixels=0, gradrect=0,
	gradpixels=0;
tjhandle tj = NULL;
hnd_t output_handle = 0;
int frames = 0;
int stride[3];
unsigned char *plane[3];
Bool frameSent = FALSE;


/* Frame rate governor */

extern double gettime();
static double lastFrame = -1.;
static double fps = 0.0;


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
  int pixelFormat = TJPF_RGB, pixelSize = rfbServerFormat.bitsPerPixel / 8,
    pitch = rfbScreen.paddedWidthInBytes;
  x264_param_t param;
  unsigned char *dstBuf[3], *srcPtr;

  if (!encoder) {
    char *env=NULL;

    if ((env = getenv("H264_FPS")) != NULL && strlen(env) > 0) {
      double temp = -1;
      if (sscanf(env, "%lf", &temp) == 1 && temp > 0.0)
        fps = temp;
      rfbLog("H.264 frame rate set to %.2lf\n", fps);
    }
    
    x264_param_default_preset(&param, "veryfast", "zerolatency");
    param.i_threads = 1;
    param.i_width = rfbScreen.width;
    param.i_height = rfbScreen.height;
    param.i_fps_num = param.i_timebase_den =
      fps > 0.0 ? (int)(fps * 1001.0 + 0.5) : 30000;
    param.i_fps_den = param.i_timebase_num = 1001;
    param.rc.i_rc_method = X264_RC_CQP;
    param.rc.i_qp_constant = 10;
    param.b_repeat_headers = 0;
    param.b_annexb = 0;
    param.i_keyint_max = 200000;
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
    stride[0] = pic_in.img.i_stride[0] = PAD(pw, 4);
    stride[1] = stride[2] = pic_in.img.i_stride[1] = pic_in.img.i_stride[2]
      = PAD(pw / 2, 4);
    plane[0] = pic_in.img.plane[0] = yuvImage;
    plane[1] = pic_in.img.plane[1] = pic_in.img.plane[0] +
      pic_in.img.i_stride[0] * ph;
    plane[2] = pic_in.img.plane[2] = pic_in.img.plane[1] +
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

  srcPtr = (unsigned char *)&rfbScreen.pfbMemory[pitch * y + pixelSize * x];
  if (x % 2 != 0) {x--;  w++;}
  if (y % 2 != 0) {y--;  h++;}
  if (w % 2 != 0) w++;
  if (h % 2 != 0) h++;
  dstBuf[0] = plane[0] + stride[0] * y + x;
  dstBuf[1] = plane[1] + stride[1] * (y / 2) + (x / 2);
  dstBuf[2] = plane[2] + stride[2] * (y / 2) + (x / 2);
  if (tjEncodeYUVPlanes(tj, srcPtr, w, pitch, h, pixelFormat, dstBuf, stride,
      TJSAMP_420, 0) < 0) {
    rfbLog("TurboJPEG Error: %s\n", tjGetErrorStr());
    return FALSE;
  }

  if (fps > 0.0) {
    if (lastFrame >= 0.0 &&
        gettime() - lastFrame < 1.0 / fps) {
      frameSent = FALSE;
      return TRUE;
    }
    lastFrame = gettime();
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
  frameSent = TRUE;

  return SendCompressedData(cl, (char *)nals[0].p_payload, frame_size);
}


Bool ResetH264Encoder(rfbClientPtr cl)
{
  Bool retval = TRUE;

  if (!frameSent) {
    x264_nal_t* nals = NULL;
    int i_nals = 0;
    x264_picture_t pic_out;

    int frame_size = x264_encoder_encode(encoder, &nals, &i_nals, &pic_in,
                                         &pic_out);
    if (frame_size < 0) {
      rfbLog("x264 Error\n");
      retval = FALSE;
    } else {
      if (output_handle) {
        if (flv_output.write_frame(output_handle, nals[0].p_payload,
                                   frame_size, &pic_out) != frame_size) {
          rfbLog("Could not write output file\n");
          retval = FALSE;
        }
      }
      frames++;
      if (!SendCompressedData(cl, (char *)nals[0].p_payload, frame_size)) {
        rfbLog("Could not send compressed data\n");
        retval = FALSE;
      }
    }
  }

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
  lastFrame = -1.;

  return retval;
}
