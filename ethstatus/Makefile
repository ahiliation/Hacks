# Makefile for EthStatus

# Edited for Debian GNU/Linux.
DESTDIR =

LDFLAGS += -lncurses
CFLAGS += -O2 -Wall

BIN = ethstatus
SRC = ethstatus.c

# Edited for Debian GNU/Linux.
INSTDIR = $(DESTDIR)/usr/bin

INSTALL=/usr/bin/install
INSTALL_PARMS=-o 0 -g 0 -m 0755

all : ethstatus

ethstatus : ethstatus.c ethstatus.h
	gcc $(CFLAGS) $(CPPFLAGS) -o ${BIN} ${SRC} $(LDFLAGS)

clean:
	rm -f ${BIN} *.o core *~ 

install:
	# Edited for Debian GNU/Linux.
	$(INSTALL) $(INSTALL_PARMS) ethstatus $(DESTDIR)/usr/bin
