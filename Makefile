# $Id: Makefile,v 1.1.1.1 2008-07-15 23:08:11 dcommander Exp $

INSTDIR = /usr/local/bin

CC = gcc
CFLAGS = -O2 -I/usr/include
LDFLAGS = -lz

PROG = compare-encodings
OBJS = compare-encodings.o misc.o hextile.o zlib.o tight.o
SRCS = compare-encodings.c misc.c hextile.c zlib.c tight.c

default: $(PROG)

install: $(PROG)
	install -p -g root -o root $(PROG) $(INSTDIR)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(PROG) $(OBJS)

clean:
	rm -f  $(PROG) $(OBJS) *core* *~ *.bak

depend: $(SRCS)
	makedepend -Y -I. $(SRCS) 2> /dev/null

.c.o:
	$(CC) $(CFLAGS) -c $<

# DO NOT DELETE

compare-encodings.o: rfb.h rfbproto.h
misc.o: rfb.h rfbproto.h
hextile.o: rfb.h rfbproto.h
zlib.o: rfb.h rfbproto.h
tight.o: rfb.h rfbproto.h
