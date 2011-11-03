# $Id: Makefile,v 1.7 2011-10-07 09:15:45 dcommander Exp $

INSTDIR = /usr/local/bin

CC = gcc
CXX = g++
CFLAGS = -O3 -m32 -Wall -fno-strict-aliasing -I. -D_GNU_SOURCE
LDFLAGS = -lz -O3 -m32 -ljpeg -lturbojpeg -lpthread

PROG = compare-encodings
OBJS = compare-encodings.o misc.o hextile.o zlib.o translate.o
SRCS = compare-encodings.c misc.c hextile.c zlib.c translate.c

ifeq ($(TIGERVNC), yes)
	OBJS := $(OBJS) tiger.o
	SRCS := $(SRCS) tiger.cxx
	CFLAGS := $(CFLAGS) -I/opt/libjpeg-turbo/include
	LDFLAGS := $(LDFLAGS) -L/opt/libjpeg-turbo/lib32 -Wl,-R/opt/libjpeg-turbo/lib32
	ifeq ($(TIGERD), yes)
		CFLAGS := $(CFLAGS) -DTIGER_BOTH
	endif
else
	OBJS := $(OBJS) tight.o
	SRCS := $(SRCS) tight.c
endif

ifeq ($(TIGERD), yes)
  CFLAGS := $(CFLAGS) -DTIGERD
endif

default: $(PROG)

install: $(PROG)
	install -p -g root -o root $(PROG) $(INSTDIR)

ifeq ($(TIGERVNC), yes)

$(PROG): $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROG)

else

ifeq ($(TIGERD), yes)
$(PROG): $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROG)
else
$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(PROG)
endif

endif

clean:
	rm -f  $(PROG) $(OBJS) *core* *~ *.bak

depend: $(SRCS)
	makedepend -Y -I. $(SRCS) 2> /dev/null

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

ifeq ($(TIGERD), yes)
compare-encodings.o: compare-encodings.c
	$(CXX) $(CFLAGS) -c $< -o $@
endif

%.o: %.cxx
	$(CXX) $(CFLAGS) -c $< -o $@

# DO NOT DELETE

compare-encodings.o: rfb.h rfbproto.h hextiled.c zlibd.c tightd.c tigerd.cxx
misc.o: rfb.h rfbproto.h
hextile.o: rfb.h rfbproto.h
zlib.o: rfb.h rfbproto.h
tight.o: rfb.h rfbproto.h
tiger.o: rfb.h rfbproto.h
translate.o: rfb.h rfbproto.h tableinittctemplate.c tabletranstemplate.c
