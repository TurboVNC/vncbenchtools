/*  RawUpdates2ppm -- an utility to convert ``raw'' files saved with
 *    the fbs-dump utility to separate 24-bit ppm files.
 *  $Id: compare-encodings.c,v 1.9 2011-10-07 09:15:45 dcommander Exp $
 *  Copyright (C) 2000 Const Kaplinsky <const@ce.cctpu.edu.ru>
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *  Copyright (C) 2010, 2012, 2014 D. R. Commander
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

/*
 * Important note: this ``utility'' is more a hack than a product. It
 * was written for one-time use.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/time.h>

#include "rfb.h"

#ifndef min
 #define min(a,b) ((a)<(b)?(a):(b))
#endif

#define BUFFER_SIZE (1024*512)
static char buffer[BUFFER_SIZE];

static char *compressedData = NULL;
static char *uncompressedData = NULL;

#define GET_PIXEL8(pix, ptr) ((pix) = *(ptr)++)

#define GET_PIXEL16(pix, ptr) (((CARD8*)&(pix))[0] = *(ptr)++, \
			       ((CARD8*)&(pix))[1] = *(ptr)++)

#define GET_PIXEL32(pix, ptr) (((CARD8*)&(pix))[0] = *(ptr)++, \
			       ((CARD8*)&(pix))[1] = *(ptr)++, \
			       ((CARD8*)&(pix))[2] = *(ptr)++, \
			       ((CARD8*)&(pix))[3] = *(ptr)++)

/* The zlib encoding requires expansion/decompression/deflation of the
   compressed data in the "buffer" above into another, result buffer.
   However, the size of the result buffer can be determined precisely
   based on the bitsPerPixel, height and width of the rectangle.  We
   allocate this buffer one time to be the full size of the buffer. */

static int raw_buffer_size = -1;
static char *raw_buffer;

static z_stream decompStream;
static Bool decompStreamInited = False;

/*
 * Variables for the ``tight'' encoding implementation.
 */

/* Separate buffer for compressed data. */
#define ZLIB_BUFFER_SIZE 512
static char zlib_buffer[ZLIB_BUFFER_SIZE];

/* Four independent compression streams for zlib library. */
static z_stream zlibStream[4];
Bool zlibStreamActive[4] = {
  False, False, False, False
};

/* Filter stuff. Should be initialized by filter initialization code. */
static Bool cutZeros;
static int rectWidth, rectColors;
static char tightPalette[256*4];
static CARD8 tightPrevRow[2048*3*sizeof(CARD16)];

#ifndef TIGERD
/* JPEG decoder state. */

static Bool jpegError;

#include <jpeglib.h>
#include <turbojpeg.h>

tjhandle tjhnd=NULL;

#ifdef H264
extern Bool ResetH264Encoder(rfbClientPtr);
#endif

/*
 * JPEG source manager functions for JPEG decompression in Tight decoder.
 */

static struct jpeg_source_mgr jpegSrcManager;
static JOCTET *jpegBufferPtr;
static size_t jpegBufferLen;

static void
JpegInitSource(j_decompress_ptr cinfo)
{
  jpegError = False;
}

static boolean
JpegFillInputBuffer(j_decompress_ptr cinfo)
{
  jpegError = True;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;
  jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;

  return TRUE;
}

static void
JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes)
{
  if (num_bytes < 0 || num_bytes > jpegSrcManager.bytes_in_buffer) {
    jpegError = True;
    jpegSrcManager.bytes_in_buffer = jpegBufferLen;
    jpegSrcManager.next_input_byte = (JOCTET *)jpegBufferPtr;
  } else {
    jpegSrcManager.next_input_byte += (size_t) num_bytes;
    jpegSrcManager.bytes_in_buffer -= (size_t) num_bytes;
  }
}

static void
JpegTermSource(j_decompress_ptr cinfo)
{
  /* No work necessary here. */
}

static void
JpegSetSrcManager(j_decompress_ptr cinfo, CARD8 *compressedData,
		  int compressedLen)
{
  jpegBufferPtr = (JOCTET *)compressedData;
  jpegBufferLen = (size_t)compressedLen;

  jpegSrcManager.init_source = JpegInitSource;
  jpegSrcManager.fill_input_buffer = JpegFillInputBuffer;
  jpegSrcManager.skip_input_data = JpegSkipInputData;
  jpegSrcManager.resync_to_restart = jpeg_resync_to_restart;
  jpegSrcManager.term_source = JpegTermSource;
  jpegSrcManager.next_input_byte = jpegBufferPtr;
  jpegSrcManager.bytes_in_buffer = jpegBufferLen;

  cinfo->src = &jpegSrcManager;
}

#endif

static long
ReadCompactLen (void)
{
  long len;
  CARD8 b;

  if (!ReadFromRFBServer((char *)&b, 1))
    return -1;
  len = (int)b & 0x7F;
  if (b & 0x80) {
    if (!ReadFromRFBServer((char *)&b, 1))
      return -1;
    len |= ((int)b & 0x7F) << 7;
    if (b & 0x80) {
      if (!ReadFromRFBServer((char *)&b, 1))
	return -1;
      len |= ((int)b & 0xFF) << 14;
    }
  }
  return len;
}

#define myFormat rfbClient.format
#define BPP 8
#include "hextiled.c"
#include "zlibd.c"
#ifdef TIGERD
#include "tigerd.cxx"
#else
#include "tightd.c"
#endif
#undef BPP
#define BPP 16
#include "hextiled.c"
#include "zlibd.c"
#ifdef TIGERD
#include "tigerd.cxx"
#else
#include "tightd.c"
#endif
#undef BPP
#define BPP 32
#include "hextiled.c"
#include "zlibd.c"
#ifdef TIGERD
#include "tigerd.cxx"
#else
#include "tightd.c"
#endif
#undef BPP

#define TIGHT_STATISTICS
#ifdef TIGHT_STATISTICS
extern unsigned long solidrect, solidpixels, monorect, monopixels, ndxrect, ndxpixels, jpegrect, jpegpixels, gradrect, gradpixels, fcrect, fcpixels;
#endif


double gettime(void)
{
  struct timeval tv;
  gettimeofday(&tv, (struct timezone *)NULL);
  return((double)tv.tv_sec+(double)tv.tv_usec*0.000001);
}

#define SAVE_PATH  "./ppm"
/* #define SAVE_PPM_FILES */
/* #define LAZY_TIGHT */

static int color_depth = 16;
static int sum_raw = 0, sum_tight = 0, sum_hextile = 0, sum_zlib = 0,
  sum_zrle = 0;
static double t0, ttight[2]={0.,0.}, thextile[2]={0.,0.}, tzlib[2]={0.,0.},
  tzrle[2]={0.,0.};
static int tndx = 0;

static void show_usage (char *program_name);
static int do_convert (FILE *in);
static int parse_fb_update (FILE *in);

static int parse_rectangle (FILE *in, int xpos, int ypos,
                            int width, int height, int rect_no, int enc);

#ifdef SAVE_PPM_FILES
static int save_rectangle (FILE *ppm, int width, int height, int depth);
#endif

static int handle_hextile8 (FILE *in, int x, int y, int width, int height);
static int handle_hextile16 (FILE *in, int x, int y, int width, int height);
static int handle_hextile32 (FILE *in, int x, int y, int width, int height);

static int handle_raw8 (FILE *in, int x, int y, int width, int height);
static int handle_raw16 (FILE *in, int x, int y, int width, int height);
static int handle_raw32 (FILE *in, int x, int y, int width, int height);

static void copy_data (char *data, int x, int y, int w, int h, int bpp);

static CARD32 get_CARD32 (char *ptr);
static CARD16 get_CARD16 (char *ptr);


static int total_updates;
static int total_rects;
static unsigned long total_pixels;
static int tightonly = 0;
static int verbose = 0;
static int flip_rgb = 0;
int decompress = 0;
FILE *out = NULL;
char *outfilename = NULL;

int main (int argc, char *argv[])
{
  FILE *in;
  int i;
  char *filename = NULL;
  int err;

  if (argc < 2) {
    show_usage (argv[0]);
    return 1;
  }

  for (i = 0; i < argc; i++) {
    if (strcmp (argv[i], "-8") == 0) {
      color_depth = 8;
    } else if (strcmp (argv[i], "-16") == 0) {
      color_depth = 16;
    } else if (strcmp (argv[i], "-24") == 0) {
      color_depth = 24;
    } else if (strcmp (argv[i], "-to") == 0) {
      tightonly = 1;
    } else if (strcmp (argv[i], "-d") == 0) {
      decompress = 1;
    } else if (strcmp (argv[i], "-o") == 0) {
      if (i < argc - 1) {
        outfilename = argv[++i];
        tightonly = 1;
      }
    } else if (strcmp (argv[i], "-r") == 0) {
      flip_rgb = 1;
    } else if (strcmp (argv[i], "-v") == 0) {
      verbose = 1;
    } else filename = argv[i];
  }

  in = filename ? fopen (filename, "r") : stdin;
  if (in == NULL) {
    perror ("Cannot open input file");
    return 1;
  }

  #ifndef H264
  if (outfilename) {
    decompress = 0;
    out = fopen (outfilename, "wb");
    if (out == NULL) {
      perror ("Cannot open output file");
      return 1;
    }
  }
  #endif

  err = (do_convert (in) != 0);
  #ifdef H264
  err |= (ResetH264Encoder(&rfbClient) != TRUE);
  #endif
  if (outfilename) goto done;
  sleep(5);
  fseek(in, 0, SEEK_SET);
  sum_raw = sum_hextile = sum_zlib = sum_zrle = sum_tight = 0;
	#ifdef TIGHT_STATISTICS
  fcrect = ndxrect = jpegrect = monorect = solidrect = 0;
  fcpixels = ndxpixels = jpegpixels = monopixels = solidpixels = 0;
  #endif
  decompStreamInited = False;
  for(i = 0; i < 4; i++) zlibStreamActive[i] = False;
  err |= (do_convert (in) != 0);

  done:
  if (in != stdin)
    fclose (in);

  if (out != NULL)
    fclose (out);

  fprintf (stderr, (err) ? "Fatal error has occured.\n" : "Succeeded.\n");
  return err;
}

static void show_usage (char *program_name)
{
  fprintf (stderr,
           "Usage: %s [-8|-16|-24] [INPUT_FILE]\n"
           "\n"
           "If the INPUT_FILE name is not provided, standard input is used.\n",
           program_name);
}

static int do_convert (FILE *in)
{
  int msg_type, n;
  char buf[8];

  InitEverything (color_depth);

#ifdef LAZY_TIGHT
  rfbTightDisableGradient = TRUE;
#endif

  total_updates = 0;
  total_rects = 0;
  total_pixels = 0;

  if (verbose)
    printf ("upd.no -                              Bytes per rectangle:\n"
            "   rect.no   coords     size       raw |hextile| zlib | ZRLE | tight\n"
            "---------- -------------------- -------+-------+------+------+------\n");

  msg_type = getc (in);
  while (msg_type != EOF) {
    switch (msg_type) {
    case rfbFramebufferUpdate:
      if (parse_fb_update (in) != 0)
        return -1;
      break;

    case rfbSetColourMapEntries:
      fprintf (stderr, "> SetColourMap...\n");
      if (fread (buf + 1, 1, 5, in) != 5) {
        fprintf (stderr, "Read error.\n");
        return -1;
      }
      n = (int) get_CARD16 (buf + 4);
      while (n--) {
        if (fread (buf, 1, 6, in) != 6) {
          fprintf (stderr, "Read error.\n");
          return -1;
        }
      }
      break;

    case rfbBell:
      fprintf (stderr, "> Bell...\n");
      break;

    case rfbServerCutText:
      fprintf (stderr, "> ServerCutText...\n");
      if (fread (buf + 1, 1, 7, in) != 7) {
        fprintf (stderr, "Read error.\n");
        return -1;
      }
      n = (size_t) get_CARD32 (buf + 4);
      while (n--) {
        if (getc (in) == EOF) {
          fprintf (stderr, "Read error.\n");
          return -1;
        }
      }
      break;

    default:
      fprintf (stderr, "Unknown server message: 0x%X\n", msg_type);
      return -1;                /* Unknown server message */
    }
    msg_type = getc (in);
  }

  if(tightonly) sum_raw=sum_hextile=sum_zlib=sum_zrle=INT_MAX;

  printf ("\nGrand totals:\n"
          "                          raw    |  hextile  |   zlib   |   ZRLE   |  tight  \n"
          "                       ----------+-----------+----------+----------+---------\n"
          "Bytes in all rects:    %9d | %9d | %8d | %8d | %8d\n"
          "Tight/XXX B/W saving:  %8.2f%% | %8.2f%% | %7.2f%% | %7.2f%% |\n",
          sum_raw, sum_hextile, sum_zlib, sum_zrle, sum_tight,
          (double)(sum_raw - sum_tight) * 100 / (double) sum_raw,
          (double)(sum_hextile - sum_tight) * 100 / (double) sum_hextile,
          (double)(sum_zlib - sum_tight) * 100 / (double) sum_zlib,
          (double)(sum_zrle - sum_tight) * 100 / (double) sum_zrle);
  printf ("%scoding time:         ......... | %8.4fs | %7.4fs | %7.4fs | %7.4fs\n",
          decompress? "De":"En", thextile[tndx], tzlib[tndx], tzrle[tndx],
          ttight[tndx]);

  #ifdef TIGHT_STATISTICS
  printf("Solid rectangles = %lu, pixels = %f mil\n", solidrect, (double)solidpixels/1000000.);
  printf("Mono rectangles  = %lu, pixels = %f mil\n", monorect, (double)monopixels/1000000.);
  printf("Index rectangles = %lu, pixels = %f mil\n", ndxrect, (double)ndxpixels/1000000.);
  printf("JPEG rectangles  = %lu, pixels = %f mil\n", jpegrect, (double)jpegpixels/1000000.);
  printf("Grad rectangles  = %lu, pixels = %f mil\n", gradrect, (double)gradpixels/1000000.);
  printf("Raw rectangles   = %lu, pixels = %f mil\n", fcrect, (double)fcpixels/1000000.);
  #endif

  printf("Avg. pixel count for %d FB updates:  %f\n", total_rects,
	 (double)total_pixels/(double)total_rects);
  printf("\n");
  if(tndx==1)
  printf ("Avg. %scoding time:    ......... | %8.4fs | %7.4fs | %7.4fs | %7.4fs\n\n",
          decompress? "De":"En", (thextile[0]+thextile[1])/2.,
          (tzlib[0]+tzlib[1])/2., (tzrle[0]+tzrle[1])/2.,
          (ttight[0]+ttight[1])/2.);
  tndx++;

  return (ferror (in)) ? -1 : 0;
}

static int parse_fb_update (FILE *in)
{
  rfbFramebufferUpdateMsg msg;
  rfbFramebufferUpdateRectHeader rh;
  CARD16 rect_count;
  CARD16 xpos, ypos, width, height;
  int i;
  CARD32 enc;

  memset(&msg, 0, sz_rfbFramebufferUpdateMsg);
  if (fread (&msg.pad, 1, 3, in) != 3) {
    fprintf (stderr, "Read error.\n");
    return -1;
  }

  rect_count = get_CARD16 ((char *)&msg.nRects);

  if (out) msg.nRects = 0xFFFF;
  if (!WriteToSessionCapture((char *)&msg, sz_rfbFramebufferUpdateMsg))
    return False;

  for (i = 0; i < rect_count; i++) {
    if (fread (&rh, 1, sz_rfbFramebufferUpdateRectHeader, in)
	!= sz_rfbFramebufferUpdateRectHeader) {
      fprintf (stderr, "Read error.\n");
      return -1;
    }
    xpos = get_CARD16 ((char *)&rh.r.x);
    ypos = get_CARD16 ((char *)&rh.r.y);
    width = get_CARD16 ((char *)&rh.r.w);
    height = get_CARD16 ((char *)&rh.r.h);
    total_pixels += width * height;

    enc = get_CARD32 ((char *)&rh.encoding);

    parse_rectangle(in, xpos, ypos, width, height, i, enc);
    total_rects++;
  }

  if (out) {
    rh.encoding = Swap32IfLE(rfbEncodingLastRect);
    rh.r.x = rh.r.y = rh.r.w = rh.r.h = 0;
    if (!WriteToSessionCapture((char *)&rh, sz_rfbFramebufferUpdateRectHeader))
      return False;
  }

  total_updates++;

  return 0;
}

static int parse_rectangle (FILE *in, int xpos, int ypos,
                            int width, int height, int rect_no, int enc)
{
#ifdef SAVE_PPM_FILES
  char fname[80];  FILE *ppm;
#endif
  int err, i;
  int pixel_bytes;

  if (enc == 5) {
    switch (color_depth) {
    case 8:
      err = handle_hextile8 (in, xpos, ypos, width, height);
      pixel_bytes = 1;
      break;
    case 16:
      err = handle_hextile16 (in, xpos, ypos, width, height);
      pixel_bytes = 2;
      break;
    default:                      /* 24 */
      err = handle_hextile32 (in, xpos, ypos, width, height);
      pixel_bytes = 4;
    }
  } else if (enc == 0) {
    switch (color_depth) {
    case 8:
      err = handle_raw8 (in, xpos, ypos, width, height);
      pixel_bytes = 1;
      break;
    case 16:
      err = handle_raw16 (in, xpos, ypos, width, height);
      pixel_bytes = 2;
      break;
    default:                      /* 24 */
      err = handle_raw32 (in, xpos, ypos, width, height);
      pixel_bytes = 4;
    }
  } else {
    fprintf (stderr, "Wrong encoding: 0x%02X=%d.\n", enc, enc);
    return -1;
  }

  if (err != 0) {
    fprintf (stderr, "Error decoding rectangle.\n");
    return -1;
  }

#ifdef SAVE_PPM_FILES
  sprintf (fname, "%.40s/%05d-%04d.ppm", SAVE_PATH, total_updates, rect_no);
  ppm = fopen (fname, "w");
  if (ppm != NULL) {
    save_rectangle (ppm, rfbScreen.width, rfbScreen.height, color_depth);
    fclose (ppm);
  }
#endif

  rfbClient.rfbBytesSent[rfbEncodingHextile] = 0;
  rfbClient.rfbRectanglesSent[rfbEncodingHextile] = 0;
  rfbClient.rfbBytesSent[rfbEncodingZlib] = 0;
  rfbClient.rfbRectanglesSent[rfbEncodingZlib] = 0;
  rfbClient.rfbBytesSent[rfbEncodingZRLE] = 0;
  rfbClient.rfbRectanglesSent[rfbEncodingZRLE] = 0;
  rfbClient.rfbBytesSent[rfbEncodingTight] = 0;
  rfbClient.rfbRectanglesSent[rfbEncodingTight] = 0;

  if(!tightonly) {

  sblen = sbptr = 0;
  if(!decompress) t0 = gettime();
  if (!rfbSendRectEncodingHextile(&rfbClient, xpos, ypos, width, height)) {
      fprintf (stderr, "Error in hextile encoder!\n");
      return -1;
  }
  if(!rfbSendUpdateBuf(&rfbClient)) {
    fprintf(stderr, "Could not flush output buffer\n");
    return -1;
  }
  if(!decompress) thextile[tndx] += gettime() - t0;
  if(decompress) {
    for (i = 0; i < rfbClient.rfbRectanglesSent[rfbEncodingHextile]; i++) {
      rfbFramebufferUpdateRectHeader rect;
      if (!ReadFromRFBServer((char *)&rect, sz_rfbFramebufferUpdateRectHeader)) {
        fprintf(stderr, "Could not read rectangle header.\n");
        return -1;
      }
      rect.encoding = Swap32IfLE(rect.encoding);
      rect.r.x = Swap16IfLE(rect.r.x);
      rect.r.y = Swap16IfLE(rect.r.y);
      rect.r.w = Swap16IfLE(rect.r.w);
      rect.r.h = Swap16IfLE(rect.r.h);
      if (rect.encoding == rfbEncodingHextile) {
        t0 = gettime();
        switch (color_depth) {
        case 8:
          err = HandleHextile8 (rect.r.x, rect.r.y, rect.r.w, rect.r.h);  break;
        case 16:
          err = HandleHextile16 (rect.r.x, rect.r.y, rect.r.w, rect.r.h);  break;
        default:
          err = HandleHextile32 (rect.r.x, rect.r.y, rect.r.w, rect.r.h);  break;
        }
        if (!err) {
          fprintf (stderr, "Error in hextile decoder!\n");
          return -1;
        }
        thextile[tndx] += gettime() - t0;
      }
			else {
        printf("Non-hextile rectangle encountered!\n");
        return -1;
      }
    }
    if(sbptr != sblen) {
      printf("ERROR: incomplete decode of hextile-encoded rectangles.\n");
      return -1;
    }
  }

  sblen = sbptr = 0;
  if(!decompress) t0 = gettime();
  if (!rfbSendRectEncodingZlib(&rfbClient, xpos, ypos, width, height)) {
      fprintf (stderr, "Error in zlib encoder!.\n");
      return -1;
  }
  if(!rfbSendUpdateBuf(&rfbClient)) {
    fprintf(stderr, "Could not flush output buffer\n");
    return -1;
  }
  if(!decompress) tzlib[tndx] += gettime() - t0;
  if(decompress) {
    for (i = 0; i < rfbClient.rfbRectanglesSent[rfbEncodingZlib]; i++) {
      rfbFramebufferUpdateRectHeader rect;
      if (!ReadFromRFBServer((char *)&rect, sz_rfbFramebufferUpdateRectHeader)) {
        fprintf(stderr, "Could not read rectangle header.\n");
        return -1;
      }
      rect.encoding = Swap32IfLE(rect.encoding);
      rect.r.x = Swap16IfLE(rect.r.x);
      rect.r.y = Swap16IfLE(rect.r.y);
      rect.r.w = Swap16IfLE(rect.r.w);
      rect.r.h = Swap16IfLE(rect.r.h);
      if (rect.encoding == rfbEncodingZlib) {
        t0 = gettime();
        switch (color_depth) {
        case 8:
          err = HandleZlib8 (rect.r.x, rect.r.y, rect.r.w, rect.r.h);  break;
        case 16:
          err = HandleZlib16 (rect.r.x, rect.r.y, rect.r.w, rect.r.h);  break;
        default:
          err = HandleZlib32 (rect.r.x, rect.r.y, rect.r.w, rect.r.h);  break;
        }
        if (!err) {
          fprintf (stderr, "Error in zlib decoder!\n");
          return -1;
        }
        tzlib[tndx] += gettime() - t0;
      }
			else {
        printf("Non-zlib rectangle encountered!\n");
        return -1;
      }
    }
    if(sbptr != sblen) {
      printf("ERROR: incomplete decode of zlib-encoded data.\n");
      return -1;
    }
  }

  sblen = sbptr = 0;
  if (!decompress) {
    sblen = sbptr = 0;
    t0 = gettime();
    if (!rfbSendRectEncodingZRLE(&rfbClient, xpos, ypos, width, height)) {
      fprintf (stderr, "Error in ZRLE encoder!.\n");
      return -1;
    }
    if(!rfbSendUpdateBuf(&rfbClient)) {
      fprintf(stderr, "Could not flush output buffer\n");
      return -1;
    }
    tzrle[tndx] += gettime() - t0;
    rfbFreeZrleData(&rfbClient);
  }

  }

  sblen = sbptr = 0;
  if(!decompress) t0 = gettime();
  if (!rfbSendRectEncodingTight(&rfbClient, xpos, ypos, width, height)) {
      fprintf (stderr, "Error in tight encoder!.\n");
      return -1;
  }
  if(!rfbSendUpdateBuf(&rfbClient)) {
    fprintf(stderr, "Could not flush output buffer\n");
    return -1;
  }
  if(!decompress) ttight[tndx] += gettime() - t0;
  if(decompress) {
    #ifdef __TURBOD_MT__
    if (!threadInit) {
      InitThreads();
      if (!threadInit) return False;
    }
    #endif
    for (i = 0; i < rfbClient.rfbRectanglesSent[rfbEncodingTight]; i++) {
      rfbFramebufferUpdateRectHeader rect;
      if (!ReadFromRFBServer((char *)&rect, sz_rfbFramebufferUpdateRectHeader)) {
        fprintf(stderr, "Could not read rectangle header.\n");
        return -1;
      }
      rect.encoding = Swap32IfLE(rect.encoding);
      rect.r.x = Swap16IfLE(rect.r.x);
      rect.r.y = Swap16IfLE(rect.r.y);
      rect.r.w = Swap16IfLE(rect.r.w);
      rect.r.h = Swap16IfLE(rect.r.h);
      if (rect.encoding == rfbEncodingTight) {
        t0 = gettime();
        switch (color_depth) {
        case 8:
          err = HandleTight8 (rect.r.x, rect.r.y, rect.r.w, rect.r.h);  break;
        case 16:
          err = HandleTight16 (rect.r.x, rect.r.y, rect.r.w, rect.r.h);  break;
        default:
          err = HandleTight32 (rect.r.x, rect.r.y, rect.r.w, rect.r.h);  break;
        }
        if (!err) {
          fprintf (stderr, "Error in tight decoder!\n");
          return -1;
        }
        ttight[tndx] += gettime() - t0;
      }
			else {
        printf("Non-tight rectangle encountered!\n");
        return -1;
      }
    }
    if(sbptr != sblen) {
      printf("ERROR: incomplete decode of tight-encoded data.\n");
      return -1;
    }

    #ifdef __TURBOD_MT__
    for (i = 1; i < nt; i++) {
      pthread_mutex_lock(&tparam[i].done);
      pthread_mutex_unlock(&tparam[i].done);
    }
    #endif
  }

  if (verbose)
    printf ("%05d-%04d (%4d,%3d %4d*%3d): %7d|%7d|%6d|%6d|%6d\n",
            total_updates, rect_no, xpos, ypos, width, height,
            width * height * pixel_bytes + 12,
            rfbClient.rfbBytesSent[rfbEncodingHextile],
            rfbClient.rfbBytesSent[rfbEncodingZlib],
            rfbClient.rfbBytesSent[rfbEncodingZRLE],
            rfbClient.rfbBytesSent[rfbEncodingTight]);

  sum_raw += width * height * pixel_bytes + 12;
  sum_hextile += rfbClient.rfbBytesSent[rfbEncodingHextile];
  sum_tight += rfbClient.rfbBytesSent[rfbEncodingTight];
  sum_zrle += rfbClient.rfbBytesSent[rfbEncodingZRLE];
  sum_zlib += rfbClient.rfbBytesSent[rfbEncodingZlib];

  return 0;
}

#ifdef SAVE_PPM_FILES
static int save_rectangle (FILE *ppm, int width, int height, int depth)
{
  CARD8 *data8 = (CARD8 *) rfbScreen.pfbMemory;
  CARD16 *data16 = (CARD16 *) rfbScreen.pfbMemory;
  CARD32 *data32 = (CARD32 *) rfbScreen.pfbMemory;

  int i, r, g, b;

  fprintf (ppm, "P6\n%d %d\n255\n", width, height);

  switch (depth) {
  case 8:
    for (i = 0; i < width * height; i++) {
      r = *data8 & 0x07;
      r = (r << 5) | (r << 2) | (r >> 1);
      g = (*data8 >> 3) & 0x07;
      g = (g << 5) | (g << 2) | (g >> 1);
      b = (*data8 >> 6) & 0x03;
      b = (b << 6) | (b << 4) | (b >> 2) | b;

      putc (r, ppm); putc (g, ppm); putc (b, ppm);

      data8++;
    }
    break;
  case 16:
    for (i = 0; i < width * height; i++) {
      r = (*data16 >> 11) & 0x1F;
      r = (r << 3) | (r >> 2);
      g = (*data16 >> 5) & 0x3F;
      g = (g << 2) | (g >> 4);
      b = *data16 & 0x1F;
      b = (b << 3) | (b >> 2);

      putc (r, ppm); putc (g, ppm); putc (b, ppm);

      data16++;
    }
    break;
  default:                      /* 24 */
    for (i = 0; i < width * height; i++) {
      r = (*data32 >> 16) & 0xFF;
      g = (*data32 >> 8) & 0xFF;
      b = *data32 & 0xFF;

      putc (r, ppm); putc (g, ppm); putc (b, ppm);

      data32++;
    }
  }

  return 1;
}
#endif

/*
 * Decoding raw rectangles.
 */

#define DEFINE_HANDLE_RAW(bpp)                                             \
                                                                           \
static int handle_raw##bpp (FILE *in, int x, int y, int width, int height) \
{                                                                          \
  char *data = NULL;                                                       \
                                                                           \
  if ((data = (char *)malloc(width * height * (bpp / 8))) == NULL) {       \
    fprintf (stderr, "Memory allocation error.\n");                        \
    return -1;                                                             \
  }                                                                        \
                                                                           \
  if ((int)fread (data, 1, width * height * (bpp / 8), in)                 \
    != width * height * (bpp / 8)) {                                       \
    fprintf (stderr, "Read error.\n");                                     \
    return -1;                                                             \
  }                                                                        \
                                                                           \
  copy_data ((char *)data, x, y, width, height, bpp);                      \
  free(data);                                                              \
  return 0;                                                                \
}

DEFINE_HANDLE_RAW(8)
DEFINE_HANDLE_RAW(16)
DEFINE_HANDLE_RAW(32)

/*
 * Decoding hextile rectangles.
 */

#define rfbHextileRaw			(1 << 0)
#define rfbHextileBackgroundSpecified	(1 << 1)
#define rfbHextileForegroundSpecified	(1 << 2)
#define rfbHextileAnySubrects		(1 << 3)
#define rfbHextileSubrectsColoured	(1 << 4)

#define DEFINE_HANDLE_HEXTILE(bpp)                                         \
                                                                           \
static int handle_hextile##bpp (FILE *in, int xpos, int ypos, int width,   \
                                int height)                                \
{                                                                          \
  CARD##bpp bg = 0, fg = 0;                                                \
  int x, y, w, h;                                                          \
  int sx, sy, sw, sh;                                                      \
  int jx, jy;                                                              \
  int subencoding, n_subrects, coord_pair;                                 \
  CARD##bpp data[16*16];                                                   \
  int i;                                                                   \
                                                                           \
  for (y = 0; y < height; y += 16) {                                       \
    for (x = 0; x < width; x += 16) {                                      \
      w = h = 16;                                                          \
      if (width - x < 16)                                                  \
        w = width - x;                                                     \
      if (height - y < 16)                                                 \
        h = height - y;                                                    \
                                                                           \
      subencoding = getc (in);                                             \
      if (subencoding == EOF) {                                            \
        fprintf (stderr, "Read error.\n");                                 \
        return -1;                                                         \
      }                                                                    \
                                                                           \
      if (subencoding & rfbHextileRaw) {                                   \
        if ((int)fread (data, 1, w * h * (bpp / 8), in)                    \
          != w * h * (bpp / 8)) {                                          \
          fprintf (stderr, "Read error.\n");                               \
          return -1;                                                       \
        }                                                                  \
                                                                           \
        copy_data ((char *)data, xpos + x, ypos + y, w, h, bpp);           \
        continue;                                                          \
      }                                                                    \
                                                                           \
      if (subencoding & rfbHextileBackgroundSpecified) {                   \
        if (fread (&bg, 1, (bpp / 8), in) != (bpp / 8)) {                  \
          fprintf (stderr, "Read error.\n");                               \
          return -1;                                                       \
        }                                                                  \
      }                                                                    \
                                                                           \
      for (i = 0; i < w * h; i++)                                          \
        data[i] = bg;                                                      \
                                                                           \
      if (subencoding & rfbHextileForegroundSpecified) {                   \
        if (fread (&fg, 1, (bpp / 8), in) != (bpp / 8)) {                  \
          fprintf (stderr, "Read error.\n");                               \
          return -1;                                                       \
        }                                                                  \
      }                                                                    \
                                                                           \
      if (!(subencoding & rfbHextileAnySubrects)) {                        \
        copy_data ((char *)data, xpos + x, ypos + y, w, h, bpp);           \
        continue;                                                          \
      }                                                                    \
                                                                           \
      if ((n_subrects = getc (in)) == EOF) {                               \
        fprintf (stderr, "Read error.\n");                                 \
        return -1;                                                         \
      }                                                                    \
                                                                           \
      for (i = 0; i < n_subrects; i++) {                                   \
        if (subencoding & rfbHextileSubrectsColoured) {                    \
          if (fread (&fg, 1, (bpp / 8), in) != (bpp / 8)) {                \
            fprintf (stderr, "Read error.\n");                             \
            return -1;                                                     \
          }                                                                \
        }                                                                  \
        if ((coord_pair = getc (in)) == EOF) {                             \
          fprintf (stderr, "Read error.\n");                               \
          return -1;                                                       \
        }                                                                  \
        sx = rfbHextileExtractX (coord_pair);                              \
        sy = rfbHextileExtractY (coord_pair);                              \
        if ((coord_pair = getc (in)) == EOF) {                             \
          fprintf (stderr, "Read error.\n");                               \
          return -1;                                                       \
        }                                                                  \
        sw = rfbHextileExtractW (coord_pair);                              \
        sh = rfbHextileExtractH (coord_pair);                              \
        if (sx + sw > w || sy + sh > h) {                                  \
          fprintf (stderr, "Wrong hextile data, please use"                \
                           " appropriate -8/-16/-24 option.\n");           \
          return -1;                                                       \
        }                                                                  \
                                                                           \
        for (jy = sy; jy < sy + sh; jy++) {                                \
          for (jx = sx; jx < sx + sw; jx++)                                \
            data[jy * w + jx] = fg;                                        \
        }                                                                  \
      }                                                                    \
      copy_data ((char *)data, xpos + x, ypos + y, w, h, bpp);             \
    }                                                                      \
  }                                                                        \
  return 0;                                                                \
}

DEFINE_HANDLE_HEXTILE(8)
DEFINE_HANDLE_HEXTILE(16)
DEFINE_HANDLE_HEXTILE(32)

static void copy_data (char *data, int x, int y, int w, int h, int bpp)
{
  int py;
  int pixel_bytes;

  pixel_bytes = bpp / 8;

  for (py = y; py < y + h; py++) {
    if (flip_rgb && pixel_bytes >= 3) {
      char *ptr;
      for (ptr = data; ptr < data + w * pixel_bytes; ptr += pixel_bytes) {
        CARD8 temp = ptr[2];
        ptr[2] = ptr[0];
        ptr[0] = temp;
      }
    }
    memcpy (&rfbScreen.pfbMemory[py * rfbScreen.paddedWidthInBytes +
                                 x * pixel_bytes], data, w * pixel_bytes);
    data += w * pixel_bytes;
  }
}

static CARD32 get_CARD32 (char *ptr)
{
  return (CARD32) ntohl (*(unsigned long *)ptr);
}

static CARD16 get_CARD16 (char *ptr)
{
  return (CARD16) ntohs (*(unsigned short *)ptr);
}

