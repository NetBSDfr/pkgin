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

#include "config.h"

/*
 * Include our copy of queue.h before nbcompat pulls in its version.
 */
#include "queue.h"

#if HAVE_NBCOMPAT_H
#include <nbcompat.h>
#else
#include <err.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

/* Macros */
#ifndef __UNCONST
#define __UNCONST(a)	((void *)(unsigned long)(const void *)(a))
#endif

#define SUCCESS	(0)
#define	FAIL	(-1)

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

/* Prefix for extended PLIST cmd */
#define CMD_CHAR		'@'

typedef enum pl_ent_t {
	PLIST_SHOW_ALL = -1,
	PLIST_FILE,		/*  0 */
	PLIST_CWD,		/*  1 */
	PLIST_CMD,		/*  2 */
	PLIST_CHMOD,		/*  3 */
	PLIST_CHOWN,		/*  4 */
	PLIST_CHGRP,		/*  5 */
	PLIST_COMMENT,		/*  6 */
	PLIST_IGNORE,		/*  7 */
	PLIST_NAME,		/*  8 */
	PLIST_UNEXEC,		/*  9 */
	PLIST_SRC,		/* 10 */
	PLIST_DISPLAY,		/* 11 */
	PLIST_PKGDEP,		/* 12 */
	PLIST_DIR_RM,		/* 13 */
	PLIST_OPTION,		/* 14 */
	PLIST_PKGCFL,		/* 15 */
	PLIST_BLDDEP,		/* 16 */
	PLIST_PKGDIR		/* 17 */
}       pl_ent_t;

/* Types */
typedef unsigned int Boolean;

/* This structure describes a packing list entry */
typedef struct plist_t {
	struct plist_t *prev;	/* previous entry */
	struct plist_t *next;	/* next entry */
	char   *name;		/* name of entry */
	Boolean marked;		/* whether entry has been marked */
	pl_ent_t type;		/* type of entry */
}       plist_t;

/* This structure describes a package's complete packing list */
typedef struct package_t {
	plist_t *head;		/* head of list */
	plist_t *tail;		/* tail of list */
}       package_t;

#define SYMLINK_HEADER	"Symlink:"
#define CHECKSUM_HEADER	"MD5:"

enum {
	ChecksumHeaderLen = 4,	/* strlen(CHECKSUM_HEADER) */
	SymlinkHeaderLen = 8,	/* strlen(SYMLINK_HEADER) */
	ChecksumLen = 16,
	LegibleChecksumLen = 33
};

/* List of packages */
typedef struct _lpkg_t {
	TAILQ_ENTRY(_lpkg_t) lp_link;
	char   *lp_name;
}       lpkg_t;
TAILQ_HEAD(_lpkg_head_t, _lpkg_t);
typedef struct _lpkg_head_t lpkg_head_t;

/*
 * To improve performance when handling lists containing a large number of
 * packages, it can be beneficial to use hashed lookups to avoid excessive
 * strcmp() calls when searching for existing entries.
 *
 * The simple hashing function below uses the first 3 characters of either a
 * pattern match or package name (as they are guaranteed to exist).
 *
 * Based on pkgsrc package names across the tree, this can still result in
 * somewhat uneven distribution due to high numbers of packages beginning with
 * "p5-", "php", "py-" etc, and so there are diminishing returns when trying to
 * use a hash size larger than around 16 or so.
 */
#define PKG_HASH_SIZE		16
#define PKG_HASH_ENTRY(x)	(((unsigned char)(x)[0] \
				+ (unsigned char)(x)[1] * 257 \
				+ (unsigned char)(x)[2] * 65537) \
				& (PKG_HASH_SIZE - 1))

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

/* Iterator functions */
int	iterate_pkg_db(int (*)(const char *, void *), void *);
char	*find_matching_installed_pkg(const char *, int, int);

/* Packing list */
plist_t *new_plist_entry(void);
plist_t *last_plist(package_t *);
plist_t *find_plist(package_t *, pl_ent_t);
char   *find_plist_option(package_t *, const char *);
void    plist_delete(package_t *, Boolean, pl_ent_t, char *);
void    free_plist(package_t *);
void    mark_plist(package_t *);
void    csum_plist_entry(char *, plist_t *);
void    add_plist(package_t *, pl_ent_t, const char *);
void    add_plist_top(package_t *, pl_ent_t, const char *);
void    delete_plist(package_t *, Boolean, pl_ent_t, char *);
void    write_plist(package_t *, FILE *, char *);
void	stringify_plist(package_t *, char **, size_t *, const char *);
void	parse_plist(package_t *, const char *);
void    read_plist(package_t *, FILE *);
void    append_plist(package_t *, FILE *);
int     delete_package(Boolean, package_t *, Boolean, const char *);

/* Package Database */
int     pkgdb_open(int);
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

/* List of packages functions */
lpkg_t *alloc_lpkg(const char *);
lpkg_t *find_on_queue(lpkg_head_t *, const char *);
void    free_lpkg(lpkg_t *);

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
