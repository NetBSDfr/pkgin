AC_PREREQ([2.61])
AC_INIT([pkgin], [23.12.0], [https://github.com/NetBSDfr/pkgin/issues])
AC_CONFIG_SRCDIR([actions.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

# Code is written to a C99 baseline.
: ${CFLAGS="-std=c99 -g -O2"}

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

#
# Enable some useful compiler warnings if running in maintainer mode.  Note
# that maintainer mode is explicitly disabled in the pkgsrc build to avoid
# user failures (the bug reports are nice, the user experience is not).
#
AS_IF([test "x$USE_MAINTAINER_MODE" = "xyes"],
      CFLAGS="$CFLAGS -Werror -Wall -Wextra -Wunused -Wno-unused-parameter"
)

#
# --with-dbdir=/path/to/pkgin/db
#
AC_ARG_WITH([dbdir],
	[AS_HELP_STRING([--with-dbdir=DIR],
		[Path to pkgin database directory])],
	[PKGIN_DBDIR="$withval"],
	[AC_MSG_ERROR([--with-dbdir is mandatory])]
)
AC_SUBST(PKGIN_DBDIR)

#
# Check for required libraries.  All of them accept --with-*=DIR arguments
# so that we can be specific about which one to use.
#
AC_ARG_WITH([libarchive],
	[AS_HELP_STRING([--with-libarchive=DIR],
		[Root libarchive directory (e.g. /usr/local)])],
	[
		LIBARCHIVE_INCLUDES="-I$withval/include"
		LIBARCHIVE_LDFLAGS="-L$withval/lib"
		LIBARCHIVE_LIBS="-larchive"
	]
)
CPPFLAGS_save=$CPPFLAGS; CPPFLAGS="$CPPFLAGS $LIBARCHIVE_INCLUDES"
LDFLAGS_save=$LDFLAGS; LDFLAGS="$LDFLAGS $LIBARCHIVE_LDFLAGS"
LIBS_save=$LIBS; LIBS="$LIBS $LIBARCHIVE_LIBS"
AC_CHECK_HEADER([archive.h],[],[AC_MSG_FAILURE([can't find archive.h])])
AC_CHECK_LIB([archive],[archive_read_new],[],
	[AC_MSG_FAILURE([can't find libarchive])])
CPPFLAGS=$CPPFLAGS_save
LDFLAGS=$LDFLAGS_save
LIBS=$LIBS_save
AC_SUBST([LIBARCHIVE_INCLUDES])
AC_SUBST([LIBARCHIVE_LDFLAGS])
AC_SUBST([LIBARCHIVE_LIBS])

AC_ARG_WITH([libfetch],
	[AS_HELP_STRING([--with-libfetch=DIR],
		[Root libfetch directory (e.g. /usr/local)])],
	[
		LIBFETCH_INCLUDES="-I$withval/include"
		LIBFETCH_LDFLAGS="-L$withval/lib"
		LIBFETCH_LIBS="-lfetch"
	]
)
CPPFLAGS_save=$CPPFLAGS; CPPFLAGS="$CPPFLAGS $LIBFETCH_INCLUDES"
LDFLAGS_save=$LDFLAGS; LDFLAGS="$LDFLAGS $LIBFETCH_LDFLAGS"
LIBS_save=$LIBS; LIBS="$LIBS $LIBFETCH_LIBS"
AC_CHECK_HEADER([fetch.h],[],[AC_MSG_FAILURE([can't find fetch.h])])
AC_CHECK_LIB([fetch],[fetchMakeURL],[],
	[AC_MSG_FAILURE([can't find libfetch])])
CPPFLAGS=$CPPFLAGS_save
LDFLAGS=$LDFLAGS_save
LIBS=$LIBS_save
AC_SUBST([LIBFETCH_INCLUDES])
AC_SUBST([LIBFETCH_LDFLAGS])
AC_SUBST([LIBFETCH_LIBS])

AC_ARG_WITH([openssl],
	[AS_HELP_STRING([--with-openssl=DIR],
		[Root openssl directory (e.g. /usr/local)])],
	[
		OPENSSL_INCLUDES="-I$withval/include"
		OPENSSL_LDFLAGS="-L$withval/lib"
		OPENSSL_LIBS="-lssl -lcrypto"
	]
)
CPPFLAGS_save=$CPPFLAGS; CPPFLAGS="$CPPFLAGS $OPENSSL_INCLUDES"
LDFLAGS_save=$LDFLAGS; LDFLAGS="$LDFLAGS $OPENSSL_LDFLAGS"
LIBS_save=$LIBS; LIBS="$LIBS $OPENSSL_LIBS"
AC_CHECK_HEADER([openssl/ssl.h],[],
	[AC_MSG_FAILURE([can't find openssl/ssl.h])])
AC_CHECK_LIB([ssl],[SSL_CTX_new],[],
	[AC_MSG_FAILURE([can't find libssl])])
CPPFLAGS=$CPPFLAGS_save
LDFLAGS=$LDFLAGS_save
LIBS=$LIBS_save
AC_SUBST([OPENSSL_INCLUDES])
AC_SUBST([OPENSSL_LDFLAGS])
AC_SUBST([OPENSSL_LIBS])

AC_ARG_WITH([sqlite3],
	[AS_HELP_STRING([--with-sqlite3=DIR],
		[Root sqlite3 directory (e.g. /usr/local)])],
	[
		SQLITE3_INCLUDES="-I$withval/include"
		SQLITE3_LDFLAGS="-L$withval/lib"
		SQLITE3_LIBS="-lsqlite3"
	]
)
CPPFLAGS_save=$CPPFLAGS; CPPFLAGS="$CPPFLAGS $SQLITE3_INCLUDES"
LDFLAGS_save=$LDFLAGS; LDFLAGS="$LDFLAGS $SQLITE3_LDFLAGS"
LIBS_save=$LIBS; LIBS="$LIBS $SQLITE3_LIBS"
AC_CHECK_HEADER([sqlite3.h],[],[AC_MSG_FAILURE([can't find sqlite3.h])])
AC_CHECK_LIB([sqlite3],[sqlite3_open_v2],[],
	[AC_MSG_FAILURE([can't find libsqlite3])])
CPPFLAGS=$CPPFLAGS_save
LDFLAGS=$LDFLAGS_save
LIBS=$LIBS_save
AC_SUBST([SQLITE3_INCLUDES])
AC_SUBST([SQLITE3_LDFLAGS])
AC_SUBST([SQLITE3_LIBS])

#
# --with-machine-arch=MACHINE_ARCH
#
AC_ARG_WITH([machine-arch],
	[AS_HELP_STRING([--with-machine-arch=ARCH],
		[Target MACHINE_ARCH])],
	[MACHINE_ARCH="$withval"],
	[AC_MSG_ERROR([--with-machine-arch is mandatory])]
)
AC_SUBST(MACHINE_ARCH)

#
# --with-pkg-install=/path/to/pkg_install/sbin
#
AC_ARG_WITH([pkg-install],
	[AS_HELP_STRING([--with-pkg-install=DIR],
		[Path to pkg_install commands (e.g. /usr/pkg/sbin)])],
	[
		PKG_INSTALL_DIR="$withval"
	],[
		AC_MSG_ERROR([--with-pkg-install is mandatory])
	]
)
AC_SUBST(PKG_INSTALL_DIR)

#
# There is no real merit in testing for lots of different library functions and
# headers if they're essential anyway.  It's unlikely we will want to support
# esoteric environments.
#
# The only functions we test for are those that are non-portable, and for which
# we provide compat functions for environments where they are not available.
#
AC_CHECK_FUNC([humanize_number],
	AM_CONDITIONAL([HAVE_HUMANIZE_NUMBER], true),
	# May also be in libutil
	[AC_SEARCH_LIBS([humanize_number], [util],
		AM_CONDITIONAL(HAVE_HUMANIZE_NUMBER, true),
		AM_CONDITIONAL(HAVE_HUMANIZE_NUMBER, false)
	)]
)
AC_CHECK_HEADERS([util.h bsd/libutil.h libutil.h])
AC_CHECK_HEADERS([sys/termios.h termios.h])
#
# Tests for library functions that may exist outside of libc.
#
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([inet_addr], [nsl])

#
# Check for high-resolution timestamps in struct stat (from libarchive).
#
AC_CHECK_MEMBERS([struct stat.st_mtimespec.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtime_n]) # AIX
AC_CHECK_MEMBERS([struct stat.st_umtime]) # Tru64
AC_CHECK_MEMBERS([struct stat.st_mtime_usec]) # Hurd

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
