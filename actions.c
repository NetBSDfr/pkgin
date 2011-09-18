/* $Id: actions.c,v 1.22 2011/09/18 14:26:09 imilh Exp $ */

/*
 * Copyright (c) 2009, 2010, 2011 The NetBSD Foundation, Inc.
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
#include <time.h>

#ifndef LOCALBASE
#define LOCALBASE "/usr/pkg" /* see DISCLAIMER below */
#endif

const char			*pkgin_cache = PKGIN_CACHE;
static int			upgrade_type = UPGRADE_NONE, warn_count = 0, err_count = 0;
static uint8_t		said = 0;
FILE				*err_fp = NULL;
long int			rm_filepos = -1, in_filepos = -1;

int
check_yesno(uint8_t default_answer)
{
	const struct Answer	{
		const uint8_t	numval;
		const char		charval;
	} answer[] = { { ANSW_NO, 'n' }, { ANSW_YES, 'y' } };

	uint8_t			r, reverse_answer;
	int				c;

	if (yesflag)
		return ANSW_YES;
	else if (noflag)
		return ANSW_NO;

	reverse_answer = (default_answer == ANSW_YES) ? ANSW_NO : ANSW_YES;

	if (default_answer == answer[ANSW_YES].numval)
		printf(MSG_PROCEED_YES);
	else
		printf(MSG_PROCEED_NO);

	if ((c = getchar()) == answer[reverse_answer].charval)
		r = answer[reverse_answer].numval;
	else
		r = answer[default_answer].numval;

	/* avoid residual char */
	if (c != '\n')
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
			if (!check_yesno(DEFAULT_NO))
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

/**
 * \brief Analyse PKG_INSTALL_ERR_LOG for warnings
 */
static void
analyse_pkglog(long int filepos)
{
	FILE		*err_ro;
	char		err_line[BUFSIZ];

	if (filepos < 0)
		return;

	err_ro = fopen(PKG_INSTALL_ERR_LOG, "r");

	(void)fseek(err_ro, filepos, SEEK_SET);

	while (fgets(err_line, BUFSIZ, err_ro) != NULL) {
		if (strstr(err_line, "Warning") != NULL)
			warn_count++;
		if (strstr(err_line, "already installed") != NULL)
			err_count--;
		if (strstr(err_line, "addition failed") != NULL)
			err_count++;
	}

	fclose(err_ro);
}

/**
 * \brief Tags PKG_INSTALL_ERR_LOG with date
 */
#define DATELEN 64

#ifndef DEBUG
static void
log_tag(const char *fmt, ...)
{
	va_list		ap;
	char		log_action[BUFSIZ], now_date[DATELEN];
	struct tm	tim;
	time_t		now;

	now = time(NULL);
	tim = *(localtime(&now));

	va_start(ap, fmt);
	vsnprintf(log_action, BUFSIZ, fmt, ap);
	va_end(ap);

	(void)strftime(now_date, DATELEN, "%b %d %H:%M:%S", &tim);

	fprintf(err_fp, "---%s: %s", now_date, log_action);
	fflush(err_fp);
	
}
#endif

/* package removal */
void
do_pkg_remove(Plisthead *removehead)
{
	Pkglist *premove;

	/* send pkg_delete stderr to logfile */
	if (!verbosity && !said) {
		err_fp = freopen(PKG_INSTALL_ERR_LOG, "a", stderr);
		rm_filepos = ftell(err_fp);
		said = 1;
	}

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
#ifndef DEBUG
		log_tag(MSG_REMOVING, premove->depend);
		if (fexec(PKG_DELETE, pkgtools_flags, premove->depend, NULL)
			!= EXIT_SUCCESS)
			err_count++;
#endif
	}

	analyse_pkglog(rm_filepos);
	printf(MSG_WARNS_ERRS, warn_count, err_count);
	if (warn_count > 0 || err_count > 0)
		printf(MSG_PKG_INSTALL_LOGGING_TO, PKG_INSTALL_ERR_LOG);
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
	if (!verbosity && !said) {
		err_fp = freopen(PKG_INSTALL_ERR_LOG, "a", stderr);
		in_filepos = ftell(err_fp);
		said = 1;
	}

	printf(MSG_INSTALL_PKG);

	SLIST_FOREACH(pinstall, installhead, next) {

		/* file not available in the repository */
		if (pinstall->file_size == -1)
			continue;

		printf(MSG_INSTALLING, pinstall->depend);
		snprintf(pkgpath, BUFSIZ,
			"%s/%s%s", pkgin_cache, pinstall->depend, PKG_EXT);

#ifndef DEBUG
		log_tag(MSG_INSTALLING, pinstall->depend);
#endif

		/* are we upgrading pkg_install ? */
		if (strncmp(pinstall->depend, PKG_INSTALL,
				strlen(PKG_INSTALL)) == 0) {
			printf(MSG_UPGRADE_PKG_INSTALL, PKG_INSTALL);
			/* set temporary force flags */
			strncpy(pi_tmp_flags, "-ffu", 5);
			if (verbosity)
				/* append verbosity if requested */
				strncat(pi_tmp_flags, "v", 2);
			if (check_yesno(DEFAULT_YES)) {
#ifndef DEBUG
				fexec(PKG_ADD, pi_tmp_flags, pkgpath, NULL);
#endif
			} else
				continue;
		} else {
			/* every other package */
#ifndef DEBUG
			fexec(PKG_ADD, pkgtools_flags, pkgpath, NULL);
#endif
		}
	} /* installation loop */

	analyse_pkglog(in_filepos);
	printf(MSG_WARNS_ERRS, warn_count, err_count);
	if (warn_count > 0 || err_count > 0)
		printf(MSG_PKG_INSTALL_LOGGING_TO, PKG_INSTALL_ERR_LOG);
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

#define H_BUF 6

int
pkgin_install(char **opkgargs, uint8_t do_inst)
{
	int			installnum = 0, upgradenum = 0, removenum = 0;
	int			rc = EXIT_FAILURE;
	uint64_t   	file_size = 0, size_pkg = 0;
	Pkglist		*premove, *pinstall;
	Pkglist		*pimpact;
	Plisthead	*impacthead; /* impact head */
	Plisthead	*removehead = NULL, *installhead = NULL;
	char		**pkgargs;
	char		*toinstall = NULL, *toupgrade = NULL, *toremove = NULL;
	char		pkgpath[BUFSIZ], h_psize[H_BUF], h_fsize[H_BUF];
	struct stat	st;

	/* transform command line globs into pkgnames */
	if ((pkgargs = glob_to_pkgarg(opkgargs)) == NULL) {
		printf(MSG_NOTHING_TO_DO);
		return rc;
	}

	/* full impact list */
	if ((impacthead = pkg_impact(pkgargs)) == NULL) {
		printf(MSG_NOTHING_TO_DO);
		return rc;
	}

	/* check for required files */
	if (!pkg_met_reqs(impacthead)) {
		printf(MSG_REQT_MISSING);
		goto installend;
	}

	/* browse impact tree */
	SLIST_FOREACH(pimpact, impacthead, next) {

		/* check for conflicts */
		if (pkg_has_conflicts(pimpact))
			if (!check_yesno(DEFAULT_NO))
				goto installend;

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

		if (check_yesno(DEFAULT_YES)) {
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
		} /* check_yesno */

	} else
		printf(MSG_NOTHING_TO_INSTALL);

installend:

	XFREE(toinstall);
	XFREE(toupgrade);
	free_pkglist(&impacthead, IMPACT);
	free_pkglist(&removehead, DEPTREE);
	free_pkglist(&installhead, DEPTREE);
	free_list(pkgargs);

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

	if (SLIST_EMPTY(&l_plisthead))
		errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);

	/* act on every package passed to the command line */
	for (ppkgargs = pkgargs; *ppkgargs != NULL; ppkgargs++) {

		if ((pkgname = find_exact_pkg(&l_plisthead, *ppkgargs)) == NULL) {
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
		if (check_yesno(DEFAULT_YES)) {
			do_pkg_remove(removehead);

			update_db(LOCAL_SUMMARY, NULL);

			rc = EXIT_SUCCESS;
		} else
			rc = EXIT_FAILURE;

		analyse_pkglog(rm_filepos);
	} else {
		printf(MSG_NO_PKGS_TO_DELETE);
		rc = EXIT_SUCCESS;
	}

	free_pkglist(&removehead, DEPTREE);
	free_pkglist(&pdphead, DEPTREE);

	XFREE(todelete);

	return rc;
}

/* find closest match for packages to be upgraded */
static char *
narrow_match(char *pkgname, const char *fullpkgname)
{
	Pkglist	*pkglist;
	char	*best_match = NULL;
	unsigned int		i;
	size_t  pkglen, fullpkglen, matchlen;

	matchlen = 0;
	pkglen = strlen(pkgname);
	fullpkglen = strlen(fullpkgname);

	SLIST_FOREACH(pkglist, &r_plisthead, next) {
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
		if (SLIST_EMPTY(&l_plisthead))
			errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);
		localplisthead = &l_plisthead;
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
			free_list(pkgargs);
			/* record keep list */
			pkgargs = record_upgrades(keeplisthead);
		}

		update_db(LOCAL_SUMMARY, pkgargs);
	}

	free_list(pkgargs);

	free_pkglist(&keeplisthead, LIST);
}
