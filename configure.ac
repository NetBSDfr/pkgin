AC_PREREQ([2.61])
AC_INIT([pkgin], [0.16.1], [https://github.com/NetBSDfr/pkgin/issues])
AC_CONFIG_SRCDIR([actions.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

AC_ARG_WITH(libraries,
	[
  --with-libraries=<path> additional place to look for libraries],
	[LDFLAGS="$LDFLAGS -L$withval"],
	,
)
AC_ARG_WITH(includes,
	[
  --with-includes=<path> additional place to look for header files],
	[CPPFLAGS="$CPPFLAGS -I$withval"],
	,
)

AC_ARG_WITH(pkginstall,
	[
  --with-pkginstall=<path> pkg_install tools path],
	[PKGTOOLS="$withval"],
	,
)
AC_SUBST(PKGTOOLS)

#
# There is no real merit in testing for lots of different library functions and
# headers if they're essential anyway.  It's unlikely we will want to support
# esoteric environments.
#
# The only functions we test for are those that are non-portable, and for which
# we provide compat functions for environments where they are not available.
#
AC_CHECK_FUNC([humanize_number],,
	# May also be in libutil
	[AC_SEARCH_LIBS([humanize_number], [util],,
		# don't have it, include humanize_number.c to SRCS
		[SRCS="humanize_number.c $SRCS"]
	)]
)
AC_CHECK_HEADERS([util.h bsd/libutil.h libutil.h])
AC_CHECK_HEADERS([sys/termios.h termios.h])
#
# Tests for library functions that may exist outside of libc.
#
AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([inet_addr], [nsl])
AC_SEARCH_LIBS([fetchGetURL], [fetch],,
	[AC_MSG_ERROR(libfetch not found.)],
)

AC_SUBST(SRCS)

#
# Check for high-resolution timestamps in struct stat (from libarchive).
#
AC_CHECK_MEMBERS([struct stat.st_mtimespec.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtime_n]) # AIX
AC_CHECK_MEMBERS([struct stat.st_umtime]) # Tru64
AC_CHECK_MEMBERS([struct stat.st_mtime_usec]) # Hurd

AC_CHECK_PROG(SED,nbsed,nbsed,sed)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
