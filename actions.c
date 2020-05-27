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
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#if defined(HAVE_SYS_TERMIOS_H) && !defined(__FreeBSD__)
#include <sys/termios.h>
#elif HAVE_TERMIOS_H
#include <termios.h>
#endif

static int	warn_count = 0, err_count = 0;
static uint8_t	said = 0;
FILE		*err_fp = NULL;
long int	rm_filepos = -1;
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
	char		pkg_fs[BUFSIZ];
	char		*p = NULL;
	off_t		size;
	int		rc = EXIT_SUCCESS;

	SLIST_FOREACH(pinstall, installhead, next) {
		/*
		 * pkgin_install() should have already marked whether this
		 * package requires downloading or not.
		 */
		if (!pinstall->download)
			continue;

		/*
		 * We don't (yet) support resume so start by explicitly
		 * removing any existing file.  pkgin_install() has already
		 * checked to see if it's valid, and we know it is not.
		 */
		(void) snprintf(pkg_fs, BUFSIZ, "%s/%s%s", pkgin_cache,
		    pinstall->depend, PKG_EXT);
		(void) unlink(pkg_fs);
		(void) umask(DEF_UMASK);

		if (strncmp(pinstall->pkgurl, "file:///", 8) == 0) {
			/*
			 * If this package repository URL is file:// we can
			 * just symlink rather than copying.  We do not support
			 * file:// URLs with a host component.
			 */
			p = &pinstall->pkgurl[7];

			if (stat(p, &st) != 0) {
				fprintf(stderr, MSG_PKG_NOT_AVAIL,
				    pinstall->depend);
				rc = EXIT_FAILURE;
				if (check_yesno(DEFAULT_NO) == ANSW_NO)
					exit(rc);
				pinstall->file_size = -1;
				continue;
			}

			if (symlink(p, pkg_fs) < 0)
				errx(EXIT_FAILURE,
				    "Failed to create symlink %s", pkg_fs);

			size = st.st_size;
		} else {
			/*
			 * Fetch via HTTP.  download_pkg() handles printing
			 * errors from various failure modes, so we handle
			 * cleanup only.
			 */
			if ((fp = fopen(pkg_fs, "w")) == NULL)
				err(EXIT_FAILURE, MSG_ERR_OPEN, pkg_fs);

			if ((size = download_pkg(pinstall->pkgurl, fp)) == -1) {
				(void) fclose(fp);
				(void) unlink(pkg_fs);
				rc = EXIT_FAILURE;

				if (check_yesno(DEFAULT_NO) == ANSW_NO)
					exit(rc);

				pinstall->file_size = -1;
				continue;
			}

			(void) fclose(fp);
		}

		/*
		 * download_pkg() already checked that we received the size
		 * specified by the server, this checks that it matches what
		 * is recorded by pkg_summary.
		 */
		if (size != pinstall->file_size) {
			(void) unlink(pkg_fs);
			rc = EXIT_FAILURE;

			(void) fprintf(stderr, "download error: %s size"
			    " does not match pkg_summary\n", pinstall->depend);

			if (check_yesno(DEFAULT_NO) == ANSW_NO)
				exit(rc);

			pinstall->file_size = -1;
			continue;
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
close_pi_log(int output)
{
	if (!verbosity && output) {
		analyse_pkglog(rm_filepos);
		printf(MSG_WARNS_ERRS, warn_count, err_count);
		if (warn_count > 0 || err_count > 0)
			printf(MSG_PKG_INSTALL_LOGGING_TO, pkgin_errlog);
	}

	warn_count = err_count = said = 0;
}

/*
 * Execute "pkg_admin rebuild-tree" to rebuild +REQUIRED_BY files after any
 * operation that changes the installed packages.
 */
static int
rebuild_required_by(void)
{
	int rv;
	char *cmd;

	/*
	 * Redirect stdout as it prints an annoying "Done." message that we
	 * don't want mixed into our output, however we retain stderr as there
	 * may be warnings about invalid dependencies that we want logged.
	 */
	cmd = xasprintf("%s rebuild-tree >/dev/null", pkg_admin);

	open_pi_log();
	rv = system(cmd);
	close_pi_log(0);

	free(cmd);

	return rv;
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

	close_pi_log(1);
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
do_pkg_install(Plisthead *installhead, int op)
{
	int		rc = EXIT_SUCCESS;
	Pkglist		*pinstall;
	char		pkgpath[BUFSIZ];
	const char	*pmsg;
	char		*pflags = verb_flag("-DU");

	if (op == TOREFRESH)
		pmsg = "refreshing %s...\n";
	else if (op == TOUPGRADE)
		pmsg = "upgrading %s...\n";
	else
		pmsg = "installing %s...\n";

	/* send pkg_add stderr to logfile */
	open_pi_log();

	SLIST_FOREACH(pinstall, installhead, next) {
		/* file not available in the repository */
		if (pinstall->file_size == -1)
			continue;

		printf(pmsg, pinstall->depend);
		snprintf(pkgpath, BUFSIZ,
			"%s/%s%s", pkgin_cache, pinstall->depend, PKG_EXT);

		if (!verbosity)
			log_tag(pmsg, pinstall->depend);

		if (fexec(pkg_add, pflags, pkgpath, NULL) == EXIT_FAILURE)
			rc = EXIT_FAILURE;
	}

	close_pi_log(1);

	return rc;
}

/* build the output line */
#define DEFAULT_WINSIZE	80
#define MAX_WINSIZE	512
char *
action_list(char *flatlist, char *str)
{
	struct winsize	winsize;
	size_t		curlen, cols = 0;
	char		*endl, *newlist = NULL;

	/* XXX: avoid duplicate in progressmeter.c */
	/* this is called for every package so no point handling sigwinch */
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) != -1 &&
			winsize.ws_col != 0) {
		if (winsize.ws_col > MAX_WINSIZE)
			cols = MAX_WINSIZE;
		else
			cols = winsize.ws_col;
	} else
		cols = DEFAULT_WINSIZE;

	/*
	 * If the user requested -n then we print a package per line, otherwise
	 * we try to fit them indented into the current line length.
	 */
	if (flatlist == NULL) {
		flatlist = xasprintf("%s%s", noflag ? "" : "  ", str);
		return flatlist;
	}

	if (str == NULL)
		return flatlist;

	/*
	 * No need to calculate line length if -n was requested.
	 */
	if (noflag) {
		newlist = xasprintf("%s\n%s", flatlist, str);
		return newlist;
	}

	endl = strrchr(flatlist, '\n');
	if (endl)
		curlen = strlen(endl);
	else
		curlen = strlen(flatlist);

	if ((curlen + strlen(str)) >= cols)
		newlist = xasprintf("%s\n  %s", flatlist, str);
	else
		newlist = xasprintf("%s %s", flatlist, str);

	return newlist;
}

#define H_BUF 6

int
pkgin_install(char **opkgargs, int do_inst, int upgrade)
{
	FILE		*fp;
	int		installnum = 0, upgradenum = 0, removenum = 0;
	int		refreshnum = 0, downloadnum = 0;
	int		rc = EXIT_SUCCESS;
	int		privsreqd = PRIVS_PKGINDB;
	uint64_t	free_space;
	int64_t		file_size = 0, size_pkg = 0;
	size_t		len;
	ssize_t		llen;
	Pkglist		*pkg;
	Pkglist		*pimpact;
	Plisthead	*impacthead; /* impact head */
	Plisthead	*upgradehead = NULL, *installhead = NULL;
	Plisthead	*refreshhead = NULL, *downloadhead = NULL;
	char		**pkgargs, *p;
	char		*toinstall = NULL, *toupgrade = NULL;
	char		*torefresh = NULL, *todownload = NULL;
	char		*unmet_reqs = NULL;
	char		pkgpath[BUFSIZ], pkgrepo[BUFSIZ], query[BUFSIZ];
	char		h_psize[H_BUF], h_fsize[H_BUF], h_free[H_BUF];
	struct		stat st;

	if (SLIST_EMPTY(&r_plisthead)) {
		printf("%s\n", MSG_EMPTY_AVAIL_PKGLIST);
		return EXIT_FAILURE;
	}

	/* transform command line globs into pkgnames */
	if ((pkgargs = glob_to_pkgarg(opkgargs, &rc)) == NULL) {
		printf(MSG_NOTHING_TO_DO);
		return rc;
	}

	if (do_inst)
		privsreqd |= PRIVS_PKGDB;

	if (!have_privs(privsreqd))
		errx(EXIT_FAILURE, MSG_DONT_HAVE_RIGHTS);

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

		/*
		 * Packages being removed need no special handling, account
		 * for them and move to the next package.
		 */
		if (pimpact->action == TOREMOVE) {
			removenum++;
			continue;
		}

		/* check for conflicts */
		if (pkg_has_conflicts(pimpact))
			if (!check_yesno(DEFAULT_NO))
				goto installend;

		/* XXX: this should be moved higher up, pdb_rec_list assert? */
		if (pimpact->file_size <= 0) {
			(void) fprintf(stderr, MSG_EMPTY_FILE_SIZE,
			    pimpact->depend);
			continue;
		}

		/*
		 * Retrieve the correct repository for the package and save it,
		 * this is used later by pkg_download().
		 */
		(void) snprintf(query, BUFSIZ, PKG_URL, pimpact->full);
		if (pkgindb_doquery(query, pdb_get_value, pkgrepo) != 0)
			errx(EXIT_FAILURE, MSG_PKG_NO_REPO, pimpact->full);

		pimpact->pkgurl = xasprintf("%s/%s%s", pkgrepo, pimpact->full,
		    PKG_EXT);

		/*
		 * If the binary package has not already been downloaded, or
		 * its size does not match pkg_summary, then mark it to be
		 * downloaded.
		 */
		(void) snprintf(pkgpath, BUFSIZ, "%s/%s%s", pkgin_cache,
		    pimpact->full, PKG_EXT);
		if (stat(pkgpath, &st) < 0 || st.st_size != pimpact->file_size)
			pimpact->download = 1;
		else {
			/*
			 * If the cached package has the correct size, we must
			 * verify that the BUILD_DATE has not changed, in case
			 * the sizes happen to be identical.
			 */
			p = xasprintf("%s -Q BUILD_DATE %s", pkg_info, pkgpath);

			if ((fp = popen(p, "r")) == NULL)
				err(EXIT_FAILURE, "Cannot execute '%s'", p);
			(void) free(p);

			for (p = NULL, len = 0;
			     (llen = getline(&p, &len, fp)) > 0;
			     (void) free(p), p = NULL, len = 0) {
				if (p[llen - 1] == '\n')
					p[llen - 1] = '\0';
				if (pkgstrcmp(p, pimpact->build_date))
					pimpact->download = 1;
			}
			(void) pclose(fp);
		}

		/*
		 * Don't account for download size if using a file:// repo.
		 */
		if (pimpact->download) {
			downloadnum++;
			if (strncmp(pkgrepo, "file:///", 8) != 0)
				file_size += pimpact->file_size;
		}

		if (pimpact->old_size_pkg > 0)
			pimpact->size_pkg -= pimpact->old_size_pkg;

		size_pkg += pimpact->size_pkg;

		switch (pimpact->action) {
		case TOREFRESH:
			refreshnum++;
			break;
		case TOUPGRADE:
			upgradenum++;
			break;
		case TOINSTALL:
			installnum++;
			break;
		}
	}

	(void)humanize_number(h_fsize, H_BUF, file_size, "",
		HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
	(void)humanize_number(h_psize, H_BUF, size_pkg, "",
		HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);

	/* check disk space */
	free_space = fs_room(pkgin_cache);
	if (free_space < (uint64_t)file_size) {
		(void)humanize_number(h_free, H_BUF, (int64_t)free_space, "",
				HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
		errx(EXIT_FAILURE, MSG_NO_CACHE_SPACE,
			pkgin_cache, h_fsize, h_free);
	}
	free_space = fs_room(PREFIX);
	if (size_pkg > 0 && free_space < (uint64_t)size_pkg) {
		(void)humanize_number(h_free, H_BUF, (int64_t)free_space, "",
				HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
		errx(EXIT_FAILURE, MSG_NO_INSTALL_SPACE, PREFIX, h_psize,
		    h_free);
	}

	/*
	 * Nothing to install or download, exit early.
	 */
	if (!do_inst && downloadnum == 0) {
		printf(MSG_NOTHING_TO_DO);
		return rc;
	}

	/*
	 * Separate package lists according to action.
	 */
	downloadhead = order_download(impacthead);
	SLIST_FOREACH(pimpact, downloadhead, next) {
		todownload = action_list(todownload, pimpact->depend);
	}

	refreshhead = order_install(impacthead, TOREFRESH);
	SLIST_FOREACH(pkg, refreshhead, next) {
		torefresh = action_list(torefresh, pkg->depend);
	}

	upgradehead = order_install(impacthead, TOUPGRADE);
	SLIST_FOREACH(pkg, upgradehead, next) {
		toupgrade = action_list(toupgrade, pkg->depend);
	}

	installhead = order_install(impacthead, TOINSTALL);
	SLIST_FOREACH(pkg, installhead, next) {
		toinstall = action_list(toinstall, pkg->depend);
	}

	printf("\n");

	if (do_inst) {
		if (refreshnum > 0)
			printf("%d package%s to refresh:\n%s\n\n", refreshnum,
			    (refreshnum == 1) ? "" : "s", torefresh);

		if (upgradenum > 0)
			printf("%d package%s to upgrade:\n%s\n\n", upgradenum,
			    (upgradenum == 1) ? "" : "s", toupgrade);

		if (installnum > 0)
			printf("%d package%s to install:\n%s\n\n", installnum,
			    (installnum == 1) ? "" : "s", toinstall);

		printf("%d to refresh, %d to upgrade, %d to install\n",
		    refreshnum, upgradenum, installnum);
		printf("%s to download, %s to install\n", h_fsize, h_psize);
	} else {
		printf("%d package%s to download:\n%s\n", downloadnum,
		    (downloadnum == 1) ? "" : "s", todownload);
		printf("%s to download\n", h_fsize);
	}

	if (unmet_reqs != NULL)/* there were unmet requirements */
		printf(MSG_REQT_MISSING, unmet_reqs);

	if (!noflag)
		printf("\n");

	if (check_yesno(DEFAULT_YES) == ANSW_NO)
		exit(rc);

	/*
	 * First fetch all required packages.  If we're only doing downloads
	 * then we're done, otherwise recalculate to account for failures.
	 */
	if (downloadnum > 0) {
		if (pkg_download(downloadhead) == EXIT_FAILURE)
			rc = EXIT_FAILURE;
		if (!do_inst)
			goto installend;

		SLIST_FOREACH(pimpact, downloadhead, next) {
			if (pimpact->file_size != -1)
				continue;

			switch (pimpact->action) {
			case TOREFRESH:
				refreshnum--;
				break;
			case TOUPGRADE:
				upgradenum--;
				break;
			case TOINSTALL:
				installnum--;
				break;
			}
		}
	}

	if (refreshnum + upgradenum + installnum == 0)
		goto installend;

	/*
	 * At this point we're performing installs.  Do them in order.
	 */
	if (refreshnum > 0) {
		if (do_pkg_install(refreshhead, TOREFRESH) == EXIT_FAILURE)
			rc = EXIT_FAILURE;
	}
	if (upgradenum > 0) {
		if (do_pkg_install(upgradehead, TOUPGRADE) == EXIT_FAILURE)
			rc = EXIT_FAILURE;
	}
	if (installnum > 0) {
		if (do_pkg_install(installhead, TOINSTALL) == EXIT_FAILURE)
			rc = EXIT_FAILURE;
	}

	/*
	 * Recalculate +REQUIRED_BY entries after all installs have finished,
	 * as things can get out of sync when using pkg_add -DU, leading to the
	 * dreaded "Can't open +CONTENTS of depending package..." errors when
	 * upgrading next time.
	 */
	if (rebuild_required_by() != EXIT_SUCCESS)
		rc = EXIT_FAILURE;

	/* pure install, not called by pkgin_upgrade */
	if (!upgrade)
		(void)update_db(LOCAL_SUMMARY, pkgargs, 1);

installend:

	XFREE(toinstall);
	XFREE(toupgrade);
	XFREE(unmet_reqs);
	free_pkglist(&impacthead);
	free_pkglist(&upgradehead);
	free_pkglist(&installhead);
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
		pdp = malloc_pkglist();

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
		if (!noflag)
			printf("\n");
		if (check_yesno(DEFAULT_YES)) {
			do_pkg_remove(removehead);

			/*
			 * Recalculate +REQUIRED_BY entries in case anything
			 * has been unable to update correctly.
			 */
			if (rebuild_required_by() != EXIT_SUCCESS)
				rc = EXIT_FAILURE;

			(void)update_db(LOCAL_SUMMARY, NULL, 1);
		}
	} else
		printf(MSG_NO_PKGS_TO_DELETE);

	free_pkglist(&removehead);
	free_pkglist(&pdphead);

	XFREE(todelete);

	return rc;
}

/*
 * Find best match for a package to be upgraded.
 */
static char *
narrow_match(Pkglist *opkg)
{
	Pkglist	*pkglist;
	char	*best_match;
	int	refresh = 0;

	/* for now, best match is old package itself */
	best_match = xstrdup(opkg->full);

	SLIST_FOREACH(pkglist, &r_plisthead, next) {
		/* not the same pkgname, next */
		if (pkgstrcmp(opkg->name, pkglist->name))
			continue;

		/*
		 * Do not propose an upgrade if the PKGPATH does not match,
		 * otherwise users who have requested a specific version of a
		 * package for which there are usually multiple versions
		 * available (e.g. nodejs or mysql) would always have it
		 * replaced by the latest version.
		 *
		 * Note that this comparison does allow both PKGPATH values
		 * to be NULL, for example with local self-built packages, in
		 * which case we permit an upgrade.
		 */
		if (pkgstrcmp(opkg->pkgpath, pkglist->pkgpath))
			continue;

		/*
		 * If the package version is identical, check if the BUILD_DATE
		 * has changed.  If it has, we need to refresh the package as
		 * it has been rebuilt, possibly against newer dependencies.
		 */
		if (pkgstrcmp(opkg->full, pkglist->full) == 0) {
			if (pkgstrcmp(opkg->build_date, pkglist->build_date))
				refresh = 1;
			continue;
		}

		/* second package is greater */
		if (version_check(best_match, pkglist->full) == 2) {
			XFREE(best_match);
			best_match = xstrdup(pkglist->full);
		}
	} /* SLIST_FOREACH remoteplisthead */

	/* there was no upgrade candidate */
	if (strcmp(best_match, opkg->full) == 0 && !refresh)
		XFREE(best_match);

	return best_match;
}

static char **
record_upgrades(Plisthead *plisthead)
{
	Pkglist	*pkglist;
	size_t	count = 0;
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
pkgin_upgrade(int do_inst)
{
	Plistnumbered	*keeplisthead;
	char		**pkgargs;
	int		rc;

	/* record keepable packages */
	if ((keeplisthead = rec_pkglist(KEEP_LOCAL_PKGS)) == NULL)
		errx(EXIT_FAILURE, MSG_EMPTY_KEEP_LIST);

	/* upgrade all packages, not only keepables */
	if (SLIST_EMPTY(&l_plisthead))
		errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);

	pkgargs = record_upgrades(&l_plisthead);

	rc = pkgin_install(pkgargs, do_inst, 1);

	free_list(pkgargs);

	/* Record keep list */
	if (do_inst) {
		pkgargs = record_upgrades(keeplisthead->P_Plisthead);
		(void)update_db(LOCAL_SUMMARY, pkgargs, 1);
		free_list(pkgargs);
	}

	free_pkglist(&keeplisthead->P_Plisthead);
	free(keeplisthead);

	return rc;
}
