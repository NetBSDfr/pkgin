/* $Id: pkgin.h,v 1.3.2.11 2011/08/19 08:25:35 imilh Exp $ */

/*
 * Copyright (c) 2009, 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emile "iMil" Heitor <imil@NetBSD.org> .
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _PKGIN_H
#define _PKGIN_H

#if HAVE_CONFIG_H
#include "config.h"
#endif
#if HAVE_SYS_QUEUE_H
#include <sys/queue.h>
#endif

#include <fetch.h>
#include "messages.h"
#include "pkgindb.h"
#include "tools.h"
#include "lib.h"
#include "dewey.h"

#ifndef PKGTOOLS
#define PKGTOOLS "/usr/sbin"
#endif
#define PKG_DELETE PKGTOOLS"/pkg_delete"
#define PKG_ADD PKGTOOLS"/pkg_add"

#define PKG_SUMMARY "pkg_summary"
#define PKGIN_SQL_LOG PKGIN_DB"/sql.log"
#define PKGIN_ERR_LOG PKGIN_DB"/err.log"
#define PKGIN_CACHE PKGIN_DB"/cache"
#define PKG_EXT ".tgz"
#define PKGIN_CONF PKG_SYSCONFDIR"/pkgin"
#define REPOS_FILE "repositories.conf"
#define PKG_INSTALL "pkg_install"

#define LOCAL_SUMMARY 0
#define REMOTE_SUMMARY 1

#define DONOTHING -1
#define TOINSTALL 0
#define TOUPGRADE 1
#define TOREMOVE 2

#define KEEP 1
#define UNKEEP 0

#define UPGRADE_KEEP 0
#define UPGRADE_ALL 1
#define UPGRADE_NONE -1

#define DO_INST 1
#define DONT_INST 0

#define ALNUM "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define DELIMITER '|'
#define ICON_WAIT "-\\|/"

#define PKG_LLIST_CMD 0
#define PKG_RLIST_CMD 1
#define PKG_INST_CMD 2
#define PKG_UPDT_CMD 3
#define PKG_REMV_CMD 4
#define PKG_UPGRD_CMD 5
#define PKG_FUPGRD_CMD 6
#define PKG_SHFDP_CMD 7
#define PKG_SHRDP_CMD 8
#define PKG_SHDDP_CMD 9
#define PKG_KEEP_CMD 10
#define PKG_UNKEEP_CMD 11
#define PKG_SHKP_CMD 12
#define PKG_SRCH_CMD 13
#define PKG_CLEAN_CMD 14
#define PKG_AUTORM_CMD 15

#define PKG_EQUAL '='
#define PKG_GREATER '>'
#define PKG_LESSER '<'

typedef struct Dlfile {
	char *buf;
	size_t size;
} Dlfile;

/**
 * \struct Deptree
 * \brief Package dependency tree
 */
typedef struct Deptree {
	int		computed; /*!< recursion memory */
	int		keep; /*!< autoremovable package ? */
} Deptree;

/**
 * \struct Impact
 * \brief Impact list
 */
typedef struct Pkgimpact {
	int		action; /*!< TOINSTALL or TOUPGRADE */
	char	*old; /*!< package to upgrade: perl-5.8 */
} Impact;

/**
 * \struct Pkgname
 * \brief Various forms of package name
 */
typedef struct Pkglist {
	uint8_t	type; /*!< list type */

	int64_t	size_pkg; /*!< installed package size (list and impact) */
	int64_t	file_size; /*!< binary package size */
	int		level; /*<! recursion level (deptree and impact) */

	char *full; /*!< full package name with version, foo-1.0 */
	char *name; /*!< package name, foo */
	char *version; /*<! package version, 1.0 */
	char *depend; /*!< dewey or glob form for forward (direct) dependencies
					foo>=1.0, or full package name for reverse dependencies */
	union {
		char		*comment; /*!< package list comment */
		Deptree		deptree; /*<! dependency tree informations */
		Impact		impact; /*<! impact list informations */
	} p_un;

	SLIST_ENTRY(Pkglist) next;
} Pkglist;

#define comment  	p_un.comment
#define computed	p_un.deptree.computed
#define keep		p_un.deptree.keep
#define old		   	p_un.impact.old
#define action   	p_un.impact.action

#define LIST		0
#define DEPTREE		1
#define IMPACT		2

typedef SLIST_HEAD(, Pkglist) Plisthead;

extern uint8_t 		yesflag;
extern uint8_t 		noflag;
extern uint8_t 		force_update;
extern uint8_t 		force_reinstall;
extern uint8_t		verbosity;
extern uint8_t		package_version;
extern char			**pkg_repos;
extern const char	*pkgin_cache;
extern char  		lslimit;
extern char			pkgtools_flags[];

/* download.c*/
Dlfile		*download_file(char *, time_t *);
/* summary.c */
void		update_db(int, char **);
/* sqlite_callbacks.c */
int pdb_rec_list(void *, int, char **, char **);
int pdb_rec_depends(void *, int, char **, char **);
/* depends.c */
char 		*match_dep_ext(char *, const char *);
void		show_direct_depends(const char *);
void		show_full_dep_tree(const char *, const char *, const char *);
void 		full_dep_tree(const char *pkgname, const char *depquery,
	Plisthead	*pdphead);
/* pkglist.c */
Pkglist		*malloc_pkglist(uint8_t);
void		free_pkglist(Plisthead *, uint8_t);
void		list_pkgs(const char *, int);
void		search_pkg(const char *);
int			count_samepkg(Plisthead *, const char *);
Pkglist		*map_pkg_to_dep(Plisthead *, char *);
Plisthead	*rec_pkglist(const char *);
/* actions.c */
int			check_yesno(void);
int			pkgin_remove(char **);
int			pkgin_install(char **, uint8_t);
char		*action_list(char *, char *);
void		pkgin_upgrade(int);
/* order.c */
Plisthead	*order_remove(Plisthead *);
Plisthead	*order_upgrade_remove(Plisthead *);
Plisthead	*order_install(Plisthead *);
/* impact.c */
Plisthead	*pkg_impact(char **);
/* autoremove.c */
void	   	pkgin_autoremove(void);
void		show_pkg_keep(void);
void		pkg_keep(int, char **);
/* fsops.c */
int			fs_has_room(const char *, int64_t);
void		clean_cache(void);
void		create_dirs(void);
char		*read_repos(void);
/* pkg_str.c */
char		*get_pkgname_from_depend(char *);
int			exact_pkgfmt(const char *);
char		*find_exact_pkg(Plisthead *, const char *);
int			version_check(char *, char *);

#endif
