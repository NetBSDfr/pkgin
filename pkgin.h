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

#define KEEP 1
#define UNKEEP 0

#define DO_INST 1
#define DONT_INST 0

#define ALNUM "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define DELIMITER '|'
#define ICON_WAIT "-\\|/"
#define ICON_LEN 4

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

/*
 * The remote package hash is used by almost every operation, and is the one
 * that when tuned well provides the greatest performance benefits.  In most
 * situations the number of remote packages will be a large number, with the
 * total somewhere around the number of packages available in pkgsrc, with
 * 25,000 being a reasonable expectation in 2023.
 *
 * On the test system both the upgrade and install scenarios were tested, and
 * showed the following results (runtime and number of pkg_match() calls).
 *
 *  Size	Upgrade			Install
 *     1	19.675s	493,519,906	6.867s	230,816,916
 *    16	 8.334s	 48,824,381	1.980s	 24,875,947
 *    64	 7.106s	 26,579,645	1.422s	 14,572,242
 *   256	 6.677s	 21,023,457	1.222s	 11,999,939
 *  1024	 6.514s	 19,638,215	1.201s	 11,354,472
 *  4096	 6.643s	 19,293,278	1.097s	 11,192,549
 * 16384	 6.496s	 19,205,922	1.097s	 11,152,356
 *
 * An additional consideration to make when sizing this hash is that over time
 * the number of remote packages is likely to grow, and so we shouldn't be shy
 * about setting a reasonably large value to ensure that pkgin continues to
 * perform well in the event of considerable package growth.
 *
 * As the remote package list is only ever loaded once, there are fewer costs
 * associated with setting it to a higher value, and so for now 4096 is chosen.
 *
 * We may even want to dynamically resize this at some point based on the
 * number of remote packages, especially if its shown to provide benefits
 * against smaller (e.g. self-built) repositories.
 */
#define	REMOTE_PKG_HASH_SIZE	4096

/*
 * Both the packages and dependencies hashes depend on the number of affected
 * packages from either install or upgrade operations.  Using the pessimistic
 * test case, the following total entries were observed:
 *
 *  - "pkgin install": pkgs=5257,  deps=5
 *  - "pkgin upgrade": pkgs=13803, deps=6748
 *
 * Note of course that as the "pkgin install" operation in our test case mostly
 * handles packages that are already installed, the deps number is artificially
 * low as the dependencies will already have been handled by deps_impact() by
 * the time later packages are considered.  A normal "pkgin install" for new
 * packages will result in a higher deps number.
 *
 * For both hashes we are simply optimising for strcmp() calls which search for
 * existing hash entries to avoid duplicate entries, there are no pkg_match()
 * calls to consider.
 *
 * For the packages hash the following results were observed (runtime and
 * number of strcmp() calls) with DEPS_HASH_SIZE=1.
 *
 *  Size	Upgrade			Install
 *     1	6.675s	365,703,632	1.099s	39,903,276
 *    16	3.618s	252,966,780	0.708s	 3,593,899
 *    64	3.514s	247,323,761	0.678s	 1,763,491
 *   256	3.519s	245,913,686	0.651s	 1,307,191
 *  8192	3.366s	245,457,901	0.604s	 1,158,908
 * 16384	3.475s	245,450,585	0.637s	 1,156,550
 *
 * Any gains over 256 are marginal and so that's chosen.
 *
 * For dependencies, PKGS_HASH_SIZE was set to 256 and the results are:
 *
 *  Size	Upgrade	strcmp()
 *     1	3.395s	245,913,686
 *    16	1.380s	 16,493,696
 *    64	1.208s	  5,094,489
 *   256	1.170s	  2,163,943
 *  1024	1.207s	  1,392,635
 *  4096	1.428s	  1,394,523
 *
 * Again there are minimal gains over 256, and certainly no performance
 * increase, so for simplicity we use 256 for both values.
 */
#define	PKGS_HASH_SIZE		256
#define	DEPS_HASH_SIZE		256

/*
 * The local package hash is only of benefit for "pkgin install" operations
 * where selected remote packages need to search for matching local packages to
 * upgrade.  "pkgin upgrade" will always consider all local packages and so
 * very few, if any, additional lookups are performed.
 *
 * A relatively small hash size is all that's needed to provide reasonable
 * benefits.  Using the same install operation as described below, the runtime
 * was calculated and pkg_match() calls were counted using DTrace:
 *
 * LOCAL_PKG_HASH_SIZE=1	9.918s	398,107,802
 * LOCAL_PKG_HASH_SIZE=4	8.771s	270,646,524
 * LOCAL_PKG_HASH_SIZE=16	7.280s	238,795,130
 * LOCAL_PKG_HASH_SIZE=64	6.871s	230,865,958
 * LOCAL_PKG_HASH_SIZE=256	6.714s	228,828,534
 * LOCAL_PKG_HASH_SIZE=16384	6.639s	228,174,701
 *
 * Clearly the advantages drop off considerably over 64, and given the test
 * case is about as pessimistic as can be constructed and the vast majority of
 * installs will have orders of magnitude fewer packages, there's no reason to
 * consider higher values, especially as for some operations the local package
 * array needs to be reconstructed after pkgdb modifications which will cause
 * additional setup and teardown costs.
 */
#define	LOCAL_PKG_HASH_SIZE	64

/*
 * On the test system there were 1592 CONFLICTS entries that each upgrade
 * package needed to be compared against, however that dropped to 1036 once
 * only distinct patterns were queried.  The tests were against the original
 * number though, and used DTrace to count the total time spent in
 * pkg_conflicts():
 *
 * CONFLICTS_HASH_SIZE		Time spent (ns)		Total Runtime
 * 		     1		339029895		1.347s
 * 		     4		 65412959		1.051s
 * 		    16		 20528980		1.035s
 * 		    64		 12216525		0.999s
 * 		   256		 10009372		0.987s
 * 		  1024		  9248654		1.001s
 *
 * Marginal gains above 64, and adding DISTINCT will keep the numbers down a
 * lot.
 */
#define CONFLICTS_HASH_SIZE	64

/*
 * Action to perform.
 */
typedef enum action_t {
	ACTION_NONE,
	ACTION_INSTALL,
	ACTION_UPGRADE,
	ACTION_REFRESH,
	ACTION_REMOVE,
	ACTION_SUPERSEDED,
	ACTION_UNMET_REQ,
} action_t;

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
	struct Pkglist *ipkg;	/* Pointer to an impact Pkglist entry */
	struct Pkglist *lpkg;	/* Pointer to a local Pkglist entry */
	struct Pkglist *rpkg;	/* Pointer to a remote Pkglist entry */

	int64_t	size_pkg; /*!< installed package size (list and impact) */
	int64_t	file_size; /*!< binary package size */
	int	level; /*<! recursion level (deptree and impact) */

	int	download;	/* Binary package needs to be fetched */
	char	*pkgfs;		/* Local filename of downloaded package */
	char	*pkgurl;	/* Remote URL of package to fetch */

	char *full; /*!< full package name with version, foo-1.0 */
	char *name; /*!< package name, foo */
	char *version; /*<! package version, 1.0 */
	char *build_date; /*<! BUILD_DATE timestamp */
	char *category; /*!< package category */
	char *pkgpath; /*!< pkgsrc pkgpath */
	char *comment; /*!< package list comment */

	char **patterns;	/* DEPENDS patterns for this package */
	int patcount;		/* Number of DEPENDS patterns */
	char *replace;		/* PKGNAME of what SUPERSEDES a package */

	action_t action;	/* Action to perform */
	int skip;		/* Already processed via a different path */
	int	keep; /*!< autoremovable package ? */

	SLIST_ENTRY(Pkglist) next;
} Pkglist;

typedef SLIST_HEAD(, Pkglist) Plisthead;
typedef struct Plistnumbered {
	Plisthead	*P_Plisthead;
	int		P_count;
	int		P_type; /* 0 = local, 1 = remote */
} Plistnumbered;

/*
 * An array of  Plistheads suitable for hashed entries.
 */
typedef struct Plistarray {
	Plisthead	*head;
	int		size;
} Plistarray;

typedef struct Preflist {
	char		*pkg;
	char		*glob;
	SLIST_ENTRY(Preflist) next;
} Preflist;
typedef SLIST_HEAD(, Preflist) Preflisthead;

/*
 * Type of DEPENDS traversal.
 */
typedef enum depends_t {
	DEPENDS_LOCAL,
	DEPENDS_REMOTE,
	DEPENDS_REVERSE,
} depends_t;

extern uint8_t		verbosity;
extern uint8_t		package_version;
extern uint8_t		parsable;
extern uint8_t		pflag;
extern char		*env_repos;
extern char		**pkg_repos;
extern char  		lslimit;
extern int		l_plistcounter;
extern int		r_plistcounter;
extern Plistarray *	l_conflicthead;
extern Plisthead	l_plisthead[LOCAL_PKG_HASH_SIZE];
extern Plisthead	r_plisthead[REMOTE_PKG_HASH_SIZE];
extern FILE		*tracefp;

/* download.c*/
Sumfile		*sum_open(char *, time_t *);
int		sum_start(struct archive *, void *);
ssize_t		sum_read(struct archive *, void *, const void **);
int		sum_close(struct archive *, void *);
off_t		download_pkg(char *, FILE *, int, int);
/* summary.c */
int		update_db(int, int);
void		split_repos(void);
int		chk_repo_list(int);
/* sqlite_callbacks.c */
int		pdb_rec_list(void *, int, char **, char **);
int		record_pattern_to_array(void *, int, char **, char **);
/* depends.c */
void		get_depends(const char *, Plisthead *, depends_t);
void		get_depends_recursive(const char *, Plistarray *, depends_t);
int		show_direct_depends(const char *);
int		show_full_dep_tree(const char *);
int		show_rev_dep_tree(const char *);
/* pkglist.c */
void		init_local_pkglist(void);
void		init_remote_pkglist(void);
int		is_empty_plistarray(Plistarray *);
int		is_empty_local_pkglist(void);
int		is_empty_remote_pkglist(void);
Pkglist		*pkgname_in_local_pkglist(const char *, Plisthead *, int);
Pkglist		*pkgname_in_remote_pkglist(const char *, Plisthead *, int);
Pkglist		*pattern_in_pkglist(const char *, Plisthead *, int);
size_t		pkg_hash_entry(const char *, int);
void		free_local_pkglist(void);
void		free_remote_pkglist(void);
Pkglist		*malloc_pkglist(void);
void		free_pkglist_entry(Pkglist **);
void		free_pkglist(Plisthead **);
Plistarray	*init_array(int);
void		free_array(Plistarray *);
Plisthead	*init_head(void);
Plistnumbered	*rec_pkglist(const char *, ...);
void		list_pkgs(const char *, int);
int		search_pkg(const char *);
void		show_category(char *);
int		show_pkg_category(char *);
void		show_all_categories(void);
/* actions.c */
int		action_is_install(action_t);
int		action_is_remove(action_t);
void		do_pkg_remove(Plisthead *);
int		pkgin_remove(char **);
int		pkgin_install(char **, int, int);
char		*action_list(char *, char *);
int		pkgin_upgrade(int);
/* order.c */
Plisthead	*order_remove(Plisthead *);
Plisthead	*order_download(Plisthead *);
Plisthead	*order_install(Plisthead *);
/* impact.c */
Plisthead	*pkg_impact(char **, int *, int);
/* autoremove.c */
void	   	pkgin_autoremove(void);
void		show_pkg_keep(void);
void		show_pkg_nokeep(void);
int		pkg_keep(int, char *);
/* fsops.c */
uint64_t	fs_room(const char *);
void		clean_cache(void);
char		*read_repos(void);
/* pkg_str.c */
int		find_preferred_pkg(const char *, Pkglist **, char **);
char	   	*unique_pkg(const char *, const char *);
Pkglist		*find_remote_pkg(const char *, const char *, const char *);
Pkglist		*find_local_pkg(const char *, const char *);
int		exact_pkgfmt(const char *);
int		version_check(char *, char *);
int		pkgstrcmp(const char *, const char *);
char *		pkgname_from_pattern(const char *);
int		sort_pkg_alpha(const void *, const void *);
/* selection.c */
void		export_keep(void);
void		import_keep(int, const char *);
/* pkg_check.c */
int		pkg_met_reqs(Plisthead *);
char *		pkg_conflicts(Pkglist *);
void		show_prov_req(const char *, const char *);
/* pkg_infos.c */
int		show_pkg_info(char, char *);

/* pkg_install.c */
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
uint64_t	pkgindb_savepoint(void);
void		pkgindb_savepoint_rollback(uint64_t);
void		pkgindb_savepoint_release(uint64_t);
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
