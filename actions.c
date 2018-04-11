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

#include "pkgin.h"
#include <time.h>

#ifndef LOCALBASE
#define LOCALBASE "/usr/pkg" /* see DISCLAIMER below */
#endif

static int	upgrade_type = UPGRADE_NONE, warn_count = 0, err_count = 0;
static uint8_t	said = 0;
FILE		*err_fp = NULL;
long int	rm_filepos = -1, in_filepos = -1;
char		pkgtools_flags[5];

#ifndef DEBUG
static char *
verb_flag(const char *flags)
{
	strcpy(pkgtools_flags, flags);

	if (verbosity)
		strlcat(pkgtools_flags, "v", 1);

	return pkgtools_flags;
}
#endif

static int
pkg_download(Plisthead *installhead)
{
	FILE		*fp;
	Pkglist  	*pinstall;
	struct stat	st;
	char		pkg_fs[BUFSIZ], pkg_url[BUFSIZ], query[BUFSIZ];
	ssize_t		size;
	int		rc = EXIT_SUCCESS;

	printf(MSG_DOWNLOAD_PKGS);

	SLIST_FOREACH(pinstall, installhead, next) {
		snprintf(pkg_fs, BUFSIZ,
			"%s/%s%s", pkgin_cache, pinstall->depend, PKG_EXT);

		/* pkg_info -X -a produces pkg_summary with empty FILE_SIZE,
		 * people could spend some time blaming on pkgin before finding
		 * what's really going on.
		 */
		if (pinstall->file_size == 0)
			printf(MSG_EMPTY_FILE_SIZE, pinstall->depend);

		/* already fully downloaded */
		if (stat(pkg_fs, &st) == 0 && 
			st.st_size == pinstall->file_size &&
			pinstall->file_size != 0 )
			continue;

		snprintf(query, BUFSIZ, PKG_URL, pinstall->depend);
		/* retrieve repository for package  */
		if (pkgindb_doquery(query, pdb_get_value, pkg_url) != 0)
			errx(EXIT_FAILURE, MSG_PKG_NO_REPO, pinstall->depend);

		strlcat(pkg_url, "/", sizeof(pkg_url));
		strlcat(pkg_url, pinstall->depend, sizeof(pkg_url));
		strlcat(pkg_url, PKG_EXT, sizeof(pkg_url));

		/* if pkg's repo URL is file://, just symlink */
		if (strncmp(pkg_url, SCHEME_FILE, strlen(SCHEME_FILE)) == 0) {
			(void)unlink(pkg_fs);
			if (symlink(&pkg_url[strlen(SCHEME_FILE) + 3],
				pkg_fs) < 0)
				errx(EXIT_FAILURE, MSG_SYMLINK_FAILED, pkg_fs);
			printf(MSG_SYMLINKING_PKG, pkg_url);
			continue;
		}

		umask(DEF_UMASK);
		if ((fp = fopen(pkg_fs, "w")) == NULL)
			err(EXIT_FAILURE, MSG_ERR_OPEN, pkg_fs);

		if ((size = download_pkg(pkg_url, fp)) == -1) {
			fprintf(stderr, MSG_PKG_NOT_AVAIL, pinstall->depend);
			rc = EXIT_FAILURE;

			if (!check_yesno(DEFAULT_NO))
				errx(EXIT_FAILURE, MSG_PKG_NOT_AVAIL,
				pinstall->depend);

			pinstall->file_size = -1;
			fclose(fp);
			continue;
		}

		fclose(fp);
		if (size != pinstall->file_size) {
			(void)unlink(pkg_fs);
			errx(EXIT_FAILURE, "download mismatch: %s", pkg_fs);
		}
	} /* download loop */

	return rc;
}

/**
 * \brief Analyse pkgin_errlog for warnings
 */
static void
analyse_pkglog(long int filepos)
{
	FILE	*err_ro;
	char	err_line[BUFSIZ];

	if (filepos < 0)
		return;

	err_ro = fopen(pkgin_errlog, "r");

	(void)fseek(err_ro, filepos, SEEK_SET);

	while (fgets(err_line, BUFSIZ, err_ro) != NULL) {
		/* Warning: [...] was built for a platform */
		if (strstr(err_line, "Warning") != NULL)
			warn_count++;
		/* 1 package addition failed */
		if (strstr(err_line, "addition failed") != NULL)
			err_count++;
		/* Can't install dependency */
		if (strstr(err_line, "an\'t install") != NULL)
			err_count++;
		/* unable to verify signature */
		if (strstr(err_line, "unable to verify signature") != NULL)
			err_count++;
	}

	fclose(err_ro);
}

/**
 * \brief Tags pkgin_errlog with date
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

static void
open_pi_log(void)
{
	if (!verbosity && !said) {
		if ((err_fp = fopen(pkgin_errlog, "a")) == NULL) {
 			fprintf(stderr, MSG_CANT_OPEN_WRITE,
				pkgin_errlog);
			exit(EXIT_FAILURE);
		}

		dup2(fileno(err_fp), STDERR_FILENO);

		rm_filepos = ftell(err_fp);
		said = 1;
	}
}

static void
close_pi_log(void)
{
	if (!verbosity) {
		analyse_pkglog(rm_filepos);
		printf(MSG_WARNS_ERRS, warn_count, err_count);
		if (warn_count > 0 || err_count > 0)
			printf(MSG_PKG_INSTALL_LOGGING_TO, pkgin_errlog);
	}
}

/* package removal */
void
do_pkg_remove(Plisthead *removehead)
{
	Pkglist *premove;

	/* send pkg_delete stderr to logfile */
	open_pi_log();

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
		if (!verbosity)
			log_tag(MSG_REMOVING, premove->depend);
		if (fexec(pkg_delete, verb_flag("-f"), premove->depend, NULL)
			!= EXIT_SUCCESS)
			err_count++;
#endif
	}

	close_pi_log();
}

/**
 * \fn do_pkg_install
 *
 * package installation. Don't rely on pkg_add's ability to fetch and
 * install as we want to keep control on packages installation order.
 * Besides, pkg_add cannot be used to install an "older" package remotely
 * i.e. apache 1.3
 */
static int
do_pkg_install(Plisthead *installhead)
{
	int		rc = EXIT_SUCCESS;
	Pkglist		*pinstall;
	char		pkgpath[BUFSIZ], preserve[BUFSIZ];
#ifndef DEBUG
	char		*pflags;
#endif

	/* send pkg_add stderr to logfile */
	open_pi_log();

	printf(MSG_INSTALL_PKG);

	SLIST_FOREACH(pinstall, installhead, next) {

		/* file not available in the repository */
		if (pinstall->file_size == -1)
			continue;

		printf(MSG_INSTALLING, pinstall->depend);
		snprintf(pkgpath, BUFSIZ,
			"%s/%s%s", pkgin_cache, pinstall->depend, PKG_EXT);

#ifndef DEBUG
		if (!verbosity)
			log_tag(MSG_INSTALLING, pinstall->depend);
#endif
		/* there was a previous version, record +PRESERVE path */
		if (pinstall->old != NULL)
			snprintf(preserve, BUFSIZ, "%s/%s/%s",
				pkgdb_get_dir(), pinstall->old, PRESERVE_FNAME);

		/* are we upgrading pkg_install ? */
		if (pi_upgrade) { /* set in order.c */
			/* 1st item on the list, reset the flag */
			pi_upgrade = 0;
			printf(MSG_UPGRADE_PKG_INSTALL, PKG_INSTALL);
			if (!check_yesno(DEFAULT_YES))
				continue;
		}

#ifndef DEBUG
		/* is the package marked as +PRESERVE ? */
		if (pinstall->old != NULL && access(preserve, F_OK) != -1)
			/* set temporary force flags */
			/* append verbosity if requested */
			pflags = verb_flag("-ffu");
		else
			/* every other package */
			pflags = verb_flag("-D");

		if (fexec(pkg_add, pflags, pkgpath, NULL) == EXIT_FAILURE)
			rc = EXIT_FAILURE;
#endif
	} /* installation loop */

	close_pi_log();

	return rc;
}

/* build the output line */
char *
action_list(char *flatlist, char *str)
{
	int	newsize;
	char	*newlist = NULL;

	if (flatlist == NULL) {
		newsize = strlen(str) + 2;
		newlist = xmalloc(newsize * sizeof(char));
		snprintf(newlist, newsize, "\n%s", str);
	} else {
		if (str == NULL)
			return flatlist;

		newsize = strlen(str) + strlen(flatlist) + 2;
		newlist = realloc(flatlist, newsize * sizeof(char));
		strlcat(newlist, noflag ? "\n" : " ", newsize);
		strlcat(newlist, str, newsize);
	}

	return newlist;
}

#define H_BUF 6

int
pkgin_install(char **opkgargs, uint8_t do_inst)
{
	int		installnum = 0, upgradenum = 0, removenum = 0;
	int		rc = EXIT_SUCCESS;
	int		privsreqd = PRIVS_PKGINDB;
	uint64_t	file_size = 0, free_space;
	int64_t		size_pkg = 0;
	Pkglist		*premove, *pinstall;
	Pkglist		*pimpact;
	Plisthead	*impacthead; /* impact head */
	Plisthead	*removehead = NULL, *installhead = NULL;
	char		**pkgargs;
	char		*toinstall = NULL, *toupgrade = NULL, *toremove = NULL;
	char		*unmet_reqs = NULL;
	char		pkgpath[BUFSIZ], pkgurl[BUFSIZ], query[BUFSIZ];
	char		h_psize[H_BUF], h_fsize[H_BUF], h_free[H_BUF];
	struct		stat st;

	/* transform command line globs into pkgnames */
	if ((pkgargs = glob_to_pkgarg(opkgargs, &rc)) == NULL) {
		printf(MSG_NOTHING_TO_DO);
		return rc;
	}

	if (do_inst)
		privsreqd |= PRIVS_PKGDB;

	if (!have_privs(privsreqd))
		errx(EXIT_FAILURE, MSG_DONT_HAVE_RIGHTS);

	/*
	 * Perform an explicit summary update to avoid download mismatches
	 * if the repository has been recently updated.
	 */
	(void)update_db(REMOTE_SUMMARY, NULL, 0);

	/* full impact list */
	if ((impacthead = pkg_impact(pkgargs, &rc)) == NULL) {
		printf(MSG_NOTHING_TO_DO);
		free_list(pkgargs);
		return rc;
	}

	/* check for required files */
	if (!pkg_met_reqs(impacthead))
		SLIST_FOREACH(pimpact, impacthead, next)
			if (pimpact->action == UNMET_REQ)
				unmet_reqs =
					action_list(unmet_reqs, pimpact->full);

	/* browse impact tree */
	SLIST_FOREACH(pimpact, impacthead, next) {

		/* check for conflicts */
		if (pkg_has_conflicts(pimpact))
			if (!check_yesno(DEFAULT_NO))
				goto installend;

		snprintf(pkgpath, BUFSIZ, "%s/%s%s",
			pkgin_cache, pimpact->full, PKG_EXT);

		snprintf(query, BUFSIZ, PKG_URL, pimpact->full);
		/* retrieve repository for package  */
		if (pkgindb_doquery(query, pdb_get_value, pkgurl) != 0)
			errx(EXIT_FAILURE, MSG_PKG_NO_REPO, pimpact->full);

		/*
		 * if package is not already downloaded or size mismatch,
		 * d/l it
		 */
		if ((stat(pkgpath, &st) < 0 ||
			st.st_size != pimpact->file_size) &&
			/* don't update file_size if repo is file:// */
			strncmp(pkgurl, SCHEME_FILE, strlen(SCHEME_FILE)) != 0)
				file_size += pimpact->file_size;

		if (pimpact->old_size_pkg > 0)
			pimpact->size_pkg -= pimpact->old_size_pkg;

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
	(void)humanize_number(h_psize, H_BUF, size_pkg, "",
		HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);

	/* check disk space */
	free_space = fs_room(pkgin_cache);
	if (free_space < file_size) {
		(void)humanize_number(h_free, H_BUF, (int64_t)free_space, "",
				HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
		errx(EXIT_FAILURE, MSG_NO_CACHE_SPACE,
			pkgin_cache, h_fsize, h_free);
	}
	free_space = fs_room(LOCALBASE);
	if (size_pkg > 0 && free_space < (uint64_t)size_pkg) {
		(void)humanize_number(h_free, H_BUF, (int64_t)free_space, "",
				HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
		errx(EXIT_FAILURE, MSG_NO_INSTALL_SPACE,
			LOCALBASE, h_psize, h_free);
	}

	printf("\n");

	if (do_inst && upgradenum > 0) {
		/* record ordered remove list before upgrade */
		removehead = order_upgrade_remove(impacthead);

		SLIST_FOREACH(premove, removehead, next) {
			if (premove->computed == TOUPGRADE) {
				toupgrade = action_list(toupgrade,
						premove->depend);
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
					toremove = action_list(toremove,
							premove->depend);
#ifdef DEBUG
					printf("package: %s - level: %d\n",
						premove->depend,
						premove->level);
#endif
				}
			}
			/*
			 * some packages may have been marked as TOREMOVE, then 
			 * discovered as TOUPGRADE
			 */
			if (toremove != NULL) {
				printf(MSG_PKGS_TO_REMOVE, removenum, toremove);
				printf("\n");
			}
		}

	} else if (do_inst)
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

		if (do_inst)
			printf(MSG_PKGS_TO_INSTALL, installnum, h_fsize, h_psize,
					toinstall);
		else
			printf(MSG_PKGS_TO_DOWNLOAD, installnum, h_fsize, toinstall);

		printf("\n");

		if (unmet_reqs != NULL)/* there were unmet requirements */
			printf(MSG_REQT_MISSING, unmet_reqs);

		if (check_yesno(DEFAULT_YES) == ANSW_NO)
			exit(rc);

		/*
		 * before erasing anything, download packages
		 * If there was an error while downloading, record it
		 */
		if (pkg_download(installhead) == EXIT_FAILURE)
			rc = EXIT_FAILURE;

		if (do_inst) {
			/* real install, not a simple download
			 *
			 * if there was upgrades, first remove
			 * old packages
			 */
			if (upgradenum > 0) {
				printf(MSG_RM_UPGRADE_PKGS);
				do_pkg_remove(removehead);
			}
			/*
			 * then pass ordered install list
			 * If there was an error while installing,
			 * record it
			 */
			if (do_pkg_install(installhead) == EXIT_FAILURE)
				rc = EXIT_FAILURE;

			/* pure install, not called by pkgin_upgrade */
			if (upgrade_type == UPGRADE_NONE)
				(void)update_db(LOCAL_SUMMARY, pkgargs, 1);

		}

	} else
		printf(MSG_NOTHING_TO_INSTALL);

installend:

	XFREE(toinstall);
	XFREE(toupgrade);
	XFREE(toremove);
	XFREE(unmet_reqs);
	free_pkglist(&impacthead, IMPACT);
	free_pkglist(&removehead, DEPTREE);
	free_pkglist(&installhead, DEPTREE);
	free_list(pkgargs);

	return rc;
}

int
pkgin_remove(char **pkgargs)
{
	int		deletenum = 0, exists, rc = EXIT_SUCCESS;
	Plisthead	*pdphead, *removehead;
	Pkglist		*pdp;
	char   		*todelete = NULL, **ppkgargs, *pkgname, *ppkg;

	pdphead = init_head();

	if (SLIST_EMPTY(&l_plisthead))
		errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);

	/* act on every package passed to the command line */
	for (ppkgargs = pkgargs; *ppkgargs != NULL; ppkgargs++) {

		if ((pkgname =
			find_exact_pkg(&l_plisthead, *ppkgargs)) == NULL) {
			printf(MSG_PKG_NOT_INSTALLED, *ppkgargs);
			rc = EXIT_FAILURE;
			continue;
		}
		ppkg = xstrdup(pkgname);
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
			/*
			 * identify unique package,
			 * don't cut it when ordering
			 */
			pdp->level = -1;
		else
			pdp->level = 0;

		pdp->name = xstrdup(pdp->depend);
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
		printf("\n");
		if (check_yesno(DEFAULT_YES)) {
			do_pkg_remove(removehead);

			(void)update_db(LOCAL_SUMMARY, NULL, 1);
		}

		analyse_pkglog(rm_filepos);
	} else
		printf(MSG_NO_PKGS_TO_DELETE);

	free_pkglist(&removehead, DEPTREE);
	free_pkglist(&pdphead, DEPTREE);

	XFREE(todelete);

	return rc;
}

/* 
 * find closest match for packages to be upgraded 
 * if we have mysql-5.1.10 installed prefer mysql-5.1.20 over
 * mysql-5.5.20 when upgrading
 */
static char *
narrow_match(Pkglist *opkg)
{
	Pkglist	*pkglist;
	char	*best_match;

	/* for now, best match is old package itself */
	best_match = xstrdup(opkg->full);

	SLIST_FOREACH(pkglist, &r_plisthead, next) {
		/* not the same pkgname, next */
		if (safe_strcmp(opkg->name, pkglist->name) != 0)
			continue;
		/* some bad packages have their PKG_PATH set to NULL */
		if (opkg->pkgpath != NULL && pkglist->pkgpath != NULL)
			/*
			 * if PKGPATH does not match, do not try to update
			 * (mysql 5.1/5.5)
			 */
			if (strcmp(opkg->pkgpath, pkglist->pkgpath) != 0)
				continue;

		/* same package version, next */
		if (safe_strcmp(opkg->full, pkglist->full) == 0)
			continue;

		/* second package is greater */
		if (version_check(best_match, pkglist->full) == 2) {
			XFREE(best_match);
			best_match = xstrdup(pkglist->full);
		}
	} /* SLIST_FOREACH remoteplisthead */

	/* there was no upgrade candidate */
	if (strcmp(best_match, opkg->full) == 0)
		XFREE(best_match);

	return best_match;
}

static char **
record_upgrades(Plisthead *plisthead)
{
	Pkglist	*pkglist;
	int	count = 0;
	char	**pkgargs;

	SLIST_FOREACH(pkglist, plisthead, next)
		count++;

	pkgargs = xmalloc((count + 2) * sizeof(char *));

	count = 0;
	SLIST_FOREACH(pkglist, plisthead, next) {
		pkgargs[count] = narrow_match(pkglist);

		if (pkgargs[count] == NULL)
			continue;

		count++;
	}
	pkgargs[count] = NULL;

	return pkgargs;
}

int
pkgin_upgrade(int uptype)
{
	Plistnumbered	*keeplisthead;
	Plisthead	*localplisthead;
	char		**pkgargs;
	int		rc;

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
		localplisthead = keeplisthead->P_Plisthead;

	pkgargs = record_upgrades(localplisthead);

	rc = pkgin_install(pkgargs, DO_INST);
	/*
	 * full upgrade, we need to record keep-packages
	 * in order to restore them
	 */
	if (uptype == UPGRADE_ALL) {
		free_list(pkgargs);
		/* record keep list */
		pkgargs = record_upgrades(keeplisthead->P_Plisthead);
	}

	(void)update_db(LOCAL_SUMMARY, pkgargs, 1);

	free_list(pkgargs);

	free_pkglist(&keeplisthead->P_Plisthead, LIST);
	free(keeplisthead);

	return rc;
}
