/* NetBSD: lib.h,v 1.69 2018/02/26 23:45:02 ginsbach Exp */

/* from FreeBSD Id: lib.h,v 1.25 1997/10/08 07:48:03 charnier Exp */

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Include and define various things wanted by the library routines.
 */

/*
 * This file has been stripped to remove anything not required by pkgin.
 */

#ifndef _INST_LIB_LIB_H_
#define _INST_LIB_LIB_H_

#if HAVE_CONFIG_H
#include "config.h"
#endif
#if HAVE_NBCOMPAT_H
#include <nbcompat.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#if HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif

#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#if HAVE_DIRENT_H
#include <dirent.h>
#endif
#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STDARG_H
#include <stdarg.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Macros */
#ifndef __UNCONST
#define __UNCONST(a)	((void *)(unsigned long)(const void *)(a))
#endif

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

#ifndef DEF_UMASK
#define DEF_UMASK 022
#endif
#ifndef	PATH_MAX
#  ifdef MAXPATHLEN
#    define PATH_MAX	MAXPATHLEN
#  else
#    define PATH_MAX	1024
#  endif
#endif

enum {
	MaxPathSize = PATH_MAX
};

/* The names of our "special" files */
#define CONTENTS_FNAME		"+CONTENTS"
#define COMMENT_FNAME		"+COMMENT"
#define DESC_FNAME		"+DESC"
#define INSTALL_FNAME		"+INSTALL"
#define DEINSTALL_FNAME		"+DEINSTALL"
#define REQUIRED_BY_FNAME	"+REQUIRED_BY"
#define REQUIRED_BY_FNAME_TMP	"+REQUIRED_BY.tmp"
#define DISPLAY_FNAME		"+DISPLAY"
#define MTREE_FNAME		"+MTREE_DIRS"
#define BUILD_VERSION_FNAME	"+BUILD_VERSION"
#define BUILD_INFO_FNAME	"+BUILD_INFO"
#define INSTALLED_INFO_FNAME	"+INSTALLED_INFO"
#define SIZE_PKG_FNAME		"+SIZE_PKG"
#define SIZE_ALL_FNAME		"+SIZE_ALL"
#define PRESERVE_FNAME		"+PRESERVE"

/* The names of special variables */
#define AUTOMATIC_VARNAME	"automatic"

/* Types */
typedef unsigned int Boolean;

/* Prototypes */
/* Misc */
int	fexec(const char *, ...);
int	fexec_skipempty(const char *, ...);
int	fcexec(const char *, const char *, ...);
int	pfcexec(const char *, const char *, const char **);

/* variables file handling */

char   *var_get(const char *, const char *);
char   *var_get_memory(const char *, const char *);
int	var_set(const char *, const char *, const char *);
int     var_copy_list(const char *, const char **);

/* automatically installed as dependency */

Boolean	is_automatic_installed(const char *);
int	mark_as_automatic_installed(const char *, int);

/* String */
int     pkg_match(const char *, const char *);
int	pkg_order(const char *, const char *, const char *);
int	quick_pkg_match(const char *, const char *);

/* Package Database */
const char   *pkgdb_get_dir(void);
/*
 * Priorities:
 * 0 builtin default
 * 1 config file
 * 2 environment
 * 3 command line
 */
void	pkgdb_set_dir(const char *, int);
char   *pkgdb_pkg_dir(const char *);
char   *pkgdb_pkg_file(const char *, const char *);

/* Helper functions for memory allocation */
char *xstrdup(const char *);
void *xrealloc(void *, size_t);
void *xcalloc(size_t, size_t);
void *xmalloc(size_t);
#if defined(__GNUC__) && __GNUC__ >= 2
char	*xasprintf(const char *, ...)
			   __attribute__((__format__(__printf__, 1, 2)));
#else
char	*xasprintf(const char *, ...);
#endif

#endif				/* _INST_LIB_LIB_H_ */
