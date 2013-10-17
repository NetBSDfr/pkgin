# $NetBSD: Makefile.in,v 1.36 2009/06/05 19:58:04 joerg Exp $
#

srcdir=		.
prefix= 	/usr/local

SHELL=		/bin/sh

CC=		clang
CCLD=		clang
CFLAGS=		-I$(srcdir) -I.  -pipe -O2 -I/usr/pkg/include -I/usr/include
CPPFLAGS=	-DHAVE_NBCOMPAT_H=1 -I/Users/yourimouton/Downloads/pkgsrc/pkgtools/pkgin/work/libnbcompat -I/usr/pkg/include -I/usr/include
DEFS=		-DHAVE_CONFIG_H
INSTALL=	/Users/yourimouton/Downloads/pkgsrc/pkgtools/pkgin/work/.tools/bin/install -c
LIBMODE?=	644
LDFLAGS=	-L/Users/yourimouton/Downloads/pkgsrc/pkgtools/pkgin/work/libnbcompat -L/usr/pkg/lib -L/usr/lib

AWK=		/usr/pkg/bin/nawk
AR=		ar
RANLIB=		ranlib

LIB=		libnbcompat.a

INCS=		nbcompat.h \
		nbcompat/assert.h \
		nbcompat/cdefs.h \
		nbcompat/ctype.h \
		nbcompat/dirent.h \
		nbcompat/endian.h \
		nbcompat/err.h \
		nbcompat/fnmatch.h \
		nbcompat/fts.h \
		nbcompat/getopt.h \
		nbcompat/glob.h \
		nbcompat/grp.h \
		nbcompat/limits.h \
		nbcompat/md5.h \
		nbcompat/nbconfig.h \
		nbcompat/netdb.h \
		nbcompat/param.h \
		nbcompat/paths.h \
		nbcompat/poll.h \
		nbcompat/pwd.h \
		nbcompat/queue.h \
		nbcompat/regex.h \
		nbcompat/rmd160.h \
		nbcompat/sha1.h \
		nbcompat/sha2.h \
		nbcompat/stat.h \
		nbcompat/statvfs.h \
		nbcompat/stdbool.h \
		nbcompat/stdio.h \
		nbcompat/stdlib.h \
		nbcompat/string.h \
		nbcompat/termcap.h \
		nbcompat/time.h \
		nbcompat/types.h \
		nbcompat/tzfile.h \
		nbcompat/unistd.h \
		nbcompat/util.h \
		nbcompat/vis.h

OBJS=		 ${LIBOBJDIR}glob$U.o ${LIBOBJDIR}md5c$U.o ${LIBOBJDIR}md5hl$U.o ${LIBOBJDIR}rmd160$U.o ${LIBOBJDIR}rmd160hl$U.o ${LIBOBJDIR}sha1$U.o ${LIBOBJDIR}sha1hl$U.o ${LIBOBJDIR}sha2$U.o ${LIBOBJDIR}sha2hl$U.o ${LIBOBJDIR}vis$U.o ${LIBOBJDIR}unvis$U.o ${LIBOBJDIR}shquote$U.o ${LIBOBJDIR}warn$U.o ${LIBOBJDIR}pwcache$U.o

WITH_DB=	

.if ${WITH_DB} == "yes"
.  include "${.CURDIR}/db/Makefile.inc"
.endif

LINK=		$(CCLD) $(CFLAGS) $(LDFLAGS) -o $@
COMPILE=	$(CC) $(CPPFLAGS) $(CFLAGS)

all: nbcompat/nbconfig.h $(LIB)

.c.o:
	$(COMPILE) $(DEFS) -c $<

$(LIB): $(OBJS)
	$(AR) cr $@ $(OBJS)
	$(RANLIB) $@

nbcompat/nbconfig.h: nbcompat/config.h nbcompat.awk
	$(AWK) -f nbcompat.awk nbcompat/config.h > $@

install:
	$(INSTALL) -m 755 -d ${DESTDIR}$(prefix)/lib
	$(INSTALL) -m $(LIBMODE) ${LIB} ${DESTDIR}$(prefix)/lib
	$(RANLIB) ${DESTDIR}$(prefix)/lib/$(LIB)
	$(INSTALL) -m 755 -d ${DESTDIR}$(prefix)/include
	$(INSTALL) -m 755 -d ${DESTDIR}$(prefix)/include/nbcompat
	@for file in $(INCS); do \
		echo "$(INSTALL) -m 444 $$file ${DESTDIR}$(prefix)/include/$$file"; \
		$(INSTALL) -m 444 $$file ${DESTDIR}$(prefix)/include/$$file; \
	done

clean:
	rm -f *.a *.o bits nbcompat/nbcompat.h

distclean: clean
	rm -f Makefile config.log config.status configure.lineno
	rm -f nbcompat/config.h nbcompat/nbconfig.h
