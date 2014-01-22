/*  FBS-dump -- a small utility to distil ``FBS'' files saved with
 *    Tim Waugh's rfbproxy utility.
 *  $Id: fbs-dump.c,v 1.1 2008-08-15 00:18:17 dcommander Exp $
 *  Copyright (C) 2000 Const Kaplinsky <const@ce.cctpu.edu.ru>
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
 * A bit of documentation: this utility parses ``framebuffer stream''
 * files saved by rfbproxy. It removes all control information and
 * prints raw data which were received from VNC server (to stdout).
 * Initial handshaking data is also removed from the ouptut. This tool
 * makes it easy to write next-level parsers for server messages stored
 * sequentially in such raw files.
 *
 * Important note: this ``utility'' is more a hack than a product. It
 * was written for one-time use.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>

static void show_usage (char *program_name);
static int dump_messages (FILE *in);

int main (int argc, char *argv[])
{
  FILE *in;
  char buf[12];
  int err;

  if (argc > 2) {
    show_usage (argv[0]);
    return 1;
  }

  in = (argc == 2) ? fopen (argv[1], "r") : stdin;
  if (in == NULL) {
    perror ("Cannot open input file");
    return 1;
  }

  err = (fread (buf, 1, 12, in) != 12 ||
         strncmp (buf, "FBS 001.000\n", 12) != 0 ||
         dump_messages (in) != 0);

  if (in != stdin)
    fclose (in);

  if (!err)
    fprintf (stderr, "Succeeded.\n");

  return err;
}

static void show_usage (char *program_name)
{
  fprintf (stderr,
           "Usage: %s [INPUT_FILE]\n"
           "\n"
           "If the INPUT_FILE name is not provided, standard input is used.\n"
           "Destination is always the standard output.\n",
           program_name);
}

static int dump_messages (FILE *in)
{
  u_int32_t data_len, timestamp;
  size_t padded_len;
  size_t hsh_bytes, hsh_read, name_bytes;
  char hsh[40];
  char *buf;

  hsh_bytes = 40;               /* 40 bytes of handshaking to be skipped */
  name_bytes = 0;               /* Length of the desktop name (yet unknown) */

  while (fread (&data_len, 1, 4, in) == 4) {
    data_len = ntohl (data_len);
    padded_len = data_len;
    if (data_len & 0x03)
      padded_len += 4 - (data_len & 0x03);

    if (hsh_bytes) {
      hsh_read = (hsh_bytes > data_len) ? data_len : hsh_bytes;

      if (fread (hsh + 40 - hsh_bytes, 1, hsh_read, in) != hsh_read) {
        fprintf (stderr, "Read error.\n");
        return -1;
      }
      hsh_bytes -= hsh_read;
      data_len -= hsh_read;
      padded_len -= hsh_read;

      if (!hsh_bytes) {
        /* Set the desktop name length */
        name_bytes = ntohl (*((unsigned long *)(hsh + 36)));
        /* Print some information */
        fprintf (stderr, "Protocol version: %.11s\n", hsh);
        fprintf (stderr, "Framebuffer size: %hu * %hu\n",
                 ntohs (*((unsigned short *)(hsh + 16))),
                 ntohs (*((unsigned short *)(hsh + 18))));
      }
    }

    buf = malloc (padded_len + 4);
    if (buf == NULL) {
      perror ("malloc() failed");
      return -1;
    }

    if (fread (buf, 1, padded_len + 4, in) != padded_len + 4) {
      fprintf (stderr, "Read error.\n");
      free (buf);
      return -1;
    }

    if (data_len) {
      if (name_bytes) {
        if (name_bytes != data_len) {
          fprintf (stderr, "Internal program error.\n");
          free (buf);
          return -1;
        }
        fprintf (stderr, "Desktop name: %.*s\n", name_bytes, buf);
        name_bytes = 0;
      } else {
        if (fwrite (buf, 1, data_len, stdout) != data_len) {
          fprintf (stderr, "Write error.\n");
          free (buf);
          return -1;
        }
      }
    }

    /* Currently not used... */
    timestamp = ntohl (*((u_int32_t *)(buf + padded_len)));

    free (buf);
  }

  return (ferror (in)) ? -1 : 0;
}

