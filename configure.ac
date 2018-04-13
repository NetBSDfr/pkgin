AC_PREREQ([2.61])
AC_INIT([pkgin], [0.9.4.99], [imil@NetBSD.org])
AC_CONFIG_SRCDIR([actions.c])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST
AC_GNU_SOURCE

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

AC_CHECK_FUNCS([dup2 getcwd localeconv memmove memset mkdir putenv regcomp rmdir setenv strcasecmp strchr strcspn strdup strncasecmp strpbrk strrchr strstr strtol freopen tcgetpgrp])

AC_CHECK_FUNC([pthread_create],,
	AC_CHECK_LIB(pthread, pthread_create,,
	)
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
AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h locale.h netdb.h stdint.h stdlib.h string.h sys/file.h sys/param.h sys/statvfs.h sys/time.h unistd.h sys/queue.h ctype.h stdio.h assert.h err.h stdarg.h sys/wait.h errno.h dirent.h regex.h sys/poll.h sys/signal.h signal.h termcap.h fnmatch.h sys/utsname.h nbcompat.h nbcompat/string.h util.h bsd/libutil.h libutil.h sys/termios.h termios.h sys/cdefs.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Check for high-resolution timestamps in struct stat (from libarchive).
AC_CHECK_MEMBERS([struct stat.st_mtimespec.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtim.tv_nsec])
AC_CHECK_MEMBERS([struct stat.st_mtime_n]) # AIX
AC_CHECK_MEMBERS([struct stat.st_umtime]) # Tru64
AC_CHECK_MEMBERS([struct stat.st_mtime_usec]) # Hurd

AC_CHECK_PROG(SED,nbsed,nbsed,sed)

dnl fix rpath for SunOS, is there a cleaner way to do this ?
case $host_os in
	sunos* | solaris*)
		RPATH="-Wl,-R"
		;;
	*)
		RPATH="-Wl,-rpath"
		;;
esac
AC_SUBST(RPATH)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
