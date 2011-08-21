/* $Id: actions.c,v 1.4.2.9 2011/08/21 11:42:44 imilh Exp $ */

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

#include "pkgin.h"

#ifndef LOCALBASE
#define LOCALBASE "/usr/pkg" /* see DISCLAIMER below */
#endif

const char			*pkgin_cache = PKGIN_CACHE;
static int			upgrade_type = UPGRADE_NONE;
static uint8_t		said = 0;	

int
check_yesno(void)
{
	int c, r = 0;

	if (yesflag)
		return 1;
	else if (noflag)
		return 0;

	printf(MSG_PROCEED);

	if ((c = getchar()) == 'y')
	    	r = 1;
	else if (c == '\n' || c == EOF)
	    	return 0;

	/* avoid residual char */
	while((c = getchar()) != '\n' && c != EOF)
		continue;
	return r;
}

static void
pkg_download(Plisthead *installhead)
{
	FILE		*fp;
	Pkglist  	*pinstall;
	struct stat	st;
	Dlfile		*dlpkg;
	char		pkg[BUFSIZ], query[BUFSIZ];

	printf(MSG_DOWNLOAD_PKGS);

	SLIST_FOREACH(pinstall, installhead, next) {
		snprintf(pkg, BUFSIZ,
		    "%s/%s%s", pkgin_cache, pinstall->depend, PKG_EXT);

		/* pkg_info -X -a produces pkg_summary with empty FILE_SIZE,
		 * people could spend some time blaming on pkgin before finding
		 * what's really going on.
		 */
		if (pinstall->file_size == 0)
			printf(MSG_EMPTY_FILE_SIZE, pinstall->depend);

		/* already fully downloaded */
		if (stat(pkg, &st) == 0 && 
			st.st_size == pinstall->file_size &&
			pinstall->file_size != 0 )
		    	continue;

		umask(DEF_UMASK);
		if ((fp = fopen(pkg, "w")) == NULL)
			err(EXIT_FAILURE, MSG_ERR_OPEN, pkg);

		snprintf(query, BUFSIZ, PKG_URL, pinstall->depend);
		/* retrieve repository for package  */
		if (pkgindb_doquery(query, pdb_get_value, pkg) != 0)
			errx(EXIT_FAILURE, MSG_PKG_NO_REPO, pinstall->depend);

		strlcat(pkg, "/", sizeof(pkg));
		strlcat(pkg, pinstall->depend, sizeof(pkg));
		strlcat(pkg, PKG_EXT, sizeof(pkg));

		if ((dlpkg = download_file(pkg, NULL)) == NULL) {
			fprintf(stderr, MSG_PKG_NOT_AVAIL, pinstall->depend);
			if (!check_yesno())
				errx(EXIT_FAILURE, MSG_PKG_NOT_AVAIL,
				    pinstall->depend);
			pinstall->file_size = -1;
			fclose(fp);
			continue;
		}

		fwrite(dlpkg->buf, dlpkg->size, 1, fp);
		fclose(fp);

		XFREE(dlpkg->buf);
		XFREE(dlpkg);

	} /* download loop */

}

/* package removal */
static void
do_pkg_remove(Plisthead *removehead)
{
	Pkglist *premove;

/* send pkg_delete stderr to logfile */
#ifdef HAVE_FREOPEN
	if (!verbosity && !said) {
		(void)freopen(PKGIN_ERR_LOG, "a", stderr);
		printf(MSG_LOGGING_TO, PKGIN_ERR_LOG);
		said = 1;
	}
#endif

	SLIST_FOREACH(premove, removehead, next) {
		/* file not available in the repository */
		if (premove->file_size == -1)
			continue;

		if (premove->depend == NULL)
			/* SLIST corruption, badly installed package */
			continue;

		/* pkg_install cannot be deleted */
		if (strcmp(premove->depend, PKG_INSTALL) == 0) {
			printf(MSG_NOT_REMOVING, PKG_INSTALL);
			continue;
		}

		printf(MSG_REMOVING, premove->depend);
		fprintf(stderr, "%s %s %s\n",
			PKG_DELETE, pkgtools_flags, premove->depend);
#ifndef DEBUG
		fexec(PKG_DELETE, pkgtools_flags, premove->depend, NULL);
#endif
	}
}

/**
 * \fn do_pkg_install
 *
 * package installation. Don't rely on pkg_add's ability to fetch and
 * install as we want to keep control on packages installation order.
 * Besides, pkg_add cannot be used to install an "older" package remotely
 * i.e. apache 1.3
 */
static void
do_pkg_install(Plisthead *installhead)
{
	Pkglist		*pinstall;
	char		pkgpath[BUFSIZ];
	char		pi_tmp_flags[5]; /* tmp force flags for pkg_install */

/* send pkg_add stderr to logfile */
#ifdef HAVE_FREOPEN
	if (!verbosity && !said) {
		(void)freopen(PKGIN_ERR_LOG, "a", stderr);
		printf(MSG_LOGGING_TO, PKGIN_ERR_LOG);
		said = 1;
	}
#endif

	printf(MSG_INSTALL_PKG);

	SLIST_FOREACH(pinstall, installhead, next) {

		/* file not available in the repository */
		if (pinstall->file_size == -1)
			continue;

		printf(MSG_INSTALLING, pinstall->depend);
		snprintf(pkgpath, BUFSIZ,
			"%s/%s%s", pkgin_cache, pinstall->depend, PKG_EXT);

		/* are we upgrading pkg_install ? */
		if (strncmp(pinstall->depend, PKG_INSTALL,
				strlen(PKG_INSTALL)) == 0) {
			printf(MSG_UPGRADE_PKG_INSTALL, PKG_INSTALL);
			/* set temporary force flags */
			strncpy(pi_tmp_flags, "-ffu", 5);
			if (verbosity)
				/* append verbosity if requested */
				strncat(pi_tmp_flags, "v", 2);
			if (check_yesno()) {
				fprintf(stderr, "%s %s %s\n", PKG_ADD, pi_tmp_flags, pkgpath);
#ifndef DEBUG
				fexec(PKG_ADD, pi_tmp_flags, pkgpath, NULL);
#endif
			} else
				continue;
		} else {
			/* every other package */
			fprintf(stderr, "%s %s %s\n", PKG_ADD, pkgtools_flags, pkgpath);
#ifndef DEBUG
			fexec(PKG_ADD, pkgtools_flags, pkgpath, NULL);
#endif
		}
	} /* installation loop */
}

/* build the output line */
char *
action_list(char *flatlist, char *str)
{
	int		newsize;
	char	*newlist = NULL;

	if (flatlist == NULL)
		XSTRDUP(newlist, str);
	else {
		if (str == NULL)
			return flatlist;

		newsize = strlen(str) + strlen(flatlist) + 2;
		newlist = realloc(flatlist, newsize * sizeof(char));
		strlcat(newlist, " ", newsize);
		strlcat(newlist, str, newsize);
	}

	return newlist;
}

/* find required files (REQUIRES) from PROVIDES or filename */
static int
pkg_met_reqs(Plisthead *impacthead)
{
	int			met_reqs = 1, foundreq;
	Pkglist		*pimpact, *requires;
	Plisthead	*requireshead;
#ifdef CHECK_PROVIDES
	Pkglist		*impactprov, *provides;
	Plisthead	*provideshead;
#endif
	struct stat	sb;
	char		query[BUFSIZ];

	/* first, parse impact list */
	SLIST_FOREACH(pimpact, impacthead, next) {
		/* retreive requires list for package */
		snprintf(query, BUFSIZ, GET_REQUIRES_QUERY, pimpact->full);
		requireshead = rec_pkglist(query);

		if (requireshead == NULL) /* empty requires list (very unlikely) */
			continue;

		/* parse requires list */
		SLIST_FOREACH(requires, requireshead, next) {

			foundreq = 0;

			/* for performance sake, first check basesys */
			if ((strncmp(requires->full, LOCALBASE,
				    sizeof(LOCALBASE) - 1)) != 0) {
				if (stat(requires->full, &sb) < 0) {
					printf(MSG_REQT_NOT_PRESENT,
						requires->full, pimpact->full);

					met_reqs = 0;
				}
				/* was a basysfile, no need to check PROVIDES */
				continue;
			}
			/* FIXME: the code below actually works, but there's no
			 * point losing performances when some REQUIRES do not match
			 * PROVIDES in pkg_summary(5). This is a known issue and will
			 * hopefuly be fixed.
			 */
#ifndef CHECK_PROVIDES
			continue;
#else
			/* search what local packages provide */
			provideshead = rec_pkglist(LOCAL_PROVIDES);
			SLIST_FOREACH(provides, provideshead, next) {
				if (strncmp(provides->full,
						requires->full,
						strlen(requires->full)) == 0) {

					foundreq = 1;

					/* found, no need to go further*/
					break;
				} /* match */
			} /* SLIST_FOREACH LOCAL_PROVIDES */
			free_pkglist(provideshead, LIST);

			/* REQUIRES was not found on local packages, try impact list */
			if (!foundreq) {
				/* re-parse impact list to retreive PROVIDES */
				SLIST_FOREACH(impactprov, impacthead, next) {
					snprintf(query, BUFSIZ, GET_PROVIDES_QUERY,
						impactprov->full);
					provideshead = rec_pkglist(query);

					if (provideshead == NULL)
						continue;

					/* then parse provides list for every package */
					SLIST_FOREACH(provides, provideshead, next) {
						if (strncmp(provides->full,
								requires->full,
								strlen(requires->full)) == 0) {

							foundreq = 1;

							/* found, no need to go further
							   return to impactprov list */
							break;
						} /* match */
					}
					free_pkglist(provideshead, LIST);

					if (foundreq) /* exit impactprov list loop */
						break;

				} /* SLIST_NEXT impactprov */

			} /* if (!foundreq) LOCAL_PROVIDES -> impact list */

			/* FIXME: BIG FAT DISCLAIMER
			 * as of 04/2009, some packages described in pkg_summary
			 * have unmet REQUIRES. This is a known bug that makes the
			 * PROVIDES untrustable and some packages uninstallable.
			 * foundreq is forced to 1 for now for every REQUIRES
			 * matching LOCALBASE, which is hardcoded to "/usr/pkg"
			 */
			if (!foundreq) {
				printf(MSG_REQT_NOT_PRESENT_BUT, requires->full);

				foundreq = 1;
			}
#endif
		} /* SLIST_FOREACH requires */
		free_pkglist(requireshead, LIST);
	} /* 1st impact SLIST_FOREACH */

	return met_reqs;
}

/* check for conflicts and if needed files are present */
static int
pkg_has_conflicts(Plisthead *conflictshead, Pkglist *pimpact)
{
	int			has_conflicts = 0;
	Pkglist		*conflicts; /* SLIST conflicts pointer */
	char		*conflict_pkg, query[BUFSIZ];

	if (conflictshead == NULL)
		return 0;

	/* check conflicts */
	SLIST_FOREACH(conflicts, conflictshead, next) {
		if (pkg_match(conflicts->full, pimpact->full)) {

			/* got a conflict, retrieve conflicting local package */
			snprintf(query, BUFSIZ,
				GET_CONFLICT_QUERY, conflicts->full);

			XMALLOC(conflict_pkg, BUFSIZ * sizeof(char));
			if (pkgindb_doquery(query,
					pdb_get_value, conflict_pkg) == 0)

				printf(MSG_CONFLICT_PKG,
					pimpact->full, conflict_pkg);

			XFREE(conflict_pkg);

			has_conflicts = 1;
		} /* match conflict */
	} /* SLIST_FOREACH conflicts */

	return has_conflicts;
}

#define H_BUF 6

int
pkgin_install(char **pkgargs, uint8_t do_inst)
{
	int			installnum = 0, upgradenum = 0, removenum = 0;
	int			rc = EXIT_FAILURE;
	uint64_t   	file_size = 0, size_pkg = 0;
	Pkglist		*premove, *pinstall; /* not a Pkgdeptree, just for ease */
	Pkglist		*pimpact;
	Plisthead	*impacthead; /* impact head */
	Plisthead	*removehead = NULL, *installhead = NULL;
	Plisthead	*conflictshead; /* conflicts head */
	char		*toinstall = NULL, *toupgrade = NULL, *toremove = NULL;
	char		pkgpath[BUFSIZ], h_psize[H_BUF], h_fsize[H_BUF];
	struct stat	st;

	/* full impact list */
	if ((impacthead = pkg_impact(pkgargs)) == NULL) {
		printf(MSG_NOTHING_TO_DO);
		return rc;
	}

	/* check for required files */
	if (!pkg_met_reqs(impacthead)) {
		printf(MSG_REQT_MISSING);
		return rc;
	}

	/* conflicts list */
	conflictshead = rec_pkglist(LOCAL_CONFLICTS);

	/* browse impact tree */
	SLIST_FOREACH(pimpact, impacthead, next) {

		/* check for conflicts */
		if (pkg_has_conflicts(conflictshead, pimpact)) {
			if (!check_yesno()) {
				free_pkglist(impacthead, IMPACT);

				return rc;
			}
		}

		snprintf(pkgpath, BUFSIZ, "%s/%s%s",
			pkgin_cache, pimpact->full, PKG_EXT);

		/* if package is not already downloaded or size mismatch, d/l it */
		if (stat(pkgpath, &st) < 0 || st.st_size != pimpact->file_size)
			file_size += pimpact->file_size;

		size_pkg += pimpact->size_pkg;

		switch (pimpact->action) {
		case TOUPGRADE:
			upgradenum++;
			installnum++;
			break;

		case TOINSTALL:
			installnum++;
			break;

		case TOREMOVE:
			removenum++;
			break;
		}
	}
	/* free conflicts list */
	free_pkglist(conflictshead, LIST);

	(void)humanize_number(h_fsize, H_BUF, (int64_t)file_size, "",
		HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
	(void)humanize_number(h_psize, H_BUF, (int64_t)size_pkg, "",
		HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);

	/* check disk space */
	if (!fs_has_room(pkgin_cache, (int64_t)file_size))
		errx(EXIT_FAILURE, MSG_NO_CACHE_SPACE, pkgin_cache);
	if (!fs_has_room(LOCALBASE, (int64_t)size_pkg))
		errx(EXIT_FAILURE, MSG_NO_INSTALL_SPACE, LOCALBASE);

	printf("\n");

	if (upgradenum > 0) {
		/* record ordered remove list before upgrade */
		removehead = order_upgrade_remove(impacthead);

		SLIST_FOREACH(premove, removehead, next) {
			if (premove->computed == TOUPGRADE) {
				toupgrade = action_list(toupgrade, premove->depend);
#ifdef DEBUG
				printf("package: %s - level: %d\n",
					premove->depend, premove->level);
#endif
			}
		}
		printf(MSG_PKGS_TO_UPGRADE, upgradenum, toupgrade);
		printf("\n");

		if (removenum > 0) {
			SLIST_FOREACH(premove, removehead, next) {
				if (premove->computed == TOREMOVE) {
					toremove = action_list(toremove, premove->depend);
#ifdef DEBUG
					printf("package: %s - level: %d\n",
						premove->depend, premove->level);
#endif
				}
			}
			printf(MSG_PKGS_TO_REMOVE, removenum, toremove);
			printf("\n");
		}

	} else
		printf(MSG_NOTHING_TO_UPGRADE);

	if (installnum > 0) {
		/* record ordered install list */
		installhead = order_install(impacthead);

		SLIST_FOREACH(pinstall, installhead, next) {
			toinstall = action_list(toinstall, pinstall->depend);
#ifdef DEBUG
			printf("package: %s - level: %d\n",
				pinstall->depend, pinstall->level);
#endif
		}

		printf(MSG_PKGS_TO_INSTALL, installnum, toinstall, h_fsize, h_psize);
		printf("\n");

		if (check_yesno()) {
			/* before erasing anything, download packages */
			pkg_download(installhead);

			if (do_inst) { /* real install, not a simple download */
				/* if there was upgrades, first remove old packages */
				if (upgradenum > 0) {
					printf(MSG_RM_UPGRADE_PKGS);
					do_pkg_remove(removehead);
				}
				/* then pass ordered install list */
				do_pkg_install(installhead);

				/* pure install, not called by pkgin_upgrade */
				if (upgrade_type == UPGRADE_NONE)
					update_db(LOCAL_SUMMARY, pkgargs);
				
				rc = EXIT_SUCCESS;
			}
		}
	} else
		printf(MSG_NOTHING_TO_INSTALL);


	XFREE(toinstall);
	XFREE(toupgrade);
	free_pkglist(impacthead, IMPACT);
	free_pkglist(removehead, DEPTREE);
	free_pkglist(installhead, DEPTREE);

	return rc;
}

int
pkgin_remove(char **pkgargs)
{
	int			deletenum = 0, exists, rc;
	Plisthead	*pdphead, *removehead;
	Pkglist		*pdp;
	char   		*todelete = NULL, **ppkgargs, *pkgname, *ppkg;

	pdphead = init_head();

	if (l_plisthead == NULL)
		errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);

	/* act on every package passed to the command line */
	for (ppkgargs = pkgargs; *ppkgargs != NULL; ppkgargs++) {

		if ((pkgname = find_exact_pkg(l_plisthead, *ppkgargs)) == NULL) {
			printf(MSG_PKG_NOT_INSTALLED, *ppkgargs);
			continue;
		}
		XSTRDUP(ppkg, pkgname);
		trunc_str(ppkg, '-', STR_BACKWARD);

		/* record full reverse dependency list for package */
		full_dep_tree(ppkg, LOCAL_REVERSE_DEPS, pdphead);

		XFREE(ppkg);

		exists = 0;
		/* check if package have already been recorded */
		SLIST_FOREACH(pdp, pdphead, next) {
			if (strncmp(pdp->depend, pkgname,
					strlen(pdp->depend)) == 0) {
				exists = 1;
				break;
			}
		}

		if (exists) {
			XFREE(pkgname);
			continue; /* next pkgarg */
		}

		/* add package itself */
		pdp = malloc_pkglist(DEPTREE);

		pdp->depend = pkgname;

		if (SLIST_EMPTY(pdphead))
			/* identify unique package, don't cut it when ordering */
			pdp->level = -1;
		else
			pdp->level = 0;

		XSTRDUP(pdp->name, pdp->depend);
		trunc_str(pdp->name, '-', STR_BACKWARD);

		SLIST_INSERT_HEAD(pdphead, pdp, next);
	} /* for pkgargs */

	/* order remove list */
	removehead = order_remove(pdphead);

	SLIST_FOREACH(pdp, removehead, next) {
		deletenum++;
		todelete = action_list(todelete, pdp->depend);
	}

	if (todelete != NULL) {
		printf(MSG_PKGS_TO_DELETE, deletenum, todelete);
		if (check_yesno()) {
			do_pkg_remove(removehead);

			update_db(LOCAL_SUMMARY, NULL);

			rc = EXIT_SUCCESS;
		} else
			rc = EXIT_FAILURE;

	} else {
		printf(MSG_NO_PKGS_TO_DELETE);
		rc = EXIT_SUCCESS;
	}

	free_pkglist(removehead, DEPTREE);
	free_pkglist(pdphead, DEPTREE);

	XFREE(todelete);

	return rc;
}

/* find closest match for packages to be upgraded */
static char *
narrow_match(char *pkgname, const char *fullpkgname)
{
	Pkglist	*pkglist;
	char	*best_match = NULL;
	int		pkglen, fullpkglen, i, matchlen = 0;

	pkglen = strlen(pkgname);
	fullpkglen = strlen(fullpkgname);

	SLIST_FOREACH(pkglist, r_plisthead, next) {
		if (strlen(pkglist->name) == pkglen &&
			strncmp(pkgname, pkglist->name, pkglen) == 0) {

			for (i = 0;
				 i < fullpkglen && fullpkgname[i] == pkglist->full[i];
				i++);

			if (i > matchlen) {
				matchlen = i;
				XSTRDUP(best_match, pkglist->full);
			}

		}
	} /* SLIST_FOREACH remoteplisthead */
	XFREE(pkgname);

	return best_match;
}

static char **
record_upgrades(Plisthead *plisthead)
{
	Pkglist	*pkglist;
	int		count = 0;
	char	**pkgargs;

	SLIST_FOREACH(pkglist, plisthead, next)
		count++;

	XMALLOC(pkgargs, (count + 2) * sizeof(char *));

	count = 0;
	SLIST_FOREACH(pkglist, plisthead, next) {
		XSTRDUP(pkgargs[count], pkglist->name);

		pkgargs[count] = narrow_match(pkgargs[count], pkglist->full);

		if (pkgargs[count] == NULL)
			continue;

		count++;
	}
	pkgargs[count] = NULL;

	return pkgargs;
}

void
pkgin_upgrade(int uptype)
{
	Plisthead	*keeplisthead, *localplisthead;
	char		**pkgargs;

	/* used for pkgin_install not to update database, this is done below */
	upgrade_type = uptype;

	/* record keepable packages */
	if ((keeplisthead = rec_pkglist(KEEP_LOCAL_PKGS)) == NULL)
		errx(EXIT_FAILURE, MSG_EMPTY_KEEP_LIST);

	/* upgrade all packages, not only keepables */
	if (uptype == UPGRADE_ALL) {
		if (l_plisthead == NULL)
			errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);
		localplisthead = l_plisthead;
	} else
		/* upgrade only keepables packages */
		localplisthead = keeplisthead;

	pkgargs = record_upgrades(localplisthead);

	if (pkgin_install(pkgargs, DO_INST) == EXIT_SUCCESS) {
		/*
		 * full upgrade, we need to record keep-packages
		 * in order to restore them
		 */
		if (uptype == UPGRADE_ALL) {

			free_pkglist(localplisthead, LIST);
			free_list(pkgargs);
			/* record keep list */
			pkgargs = record_upgrades(keeplisthead);
		}

		update_db(LOCAL_SUMMARY, pkgargs);
	}

	free_list(pkgargs);

	free_pkglist(keeplisthead, LIST);
}
