# $Id: Makefile,v 1.5 2011-07-29 17:21:42 dcommander Exp $

INSTDIR = /usr/local/bin

CC = gcc
CFLAGS = -O3 -m32 -I/usr/include -D_GNU_SOURCE
LDFLAGS = -lz -O3 -m32 -ljpeg -lturbojpeg -lpthread

PROG = compare-encodings
OBJS = compare-encodings.o misc.o hextile.o zlib.o tight.o translate.o
SRCS = compare-encodings.c misc.c hextile.c zlib.c tight.c translate.c

default: $(PROG)

install: $(PROG)
	install -p -g root -o root $(PROG) $(INSTDIR)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROG)

clean:
	rm -f  $(PROG) $(OBJS) *core* *~ *.bak

depend: $(SRCS)
	makedepend -Y -I. $(SRCS) 2> /dev/null

.c.o:
	$(CC) $(CFLAGS) -c $<

# DO NOT DELETE

compare-encodings.o: rfb.h rfbproto.h hextiled.c zlibd.c tightd.c
misc.o: rfb.h rfbproto.h
hextile.o: rfb.h rfbproto.h
zlib.o: rfb.h rfbproto.h
tight.o: rfb.h rfbproto.h
translate.o: rfb.h rfbproto.h tableinittctemplate.c tabletranstemplate.c
