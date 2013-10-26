# $Id: Makefile.in,v 1.20 2012/06/13 13:50:16 imilh Exp $

PROG=		pkgin
VERSION=	0.6.4
SRCS=		main.c summary.c tools.c pkgindb.c depends.c actions.c \
		pkglist.c download.c order.c impact.c autoremove.c fsops.c \
		pkgindb_queries.c pkg_str.c sqlite_callbacks.c selection.c \
		pkg_check.c pkg_infos.c
# included from libinstall
SRCS+=		automatic.c decompress.c dewey.c fexec.c global.c \
		opattern.c pkgdb.c var.c
# included from openssh
SRCS+=		progressmeter.c

SRCS+=		

DPSRCS=	pkgindb_create.h

CC=		clang
INSTALL=	/usr/bin/install -c -o root -g wheel

OPSYS!=		uname
OS_VER!=	uname -r
OS_ARCH=	x86_64

# satisfy mk.conf
BSD_PKG_MK=	1
.for MK_CONF in /etc/mk.conf /usr/pkg/etc/mk.conf /etc/pkgsrc.conf
.	if exists(${MK_CONF})
.		include "${MK_CONF}"
.	endif
.endfor

.if ${OPSYS} == "Minix"
PKGTOOLS?=	${PKG_TOOLS_BIN}
CPPFLAGS+=	-D_NETBSD_SOURCE -D_MINIX
.endif

LOCALBASE?=		/usr/pkg
BINDIR?=		${LOCALBASE}/bin
PKG_SYSCONFDIR?=	${LOCALBASE}/etc
VARBASE?=		/var
PKG_DBDIR?=		${VARBASE}/db/pkg

PKGTOOLS?=	${LOCALBASE}/sbin

PKGIN_VERSION=	"${VERSION} for ${OPSYS}-${OS_VER} ${OS_ARCH}"

CPPFLAGS+=	-DPKGIN_VERSION=\"${PKGIN_VERSION}\"

.include <bsd.own.mk>

.PATH:	external

.if defined(DEBUG)
CPPFLAGS+=	-DDEBUG
PKGIN_DB!=	pwd
.else
PKGIN_DB=	${VARBASE}/db/pkgin
.endif

.if ${OPSYS} == "NetBSD"
WARNS=		2
CPPFLAGS+=	-DNETBSD
.endif

CPPFLAGS+=	-DHAVE_NBCOMPAT_H=1 -Iexternal -I/usr/pkg/include -I/usr/include
CPPFLAGS+=	-g

CPPFLAGS+=	-DLOCALBASE=\"${LOCALBASE}\" 			\
		-DPKG_SYSCONFDIR=\"${PKG_SYSCONFDIR}\"		\
		-DPKG_DBDIR="\"${PKG_DBDIR}\""			\
		-DDEF_LOG_DIR="\"${PKG_DBDIR}\""		\
		-DPKGIN_DB=\"${PKGIN_DB}\"			\
		-DPKGTOOLS=\"${PKGTOOLS}\"

CPPFLAGS+=	-DHAVE_CONFIG_H
CPPFLAGS+=	-D_LARGEFILE_SOURCE -D_LARGE_FILES
CPPFLAGS+=	-DCHECK_MACHINE_ARCH=\"${MACHINE_ARCH}\"
CPPFLAGS+=	-Iexternal -I. -I${LOCALBASE}/include

LDFLAGS+=	-Lexternal -L/usr/pkg/lib -L/usr/lib -lcrypto -lssl

LDADD+=		-L${LOCALBASE}/lib -Wl,-rpath,${LOCALBASE}/lib	\
		-lbz2 -lz -larchive -lfetch -lssl -lcrypto -ltermcap -lutil -lnbcompat
LDADD+=		-lsqlite3

CLEANFILES+=	${DPSRCS}

pkgindb_create.h:
	@SEDCMD=/usr/pkg/bin/nbsed ./mkpkgindb.sh > pkgindb_create.h

afterinstall:	configinstall

beforeinstall:
	${INSTALL_DIR} -o ${BINOWN} -g ${BINGRP} -m 755 ${DESTDIR}${BINDIR}
	${INSTALL_DIR} -o ${BINOWN} -g ${BINGRP} -m 755			\
		${DESTDIR}${PKG_SYSCONFDIR}/pkgin

install:
	test -f ${DESTDIR}${PKG_SYSCONFDIR}/pkgin/repositories.conf ||	\
		${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} -m 644	\
			repositories.conf				\
		${DESTDIR}${PKG_SYSCONFDIR}/pkgin/repositories.conf

# makes maintainer's life easier

WIPHOME=/Users/yourimouton/Downloads/git
CURDATE!=date +%Y%m%d
WIPREV!=git log --pretty=format:'%H' -n 1
OLDREV!=sed -En 's/VERSION=[^0-9a-z]+([0-9a-z]+)/\1/p'			\
	${WIPHOME}/pkgin/Makefile

bump:
	rm -f ${WIPHOME}/pkgin/distinfo					\
		${WIPHOME}/../distfiles/pkgin-*				\
		${WIPHOME}/../distfiles/${OLDREV}*
	perl -pi -e "s/(VERSION=[\ \t]+).*/VERSION=\t\t${WIPREV}/"	\
		${WIPHOME}/pkgin/Makefile
	perl -pi -e "s/pkgin-[0-9]+/pkgin-${CURDATE}/"			\
		${WIPHOME}/pkgin/Makefile
	cd ${WIPHOME}/pkgin && make makesum

.include <bsd.prog.mk>
