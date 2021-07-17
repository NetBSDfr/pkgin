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

/**
 * pkg_impact() calculates "impact" of installing a package
 * it is the heart of package install / update
 *
 * pkg_impact rationale:
 *
 * pkg_impact() receive a package list as an argument.
 * Those are the packages to be installed / upgraded.
 *
 * For every package, a dependency tree is loaded,
 * and every dependency is passed to deps_impact()
 * deps_impact() loops through local (installed) packages
 * and matches depencendy over them.
 * If the dependency exists but its version does not
 * satisfy pkg_match(), the package is marked as
 * "to upgrade", meaning it will be deleted (oldpkg) and
 * a new version will be installed (pkgname).
 *
 * Finally, the package itself is passed to deps_impact()
 * to figure out if it needs to be installed or upgraded
 */

#include "pkgin.h"

/**
 * \fn dep_present
 *
 * \brief check if a dependency is already recorded in the impact list
 */
static int
dep_present(Plisthead *impacthead, char *depname)
{
	Pkglist *pimpact;

	SLIST_FOREACH(pimpact, impacthead, next)
		if (pimpact->full != NULL &&
			pkg_match(depname, pimpact->full))
			return 1;

	return 0;
}

static void
break_depends(Plisthead *impacthead)
{
	Pkglist	   	*rmimpact, *pimpact;
	Plisthead	*rdphead, *fdphead;
	Pkglist	   	*rdp, *fdp;
	char		rpkg[BUFSIZ], pkgname[BUFSIZ];
	int		dep_break, exists;

	SLIST_FOREACH(pimpact, impacthead, next) {

		if (pimpact->old == NULL) /* DONOTHING or TOINSTALL  */
			continue;

		rdphead = init_head();

		XSTRCPY(pkgname, pimpact->old);
		trunc_str(pkgname, '-', STR_BACKWARD);

		/* fetch old package reverse dependencies */
		full_dep_tree(pkgname, LOCAL_REVERSE_DEPS, rdphead);

		/* browse reverse dependencies */
		SLIST_FOREACH(rdp, rdphead, next) {
			/*
			 * do not go deeper than 1 level (direct dependency)
			 * so we avoid removing packages that depend no more
			 * on a child dep. Example:
			 * qt4-libs had a direct depend on fontconfig, which had
			 * a direct depend on freetype2. When fontconfig did not
			 * depend anymore on freetype2, local qt4-libs has a
			 * missing dependency on freetype2, which was wrong.
			 */
			if (rdp->level > 1)
				continue;

			exists = 0;
			/* check if rdp was already on impact list */
			SLIST_FOREACH(rmimpact, impacthead, next)
				if (strcmp(	rmimpact->depend,
						rdp->depend) == 0) {
					exists = 1;
					break;
				}
			if (exists)
				continue;

			fdphead = init_head();

			/*
			 * reverse dependency is a full package name,
			 * use it and strip it
			 */
			XSTRCPY(rpkg, rdp->depend);
			trunc_str(rpkg, '-', STR_BACKWARD);

			/* fetch dependencies for rdp */
			full_dep_tree(rpkg, DIRECT_DEPS, fdphead);

			/* initialize to broken dependency */
			dep_break = 1;

			/*
			 * empty full dep tree, this can't happen in normal
			 * situation. If it does, that means that the reverse
			 * dependency we're analyzing has no direct dependency.
			 * Such a situation could occur if the reverse
			 * dependency is not in the repository anymore, leading
			 * to no information regarding this package.
			 * So we will check if local package dependencies are
			 * satisfied by our newly upgraded packages.
			 */
			if (SLIST_EMPTY(fdphead)) {
				free_pkglist(&fdphead);
				fdphead = init_head();
				full_dep_tree(rpkg, LOCAL_DIRECT_DEPS, fdphead);
			}

			/*
			 * browse dependencies for rdp and see if
			 * new package to be installed matches
			 */
			SLIST_FOREACH(fdp, fdphead, next) {
				if (pkg_match(fdp->depend, pimpact->full)) {
					dep_break = 0;
					break;
				}
			}

			free_pkglist(&fdphead);

			if (!dep_break)
				continue;

			/* dependency break, insert rdp in remove-list */
			rmimpact = malloc_pkglist();
			rmimpact->depend = xstrdup(rdp->depend);
			rmimpact->name = xstrdup(rpkg);
			rmimpact->full = xstrdup(rdp->depend);
			rmimpact->old = xstrdup(rdp->depend);
			rmimpact->action = TOREMOVE;
			rmimpact->level = 0;

			SLIST_INSERT_HEAD(impacthead, rmimpact, next);
		}
		free_pkglist(&rdphead);
	}
}

/**
 * loop through local packages and match for upgrades
 */
static int
deps_impact(Plisthead *impacthead, Pkglist *pdp, int output)
{
	int		toupgrade = DONOTHING;
	Plisthead	*revdeps;
	Pkglist		*revdep, *pimpact, *lpkg, *rpkg = NULL;
	char		*remotepkg = NULL;

	/* Skip if a package has already been considered. */
	if (pkg_in_impact(impacthead, pdp->depend))
		return 0;

	/* record corresponding package on remote list*/
	if (find_preferred_pkg(pdp->depend, &rpkg, &remotepkg) != 0) {
		if (output) {
			if (remotepkg == NULL)
				fprintf(stderr, "\n"MSG_PKG_NOT_AVAIL,
				    pdp->depend);
			else
				fprintf(stderr, "\n"MSG_PKG_NOT_PREFERRED,
				    pdp->depend, remotepkg);
		}
		return 1;
	}
	XSTRCPY(remotepkg, rpkg->full);

	TRACE(" |-matching %s over installed packages\n", remotepkg);

	/* create initial impact entry with a DONOTHING status, permitting
	 * to check if this dependency has already been recorded
	 */
	pimpact = malloc_pkglist();

	pimpact->depend = xstrdup(pdp->depend);

	pimpact->action = DONOTHING;
	pimpact->old = NULL;
	pimpact->full = NULL;
	pimpact->name = xstrdup(rpkg->name);

	/*
	 * BUILD_DATE may not necessarily be set.  This can happen if for any
	 * reason the pkgsrc metadata wasn't generated correctly (this has been
	 * observed in the wild), or simply if a package was built manually.
	 */
	pimpact->build_date =
	    (rpkg->build_date) ? xstrdup(rpkg->build_date) : NULL;

	SLIST_INSERT_HEAD(impacthead, pimpact, next);

	/* parse local packages to see if depedency is installed*/
	SLIST_FOREACH(lpkg, &l_plisthead, next) {

		/* match, package is installed */
		if (strcmp(lpkg->name, pdp->name) == 0) {

			TRACE("  > found %s\n", pdp->name);

			/*
			 * Figure out if this is an upgrade or a refresh.
			 */
			if (pkg_match(pdp->depend, lpkg->full) == 0)
				toupgrade = TOUPGRADE;
			/*
			 * Only consider a package for refresh if it has an
			 * identical PKGPATH.
			 */
			else if (pkgstrcmp(lpkg->pkgpath, rpkg->pkgpath) == 0
			     && pkgstrcmp(lpkg->build_date, rpkg->build_date))
				toupgrade = TOREFRESH;

			/* installed version does not match dep requirement */
			if (toupgrade != DONOTHING) {
				TRACE("   ! didn't match\n");

				/*
				 * Ignore proposed downgrades, unless it was
				 * specifically requested by "pkgin install .."
				 * where level will be 0.
				 *
				 * XXX: at some point count these as a specific
				 * TODOWNGRADE action or something, saying it's
				 * an upgrade is a bit confusing for users.
				 */
				if (pdp->level > 0 &&
				    version_check(lpkg->full, remotepkg) == 1) {
					toupgrade = DONOTHING;
					return 0;
				}

				TRACE("   * upgrade with %s\n", lpkg->full);
				/*
				 * insert as an upgrade
				 * oldpkg is used when building removal order
				 * list
				 */
				pimpact->old = xstrdup(lpkg->full);
				pimpact->action = toupgrade;
				pimpact->full = xstrdup(remotepkg);
				pimpact->level = pdp->level;
				pimpact->file_size = rpkg->file_size;
				pimpact->size_pkg = rpkg->size_pkg;
				pimpact->old_size_pkg = lpkg->size_pkg;

				/*
				 * For any package that is upgraded, we need to
				 * consider its direct reverse dependencies, as
				 * they will need to be refreshed for any shared
				 * library bumps etc.
				 *
				 * This is primarily for install operations that
				 * result in an upgrade, as an upgrade operation
				 * will already consider every package.
				 */
				if (toupgrade == TOUPGRADE) {
					revdeps = init_head();
					full_dep_tree(pdp->name, LOCAL_REVERSE_DEPS, revdeps);
					SLIST_FOREACH(revdep, revdeps, next) {
						if (revdep->level > 1)
							continue;
						if (!pkg_in_impact(impacthead, revdep->name))
							deps_impact(impacthead, revdep, 0);
					}
				}
			}

			TRACE("  > %s matched %s\n", lpkg->full, pdp->depend);

			return 0;
		} /* if installed package match */

		/*
		 * check if another local package with option matches
		 * dependency, i.e. libflashsupport-pulse, ghostscript-esp...
		 * would probably lead to conflict if recorded, pass.
		 */
		if (pkg_match(pdp->depend, lpkg->full)) {
			TRACE(" > local package %s matched with %s\n",
				lpkg->full, pdp->depend);
			return 0;
		}

	}

	if (!dep_present(impacthead, pdp->name)) {
		TRACE(" > recording %s as to install\n", remotepkg);

		pimpact->old = NULL;
		pimpact->action = TOINSTALL;

		pimpact->full = xstrdup(remotepkg);
		/* record package dependency deepness */
		pimpact->level = pdp->level;

		pimpact->file_size = rpkg->file_size;
		pimpact->size_pkg = rpkg->size_pkg;
	}

	return 0;
}

/**
 * is pkgname already in impact list ?
 */
uint8_t
pkg_in_impact(Plisthead *impacthead, char *depname)
{
	Pkglist *pimpact;

	SLIST_FOREACH(pimpact, impacthead, next) {
		if (strcmp(pimpact->depend, depname) == 0)
			return 1;
	}

	return 0;
}

Plisthead *
pkg_impact(char **pkgargs, int *rc)
{
	static char	*icon = __UNCONST(ICON_WAIT);
	Plisthead	*impacthead, *pdphead = NULL;
	Pkglist		*pimpact, *tmpimpact, *pdp;
	char		**ppkgargs, *pkgname = NULL;
	int		istty, rv;
	char		tmpicon;

	TRACE("[>]-entering impact\n");

	impacthead = init_head();

	istty = isatty(fileno(stdout));

	if (!istty)
		printf("calculating dependencies...");

	/* retreive impact list for all packages listed in the command line */
	for (ppkgargs = pkgargs; *ppkgargs != NULL; ppkgargs++) {

		if ((rv = find_preferred_pkg(*ppkgargs, NULL, &pkgname)) != 0) {
			if (pkgname == NULL)
				fprintf(stderr, MSG_PKG_NOT_AVAIL, *ppkgargs);
			else
				fprintf(stderr, MSG_PKG_NOT_PREFERRED, *ppkgargs, pkgname);
			*rc = EXIT_FAILURE;
			continue;
		}

		TRACE("[+]-impact for %s\n", pkgname);
		/* copy real package name back to pkgargs */
		*ppkgargs = pkgname;

		if (istty) {
			tmpicon = *icon++;
			printf("\rcalculating dependencies...%c", tmpicon);
			fflush(stdout);
			if (*icon == '\0')
				icon = icon - strlen(ICON_WAIT);
		}

		pdphead = init_head();
		/* dependencies discovery */
		full_dep_tree(pkgname, DIRECT_DEPS, pdphead);

		/* parse dependencies for pkgname */
		SLIST_FOREACH(pdp, pdphead, next) {

			/* is dependency already recorded in impact list ? */
			if (pkg_in_impact(impacthead, pdp->depend))
				continue;

			/* compare needed deps with local packages */
			if (deps_impact(impacthead, pdp, 1) != 0) {
				/*
				 * There was a versioning mismatch, proceed?
				 *
				 * XXX: Shouldn't this just bail?  Surely only
				 * bad things will happen if we continue.
				 */
				if (!check_yesno(DEFAULT_NO)) {
					free_pkglist(&impacthead);
					/* avoid free's repetition */
					goto impactfail;
				}
			}
		} /* SLIST_FOREACH deps */
		free_pkglist(&pdphead);

		/* finally, insert package itself */
		pdp = malloc_pkglist();

		pdp->name = xstrdup(pkgname);
		trunc_str(pdp->name, '-', STR_BACKWARD);

		/* pkgname is not already recorded */
		if (!pkg_in_impact(impacthead, pkgname)) {
			pdp->depend = xstrdup(pkgname);
			if (deps_impact(impacthead, pdp, 1) != 0) {
				free_pkglist(&impacthead);
				goto impactfail;
			}
			XFREE(pdp->depend);
		}

		XFREE(pdp->name);
		XFREE(pdp);
	} /* for (ppkgargs) */

	if (istty)
		printf("\rcalculating dependencies...done.\n");
	else
		printf("done.\n");

	TRACE("[<]-leaving impact\n");

	free_pkglist(&pdphead);

	/* check for depedencies breakage (php-4 -> php-5) */
	break_depends(impacthead);

	SLIST_FOREACH(pimpact, impacthead, next) {
		SLIST_FOREACH(tmpimpact, impacthead, next) {
			if (strcmp(pimpact->name, tmpimpact->name) != 0)
				continue;

			/*
			 * If a package has been initially marked as remove or
			 * refresh but then later is required for upgrade, mark
			 * the original as do nothing.
			 */
			if (pimpact->action == TOUPGRADE &&
			    (tmpimpact->action == TOREMOVE ||
			     tmpimpact->action == TOREFRESH)) {
				tmpimpact->action = DONOTHING;
				continue;
			}

			/*
			 * A package has been added multiple times with the
			 * same action but via different dependency matches.
			 * This will cause double counting, so we need to mark
			 * one of them as DONOTHING.  As it currently doesn't
			 * matter which dependency caused the action, choose to
			 * mark the latter for now.
			 */
			if (pimpact->action == tmpimpact->action &&
			    strcmp(pimpact->depend, tmpimpact->depend) != 0) {
				TRACE("Duplicate action %d for %s:"
				    " removing depends '%s', keeping '%s'\n",
				    pimpact->action, pimpact->name,
				    tmpimpact->depend, pimpact->depend);
				tmpimpact->action = DONOTHING;
			}
		}
	}

	/* remove DONOTHING entries */
	SLIST_FOREACH_SAFE(pimpact, impacthead, next, tmpimpact) {
		if (pimpact->action == DONOTHING) {

			SLIST_REMOVE(impacthead, pimpact, Pkglist, next);

			free_pkglist_entry(&pimpact);
		}
	}

	/* no more impact, empty list */
	if (SLIST_EMPTY(impacthead))
		free_pkglist(&impacthead);

impactfail:
	return impacthead;
}
