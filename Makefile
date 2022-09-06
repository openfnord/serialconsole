#
# $Header$
#

### compile options

# C compiler to use
CC?=	cc

# compile with debugging
CFLAGS+=	-Wall -g

# Release version
CFLAGS+=	-DSC_VERSION='"1.0"'

# Enable this for operating systems that have a broken poll(2)
# implementation:
#   Mac OS X 10.4
#CFLAGS+=	-DHAS_BROKEN_POLL

# device to use if none given on command line
#CFLAGS+=	-DDEFAULTDEVICE='"cuad0"'

#CFLAGS+=	-DDEFAULTDEVICE='"ttyS0"'	# for Linux
#CFLAGS+=	-DDEFAULTDEVICE='"ttyUSB0"'	# for Linux USB devices

# default speed to use
#CFLAGS+=	-DDEFAULTSPEED='"9600"'

# default parameters to use
#CFLAGS+=	-DDEFAULTPARMS='"8n1"'

### install options
PREFIX?=$(DESTDIR)/usr/local

all:	sc

sc:	sc.c
	${CC} ${CFLAGS} -o $@ $<

clean:
	rm -f *.o sc *~

install:	sc
	[ -d $(PREFIX)/bin ] || install -m 755 -d $(PREFIX)/bin
	install -m 755 sc $(PREFIX)/bin/
	[ -d $(PREFIX)/man/man1 ] || install -m 755 -d $(PREFIX)/man/man1
	install -m 644 sc.1 $(PREFIX)/man/man1/

uninstall:
	rm -f $(PREFIX)/bin/sc $(PREFIX)/man/man1/sc.1
