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

# check for humanize_number
AC_CHECK_FUNC([humanize_number],,
	# in DragonFly humanize_number is in libutil
	AC_SEARCH_LIBS([humanize_number], [util],,
		# don't have it, include humanize_number.c to SRCS
		[SRCS="humanize_number.c $SRCS"]
	)
)

AC_SUBST(SRCS)
AC_SUBST(PKGTOOLS)

AC_SEARCH_LIBS([socket], [socket])
AC_SEARCH_LIBS([inet_addr], [nsl])

# check for libnbcompat
AC_CHECK_LIB([nbcompat], [strlcpy],
	[LIBS="$LIBS -lnbcompat"]
)

AC_SEARCH_LIBS([fetchGetURL], [fetch],,
	[AC_MSG_ERROR(libfetch not found.)],
)

# Checks for header files.
AC_CHECK_HEADERS([err.h sys/queue.h])
AC_CHECK_HEADERS([nbcompat.h nbcompat/string.h])
AC_CHECK_HEADERS([util.h bsd/libutil.h libutil.h])
AC_CHECK_HEADERS([sys/termios.h termios.h])

# Check for high-resolution timestamps in struct stat (from libarchive).
AC_CHECK_MEMBERS([struct stat.st_mtimespec.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtime_n]) # AIX
AC_CHECK_MEMBERS([struct stat.st_umtime]) # Tru64
AC_CHECK_MEMBERS([struct stat.st_mtime_usec]) # Hurd

AC_CHECK_PROG(SED,nbsed,nbsed,sed)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
