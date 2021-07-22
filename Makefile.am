#
# Copyright (c) 2020 Jonathan Perkin <jperkin@joyent.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

bin_PROGRAMS=		pkgin

#
# External source files imported from elsewhere.
#
openssh_SOURCES=	external/progressmeter.c
pkg_install_SOURCES=	external/automatic.c external/dewey.c
pkg_install_SOURCES+=	external/fexec.c external/opattern.c external/pkgdb.c
pkg_install_SOURCES+=	external/var.c external/xwrapper.c

pkgin_SOURCES=		actions.c autoremove.c depends.c download.c fsops.c impact.c
pkgin_SOURCES+=		main.c order.c pkg_check.c pkg_infos.c pkg_install.c
pkgin_SOURCES+=		pkg_str.c pkgindb.c pkgindb_queries.c pkglist.c preferred.c
pkgin_SOURCES+=		selection.c sqlite_callbacks.c summary.c tools.c
pkgin_SOURCES+=		$(openssh_SOURCES) $(pkg_install_SOURCES)

noinst_HEADERS=		cmd.h messages.h pkgin.h pkgindb.h tools.h
noinst_HEADERS+=	external/dewey.h external/humanize_number.h external/lib.h
noinst_HEADERS+=	external/progressmeter.h external/queue.h

#
# Conditional sources.
#
if !HAVE_HUMANIZE_NUMBER
pkgin_SOURCES+=		external/humanize_number.c
endif

#
# Pull in library dependencies.
#
pkgin_CPPFLAGS=		$(LIBARCHIVE_INCLUDES)
pkgin_LDADD=		$(LIBARCHIVE_LDFLAGS) $(LIBARCHIVE_LIBS)

pkgin_CPPFLAGS+=	$(LIBFETCH_INCLUDES)
pkgin_LDADD+=		$(LIBFETCH_LDFLAGS) $(LIBFETCH_LIBS)

pkgin_CPPFLAGS+=	$(OPENSSL_INCLUDES)
pkgin_LDADD+=		$(OPENSSL_LDFLAGS) $(OPENSSL_LIBS)

pkgin_CPPFLAGS+=	$(SQLITE3_INCLUDES)
pkgin_LDADD+=		$(SQLITE3_LDFLAGS) $(SQLITE3_LIBS)

#
# Required defines.
#
pkgin_CPPFLAGS+=	-DMACHINE_ARCH=\""$(MACHINE_ARCH)"\"
pkgin_CPPFLAGS+=	-DPKGIN_DBDIR=\""$(PKGIN_DBDIR)"\"
pkgin_CPPFLAGS+=	-DPKGIN_VERSION=\""$(VERSION)"\"
pkgin_CPPFLAGS+=	-DPKG_INSTALL_DIR=\""$(PKG_INSTALL_DIR)"\"
pkgin_CPPFLAGS+=	-DPKG_SYSCONFDIR=\""$(sysconfdir)"\"
pkgin_CPPFLAGS+=	-DPREFIX=\""$(prefix)"\"

#
# Manual pages.
#
man1_MANS=		pkgin.1
dist_pkgin_SOURCES=	pkgin.1.in
nodist_pkgin_SOURCES=	pkgin.1
pkgin.1: pkgin.1.in
	@sed -e 's,/var/db/pkgin,$(PKGIN_DBDIR),g' \
	     -e 's,/usr/pkg/etc,$(sysconfdir),g' $(srcdir)/pkgin.1.in >$@

#
# Generated sources.
#
dist_pkgin_SOURCES+=	pkgin.sql
nodist_pkgin_SOURCES+=	pkgindb_create.h
pkgindb_create.h: Makefile
	@echo "/* Automatically generated, DO NOT EDIT */" >$@
	@echo "#define CREATE_DRYDB \" \\" >>$@
	@sed -e 's/$$/ \\/' -e 's/\"/\\\"/g' $(srcdir)/pkgin.sql >>$@
	@echo '"'  >>$@

BUILT_SOURCES=		$(nodist_pkgin_SOURCES)
CLEANFILES=		$(BUILT_SOURCES)
