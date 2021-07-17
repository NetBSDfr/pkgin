/*
 * Copyright (c) 2009-2015 The NetBSD Foundation, Inc.
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
 */

#ifndef _PKGIN_H
#define _PKGIN_H

#include "config.h"

/*
 * Include our copy of queue.h before nbcompat pulls in its version.
 */
#include "external/queue.h"

#ifdef HAVE_NBCOMPAT_H
#include <nbcompat.h>
#else
#include <err.h>
#endif

#include <archive.h>
#include <archive_entry.h>
#include <fetch.h>
#include <errno.h>
#include "messages.h"
#include "pkgindb.h"
#include "tools.h"
#include "external/lib.h"
#include "external/dewey.h"

#define PKG_SUMMARY "pkg_summary"
#define PKG_EXT ".tgz"
#define PKGIN_CONF PKG_SYSCONFDIR"/pkgin"
#define REPOS_FILE "repositories.conf"
#define PREF_FILE "preferred.conf"

#define LOCAL_SUMMARY 0
#define REMOTE_SUMMARY 1

#define DONOTHING -1
#define TOINSTALL 0
#define TOUPGRADE 1
#define TOREMOVE 2
#define UNMET_REQ 3
#define TOREFRESH 4

#define KEEP 1
#define UNKEEP 0

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
#define PKG_SHNOKP_CMD 13
#define PKG_SRCH_CMD 14
#define PKG_CLEAN_CMD 15
#define PKG_AUTORM_CMD 16
#define PKG_EXPORT_CMD 17
#define PKG_IMPORT_CMD 18
#define PKG_SHPROV_CMD 19
#define PKG_SHREQ_CMD 20
#define PKG_SHPKGCONT_CMD 21
#define PKG_SHPKGDESC_CMD 22
#define PKG_SHPKGBDEFS_CMD 23
#define PKG_SHCAT_CMD 24
#define PKG_SHPCAT_CMD 25
#define PKG_SHALLCAT_CMD 26
#define PKG_STATS_CMD 27
#define PKG_GINTO_CMD 255

#define DEFAULT_NO 0
#define DEFAULT_YES 1

#define TRACE(fmt...) if (tracefp != NULL) fprintf(tracefp, fmt)

/* Support various ways to get nanosecond resolution, or default to 0 */
#if HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
#define pkgin_nanotime	st_mtimespec.tv_nsec
#elif HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
#define pkgin_nanotime	st_mtim.tv_nsec
#elif HAVE_STRUCT_STAT_ST_MTIME_N
#define pkgin_nanotime	st_mtime_n
#elif HAVE_STRUCT_STAT_ST_UMTIME
#define pkgin_nanotime	st_umtime
#elif HAVE_STRUCT_STAT_ST_MTIME_USEC
#define pkgin_nanotime	st_mtime.usec
#else
#define pkgin_nanotime	0
#endif

/**
 * \struct Sumfile
 * \brief Remote pkg_summary information
 */
typedef struct Sumfile {
	fetchIO *fd;
	struct url *url;
	char buf[65536];
	off_t size;
	off_t pos;
} Sumfile;

/**
 * \struct Pkglist
 *
 * \brief Master structure for all types of package lists (SLIST)
 */
typedef struct Pkglist {
	uint8_t	type; /*!< list type (LIST, DEPTREE or IMPACT) */

	int64_t	size_pkg; /*!< installed package size (list and impact) */
	int64_t old_size_pkg; /*!< old installed package size */
	int64_t	file_size; /*!< binary package size */
	int	level; /*<! recursion level (deptree and impact) */

	int	download; /*<! binary package needs to be fetched */
	char	*pkgurl; /*<! which repository to fetch package from */

	char *full; /*!< full package name with version, foo-1.0 */
	char *name; /*!< package name, foo */
	char *old; /*!< old package if any */
	char *version; /*<! package version, 1.0 */
	char *build_date; /*<! BUILD_DATE timestamp */
	char *depend;	/*!< dewey or glob form for forward (direct)
			 * dependencies:
			 * foo>=1.0
			 * or full package name for reverse dependencies:
			 * foo-1.0
			 */
	char *category; /*!< package category */
	char *pkgpath; /*!< pkgsrc pkgpath */
	char *comment; /*!< package list comment */
	int	keep; /*!< autoremovable package ? */
	int	action; /*!< TOINSTALL or TOUPGRADE */

	SLIST_ENTRY(Pkglist) next;
} Pkglist;

typedef SLIST_HEAD(, Pkglist) Plisthead;
typedef struct Plistnumbered {
	Plisthead	*P_Plisthead;
	int		P_count;
} Plistnumbered;

typedef struct Preflist {
	char		*pkg;
	char		*glob;
	SLIST_ENTRY(Preflist) next;
} Preflist;
typedef SLIST_HEAD(, Preflist) Preflisthead;

extern uint8_t		verbosity;
extern uint8_t		package_version;
extern uint8_t		parsable;
extern uint8_t		pflag;
extern int		r_plistcounter;
extern int		l_plistcounter;
extern char		*env_repos;
extern char		**pkg_repos;
extern char  		lslimit;
extern Plisthead	r_plisthead;
extern Plisthead	l_plisthead;
extern FILE		*tracefp;

/* download.c*/
Sumfile		*sum_open(char *, time_t *);
int		sum_start(struct archive *, void *);
ssize_t		sum_read(struct archive *, void *, const void **);
int		sum_close(struct archive *, void *);
off_t		download_pkg(char *, FILE *);
/* summary.c */
int		update_db(int, char **, int);
void		split_repos(void);
int		chk_repo_list(int);
/* sqlite_callbacks.c */
int		pdb_rec_list(void *, int, char **, char **);
int		pdb_rec_depends(void *, int, char **, char **);
/* depends.c */
int		show_direct_depends(const char *);
int		show_full_dep_tree(const char *, const char *, const char *);
void 		full_dep_tree(const char *, const char *, Plisthead *);
/* pkglist.c */
void		init_global_pkglists(void);
void		free_global_pkglists(void);
Pkglist		*malloc_pkglist(void);
void		free_pkglist_entry(Pkglist **);
void		free_pkglist(Plisthead **);
Plisthead	*init_head(void);
Plistnumbered	*rec_pkglist(const char *, ...);
int		pkg_is_installed(Plisthead *, Pkglist *);
void		list_pkgs(const char *, int);
int		search_pkg(const char *);
void		show_category(char *);
int		show_pkg_category(char *);
void		show_all_categories(void);
/* actions.c */
void		do_pkg_remove(Plisthead *);
int		pkgin_remove(char **);
int		pkgin_install(char **, int, int);
char		*action_list(char *, char *);
int		pkgin_upgrade(int);
char		*read_preferred(char *);
/* order.c */
Plisthead	*order_remove(Plisthead *);
Plisthead	*order_download(Plisthead *);
Plisthead	*order_install(Plisthead *);
/* impact.c */
uint8_t		pkg_in_impact(Plisthead *, char *);
Plisthead	*pkg_impact(char **, int *);
/* autoremove.c */
void	   	pkgin_autoremove(void);
void		show_pkg_keep(void);
void		show_pkg_nokeep(void);
void		pkg_keep(int, char **);
/* fsops.c */
int		fs_has_room(const char *, int64_t);
uint64_t	fs_room(const char *);
void		clean_cache(void);
char		*read_repos(void);
/* pkg_str.c */
int		find_preferred_pkg(const char *, Pkglist **, char **);
char	   	*unique_pkg(const char *, const char *);
Pkglist		*find_pkg_match(Plisthead *, char *);
uint8_t		non_trivial_glob(char *);
char		*get_pkgname_from_depend(char *);
int		exact_pkgfmt(const char *);
char		*simple_pkg_match(Plisthead *, const char *);
int		version_check(char *, char *);
int		pkgstrcmp(const char *, const char *);
/* selection.c */
void		export_keep(void);
void		import_keep(int, const char *);
/* pkg_check.c */
int		pkg_met_reqs(Plisthead *);
int		pkg_has_conflicts(Pkglist *);
void		show_prov_req(const char *, const char *);
/* pkg_infos.c */
int		show_pkg_info(char, char *);

/* pkg_install.c */
extern char	*pkg_install_dir;
extern char	*pkg_add;
extern char	*pkg_admin;
extern char	*pkg_delete;
extern char	*pkg_info;
void		setup_pkg_install(void);

/* pkgindb.c */
#define PRIVS_PKGDB	0x1
#define PRIVS_PKGINDB	0x2
extern char	*pkgin_dbdir;
extern char	*pkgin_sqldb;
extern char	*pkgin_cache;
extern char	*pkgin_errlog;
extern char	*pkgin_sqllog;
void		setup_pkgin_dbdir(void);
uint8_t		have_privs(int);
const char *	pdb_version(void);
int		pdb_get_value(void *, int, char **, char **);
int		pkgindb_doquery(const char *,
		    int (*)(void *, int, char *[], char *[]), void *);
int		pkgindb_dovaquery(const char *, ...);
int		pkgindb_open(void);
void		pkgindb_close(void);
int		pkg_db_mtime(struct stat *);
void		pkg_db_update_mtime(struct stat *);
void		repo_record(char **);
time_t		pkg_sum_mtime(char *);
void		pkgindb_stats(void);

/* preferred.c */
void		load_preferred(void);
void		free_preferred(void);
uint8_t		chk_preferred(char *, char **);

#endif
