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

#include <sqlite3.h>
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

static char *
verb_flag(const char *flags)
{
	strcpy(pkgtools_flags, flags);

	if (verbosity)
		strlcat(pkgtools_flags, "v", 1);

	return pkgtools_flags;
}

static int
pkg_download(Plisthead *installhead)
{
	FILE		*fp;
	Pkglist  	*p;
	struct stat	st;
	char		pkg_fs[BUFSIZ];
	char		*pkgurl;
	int		rc = EXIT_SUCCESS;

	SLIST_FOREACH(p, installhead, next) {
		/*
		 * We don't (yet) support resume so start by explicitly
		 * removing any existing file.  pkgin_install() has already
		 * checked to see if it's valid, and we know it is not.
		 */
		(void) snprintf(pkg_fs, BUFSIZ, "%s/%s%s", pkgin_cache,
		    p->ipkg->rpkg->full, PKG_EXT);
		(void) unlink(pkg_fs);
		(void) umask(DEF_UMASK);

		if (strncmp(p->ipkg->pkgurl, "file:///", 8) == 0) {
			/*
			 * If this package repository URL is file:// we can
			 * just symlink rather than copying.  We do not support
			 * file:// URLs with a host component.
			 */
			pkgurl = &p->ipkg->pkgurl[7];

			if (stat(pkgurl, &st) != 0) {
				fprintf(stderr, MSG_PKG_NOT_AVAIL,
				    p->ipkg->rpkg->full);
				rc = EXIT_FAILURE;
				if (check_yesno(DEFAULT_NO) == ANSW_NO)
					exit(rc);
				p->ipkg->file_size = -1;
				continue;
			}

			if (symlink(pkgurl, pkg_fs) < 0)
				errx(EXIT_FAILURE,
				    "Failed to create symlink %s", pkg_fs);

			p->ipkg->file_size = st.st_size;
		} else {
			/*
			 * Fetch via HTTP.  download_pkg() handles printing
			 * errors from various failure modes, so we handle
			 * cleanup only.
			 */
			if ((fp = fopen(pkg_fs, "w")) == NULL)
				err(EXIT_FAILURE, MSG_ERR_OPEN, pkg_fs);

			if ((p->ipkg->file_size =
			    download_pkg(p->ipkg->pkgurl, fp)) == -1) {
				(void) fclose(fp);
				(void) unlink(pkg_fs);
				rc = EXIT_FAILURE;

				if (check_yesno(DEFAULT_NO) == ANSW_NO)
					exit(rc);

				continue;
			}

			(void) fclose(fp);
		}

		/*
		 * download_pkg() already checked that we received the size
		 * specified by the server, this checks that it matches what
		 * is recorded by pkg_summary.
		 */
		if (p->ipkg->file_size != p->ipkg->rpkg->file_size) {
			(void) unlink(pkg_fs);
			rc = EXIT_FAILURE;

			(void) fprintf(stderr, "download error: %s size"
			    " does not match pkg_summary\n",
			    p->ipkg->rpkg->full);

			if (check_yesno(DEFAULT_NO) == ANSW_NO)
				exit(rc);

			p->ipkg->file_size = -1;
			continue;
		}
	}

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

	printf("%s", log_action);
	if (!verbosity) {
		fprintf(err_fp, "---%s: %s", now_date, log_action);
		fflush(err_fp);
	}
}

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
	Pkglist *p;

	/* send pkg_delete stderr to logfile */
	open_pi_log();

	SLIST_FOREACH(p, removehead, next) {
		log_tag(MSG_REMOVING, p->lpkg->full);
		if (fexec(pkg_delete, verb_flag("-f"), p->lpkg->full, NULL)
		    != EXIT_SUCCESS)
			err_count++;
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
do_pkg_install(Plisthead *installhead)
{
	int		rc = EXIT_SUCCESS;
	Pkglist		*p;
	char		pkgpath[BUFSIZ];
	const char	*iflags, *aflags, *pflags;

	/*
	 * Packages specified on the command line are marked as "keep", while
	 * their dependencies use the -A pkg_add flag to indicate they are
	 * automatic packages that can be autoremoved when no longer required.
	 */
	iflags = (verbosity) ? "-DUv" : "-DU";
	aflags = (verbosity) ? "-ADUv" : "-ADU";

	/* send pkg_add stderr to logfile */
	open_pi_log();

	SLIST_FOREACH(p, installhead, next) {
		/* file not available in the repository */
		if (p->ipkg->file_size == -1)
			continue;

		/*
		 * If package has been marked as keep (explicitly installed),
		 * or is an upgrade of a local package that was marked as keep,
		 * then use the non-automatic flags, otherwise use -A.
		 */
		if (p->ipkg->keep || (p->ipkg->lpkg && p->ipkg->lpkg->keep))
			pflags = iflags;
		else
			pflags = aflags;

		snprintf(pkgpath, BUFSIZ, "%s/%s%s", pkgin_cache,
		    p->ipkg->rpkg->full, PKG_EXT);

		switch (p->ipkg->action) {
		case TOREFRESH:
			log_tag("refreshing %s...\n", p->ipkg->rpkg->full);
			break;
		case TOUPGRADE:
			log_tag("upgrading %s...\n", p->ipkg->rpkg->full);
			break;
		case TOINSTALL:
			log_tag("installing %s...\n", p->ipkg->rpkg->full);
			break;
		}

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
		free(flatlist);
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

	free(flatlist);
	return newlist;
}

#define H_BUF 6

int
pkgin_install(char **pkgargs, int do_inst, int upgrade)
{
	FILE		*fp;
	int		installnum = 0, upgradenum = 0;
	int		refreshnum = 0, downloadnum = 0;
	int		rc = EXIT_SUCCESS;
	int		privsreqd = PRIVS_PKGINDB;
	uint64_t	free_space;
	int64_t		file_size = 0, size_pkg = 0;
	size_t		len;
	ssize_t		llen;
	Pkglist		*p;
	Plisthead	*impacthead, *downloadhead = NULL, *installhead = NULL;
	Plistnumbered	*conflicts;
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

	if (do_inst)
		privsreqd |= PRIVS_PKGDB;

	if (!have_privs(privsreqd))
		errx(EXIT_FAILURE, MSG_DONT_HAVE_RIGHTS);

	/*
	 * Calculate full impact (list of packages and actions to perform) of
	 * requested operation.
	 */
	if ((impacthead = pkg_impact(pkgargs, &rc)) == NULL) {
		printf(MSG_NOTHING_TO_DO);
		return rc;
	}

	/* check for required files */
	if (!pkg_met_reqs(impacthead))
		SLIST_FOREACH(p, impacthead, next)
			if (p->action == UNMET_REQ)
				unmet_reqs =
				    action_list(unmet_reqs, p->rpkg->full);

	conflicts = rec_pkglist(LOCAL_CONFLICTS);

	/* browse impact tree */
	SLIST_FOREACH(p, impacthead, next) {

		if (p->action == DONOTHING)
			continue;

		/*
		 * Packages being removed need no special handling, account
		 * for them and move to the next package.
		 */
		if (p->action == TOREMOVE)
			continue;

		/* check for conflicts */
		if (conflicts && pkg_has_conflicts(p, conflicts))
			if (!check_yesno(DEFAULT_NO))
				goto installend;

		/*
		 * Retrieve the correct repository for the package and save it,
		 * this is used later by pkg_download().
		 */
		sqlite3_snprintf(BUFSIZ, query, PKG_URL, p->rpkg->full);
		if (pkgindb_doquery(query, pdb_get_value, pkgrepo) != 0)
			errx(EXIT_FAILURE, MSG_PKG_NO_REPO, p->rpkg->full);

		p->pkgurl = xasprintf("%s/%s%s", pkgrepo, p->rpkg->full,
		    PKG_EXT);

		/*
		 * If the binary package has not already been downloaded, or
		 * its size does not match pkg_summary, then mark it to be
		 * downloaded.
		 */
		(void) snprintf(pkgpath, BUFSIZ, "%s/%s%s", pkgin_cache,
		    p->rpkg->full, PKG_EXT);
		if (stat(pkgpath, &st) < 0 ||
		    st.st_size != p->rpkg->file_size)
			p->download = 1;
		else {
			/*
			 * If the cached package has the correct size, we must
			 * verify that the BUILD_DATE has not changed, in case
			 * the sizes happen to be identical.
			 */
			char *s;
			s = xasprintf("%s -Q BUILD_DATE %s", pkg_info, pkgpath);

			if ((fp = popen(s, "r")) == NULL)
				err(EXIT_FAILURE, "Cannot execute '%s'", s);
			free(s);

			for (s = NULL, len = 0;
			     (llen = getline(&s, &len, fp)) > 0;
			     free(s), s = NULL, len = 0) {
				if (s[llen - 1] == '\n')
					s[llen - 1] = '\0';
				if (pkgstrcmp(s, p->rpkg->build_date))
					p->download = 1;
			}
			(void) pclose(fp);
		}

		/*
		 * Don't account for download size if using a file:// repo.
		 */
		if (p->download) {
			downloadnum++;
			if (strncmp(pkgrepo, "file:///", 8) != 0)
				file_size += p->rpkg->file_size;
		}

		if (p->lpkg && p->lpkg->size_pkg > 0)
			size_pkg += p->rpkg->size_pkg - p->lpkg->size_pkg;
		else
			size_pkg += p->rpkg->size_pkg;

		switch (p->action) {
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
	SLIST_FOREACH(p, downloadhead, next) {
		todownload = action_list(todownload, p->ipkg->rpkg->full);
	}

	installhead = order_install(impacthead);
	SLIST_FOREACH(p, installhead, next) {
		switch (p->ipkg->action) {
		case TOREFRESH:
			torefresh = action_list(torefresh, p->ipkg->rpkg->full);
			break;
		case TOUPGRADE:
			toupgrade = action_list(toupgrade, p->ipkg->rpkg->full);
			break;
		case TOINSTALL:
			toinstall = action_list(toinstall, p->ipkg->rpkg->full);
			break;
		}
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

		if (refreshnum == 0 && upgradenum == 0 && installnum == 0)
			exit(rc);
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

		SLIST_FOREACH(p, downloadhead, next) {
			if (p->ipkg->file_size != -1)
				continue;

			switch (p->ipkg->action) {
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
	 * At this point we're performing installs.
	 */
	if (do_pkg_install(installhead) == EXIT_FAILURE)
		rc = EXIT_FAILURE;

	/*
	 * Recalculate +REQUIRED_BY entries after all installs have finished,
	 * as things can get out of sync when using pkg_add -DU, leading to the
	 * dreaded "Can't open +CONTENTS of depending package..." errors when
	 * upgrading next time.
	 */
	if (rebuild_required_by() != EXIT_SUCCESS)
		rc = EXIT_FAILURE;

	(void)update_db(LOCAL_SUMMARY, 1);

installend:
	XFREE(todownload);
	XFREE(toinstall);
	XFREE(torefresh);
	XFREE(toupgrade);
	XFREE(unmet_reqs);
	free_pkglist(&impacthead);
	free_pkglist(&downloadhead);
	/*
	 * installhead may be NULL, for example if trying to install a package
	 * that conflicts.
	 */
	if (installhead != NULL)
		free_pkglist(&installhead);

	return rc;
}

int
pkgin_remove(char **pkgargs)
{
	Plisthead	*rmhead, *removehead;
	Pkglist		*lpkg, *p;
	int		deletenum = 0, rc = EXIT_SUCCESS;
	char   		*todelete = NULL, **arg;

	if (SLIST_EMPTY(&l_plisthead))
		errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);

	rmhead = init_head();

	/*
	 * For every package or pattern on the command line, find a matching
	 * installed package and add to rmhead.
	 */
	for (arg = pkgargs; *arg != NULL; arg++) {
		if ((lpkg = find_local_pkg_match(*arg)) == NULL) {
			printf(MSG_PKG_NOT_INSTALLED, *arg);
			rc = EXIT_FAILURE;
			continue;
		}
		free(*arg);
		*arg = xstrdup(lpkg->full);

		/*
		 * Fetch full reverse dependencies for local package and add
		 * them to rmhead too.
		 */
		get_depends_recursive(lpkg->full, rmhead, DEPENDS_REVERSE);

		/*
		 * Add the package itself.
		 */
		p = malloc_pkglist();
		p->lpkg = lpkg;
		SLIST_INSERT_HEAD(rmhead, p, next);
	}

	/* order remove list */
	removehead = order_remove(rmhead);

	SLIST_FOREACH(p, removehead, next) {
		deletenum++;
		todelete = action_list(todelete, p->lpkg->full);
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

			(void)update_db(LOCAL_SUMMARY, 1);
		}
	} else
		printf(MSG_NO_PKGS_TO_DELETE);

	free_pkglist(&removehead);
	free_pkglist(&rmhead);
	XFREE(todelete);

	return rc;
}

int
pkgin_upgrade(int do_inst)
{

	if (SLIST_EMPTY(&l_plisthead))
		errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);

	return pkgin_install(NULL, do_inst, 1);
}
