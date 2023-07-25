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

/*
 * Is package already in impact list?
 */
Pkglist *
local_pkg_in_impact(Plisthead *impacthead, char *pkgname)
{
	Pkglist *p;

	SLIST_FOREACH(p, impacthead, next) {
		if (p->lpkg && strcmp(p->lpkg->full, pkgname) == 0) {
			return p;
		}
	}

	return NULL;
}

Pkglist *
pkg_in_impact(Plisthead *impacthead, char *pkgname)
{
	Pkglist *p;

	SLIST_FOREACH(p, impacthead, next) {
		if (p->rpkg && strcmp(p->rpkg->full, pkgname) == 0) {
			return p;
		}
	}

	return NULL;
}

/*
 * Compare a local and matching remote package and determine what action needs
 * to be taken.  Requires both arguments be valid package list pointers.
 */
static int
calculate_action(Pkglist *lpkg, Pkglist *rpkg)
{
        /*
	 * If a DEPENDS match is specified and the local package does not match
	 * it then we need to upgrade.
	 *
	 * Otherwise if the version does not match then it is considered an
	 * upgrade.  Remote versions can go backwards in the event of a revert,
	 * however there is no support yet for TODOWNGRADE.
         */
	if ((rpkg->pattern && pkg_match(rpkg->pattern, lpkg->full) == 0) ||
	    (strcmp(lpkg->full, rpkg->full) != 0)) {
		TRACE("  > upgrading %s\n", lpkg->full);
		return TOUPGRADE;
	}

	/*
	 * If the remote package has an identical PKGPATH but a different
	 * BUILD_DATE then the package needs to be refreshed.
	 *
	 * Both matches use pkgstrcmp() as both fields could be NULL, for
	 * example in the case of manually constructed packages.
	 */
	if (pkgstrcmp(lpkg->pkgpath, rpkg->pkgpath) == 0 &&
	    pkgstrcmp(lpkg->build_date, rpkg->build_date)) {
		TRACE("  . refreshing %s\n", lpkg->full);
		return TOREFRESH;
	}

	TRACE("  = %s is up-to-date\n", lpkg->full);
	return DONOTHING;
}

/*
 * Given a remote package entry, loop through local packages looking for a
 * matching package name and calculate the required action.
 */
static void
deps_impact(Plisthead *impacthead, Pkglist *pdp, int upgrade)
{
	Plisthead	*rdeps;
	Pkglist		*pkg, *lpkg, *p;

	pkg = malloc_pkglist();

	pkg->action = DONOTHING;
	pkg->level = pdp->level;
	pkg->keep = pdp->keep;
	pkg->rpkg = pdp->rpkg;
	SLIST_INSERT_HEAD(impacthead, pkg, next);

	TRACE(" |- matching %s over installed packages\n", pkg->rpkg->full);
	SLIST_FOREACH(lpkg, &l_plisthead, next) {
		if (strcmp(lpkg->name, pkg->rpkg->name) != 0)
			continue;

		TRACE("  - found %s\n", lpkg->full);
		pkg->lpkg = lpkg;
		pkg->action = calculate_action(pkg->lpkg, pkg->rpkg);

		/*
		 * For any package that is upgraded, we need to consider its
		 * direct local reverse dependencies, as they will need to be
		 * refreshed for any shared library bumps etc.
		 *
		 * This is only for install operations, as upgrades will
		 * already consider every package.
		 */
		if (upgrade || pkg->action != TOUPGRADE)
			return;

		TRACE("  - considering reverse dependencies\n");
		rdeps = init_head();
		get_depends(pkg->lpkg->full, rdeps, DEPENDS_REVERSE);
		SLIST_FOREACH(p, rdeps, next) {
			Pkglist *l = p->lpkg;

			if (local_pkg_in_impact(impacthead, l->full))
				continue;

			if ((p->rpkg = find_pkg_match(l->name, l->pkgpath)))
				deps_impact(impacthead, p, upgrade);
		}

		return;
	}

	/*
	 * If we didn't match any local packages then this is a new package to
	 * install.
	 */
	TRACE(" > recording %s as to install\n", pkg->rpkg->full);
	pkg->action = TOINSTALL;
}

/*
 * Progress spinner.
 */
static char *icon = __UNCONST(ICON_WAIT);

static void
start_deps_spinner(int istty)
{
	if (!istty)
		printf("calculating dependencies...");
}

static void
update_deps_spinner(int istty)
{
	if (istty) {
		printf("\rcalculating dependencies...%c", *icon++);
		fflush(stdout);
		if (*icon == '\0')
			icon = icon - ICON_LEN;
	}
}
static void
finish_deps_spinner(int istty)
{
	if (istty)
		printf("\rcalculating dependencies...done.\n");
	else
		printf("done.\n");
}

/*
 * Loop through all local packages looking for available updates.
 */
static Plisthead *
pkg_impact_upgrade(void)
{
	Plisthead *impacthead, *depshead;
	Pkglist *dpkg, *lpkg, *p;
	int istty;

	istty = isatty(fileno(stdout));

	impacthead = init_head();
	depshead = init_head();

	SLIST_FOREACH(lpkg, &l_plisthead, next) {
		if (local_pkg_in_impact(impacthead, lpkg->full))
			continue;

		TRACE("  [+]-impact for %s\n", lpkg->full);
		p = malloc_pkglist();
		p->action = DONOTHING;
		p->lpkg = lpkg;
		SLIST_INSERT_HEAD(impacthead, p, next);

		/*
		 * Find a remote package that matches our PKGPATH (if we
		 * installed a specific version of e.g. nodejs then we don't
		 * want a newer version with a different PKGPATH to be
		 * considered).
		 *
		 * If there are no matches the package is just skipped, this
		 * can happen if for example the local package is self-built.
		 */
		p->rpkg = find_pkg_match(lpkg->name, lpkg->pkgpath);
		if (p->rpkg == NULL) {
			TRACE("   | - no remote match found\n");
			continue;
		}

		/* No upgrade or refresh found, we're done. */
		if ((p->action = calculate_action(lpkg, p->rpkg)) == DONOTHING)
			continue;

		/*
		 * If the remote package is being installed, find all of the
		 * remote package dependencies as there may be new or updated
		 * dependencies that need to be installed.
		 */
		update_deps_spinner(istty);
		get_depends_recursive(p->rpkg->full, depshead, DEPENDS_REMOTE);
	}

	/*
	 * We now have a full list of dependencies for all local packages,
	 * process them in turn, adding to impact list.  As we may have already
	 * seen an entry as part of looping through all packages, but without
	 * its correct dependency depth, update it if we found a deeper path so
	 * that install ordering is correct.
	 */
	SLIST_FOREACH(dpkg, depshead, next) {
		if ((p = pkg_in_impact(impacthead, dpkg->rpkg->full))) {
			if (dpkg->level > p->level)
				p->level = dpkg->level;
			continue;
		}
		deps_impact(impacthead, dpkg, 1);
	}
	free_pkglist(&depshead);

	return impacthead;
}

/*
 * For each package argument on the command line, look for suitable remote
 * packages and their dependencies to install.
 */
static Plisthead *
pkg_impact_install(char **pkgargs, int *rc)
{
	Plisthead *impacthead, *depshead;
	Pkglist *dpkg, *rpkg, *p;
	char **arg, *pkgname = NULL;
	int istty, rv;

	istty = isatty(fileno(stdout));

	impacthead = init_head();
	depshead = init_head();

	for (arg = pkgargs; *arg != NULL; arg++) {
		TRACE("  [+]-impact for %s\n", *arg);

		/*
		 * Find best remote package match.
		 */
		if ((rv = find_preferred_pkg(*arg, &rpkg, &pkgname)) != 0) {
			if (pkgname == NULL)
				fprintf(stderr, MSG_PKG_NOT_AVAIL, *arg);
			else
				fprintf(stderr, MSG_PKG_NOT_PREFERRED, *arg,
				    pkgname);
			*rc = EXIT_FAILURE;
			free(pkgname);
			continue;
		}
		/* copy real package name back to pkgargs */
		free(*arg);
		*arg = pkgname;

		TRACE("   | - found remote match %s\n", rpkg->full);

		/*
		 * It's possible we've already seen this package, either via a
		 * duplicate pattern match, or because it is a dependency for a
		 * package already processed.  If so, ensure it is marked as a
		 * keep package before skipping.
		 */
		if ((p = pkg_in_impact(impacthead, rpkg->full))) {
			p->keep = 1;
			continue;
		}

		p = malloc_pkglist();
		p->keep = 1;
		p->rpkg = rpkg;
		SLIST_INSERT_HEAD(impacthead, p, next);

		/*
		 * Get all recursive dependencies of the remote package and
		 * calculate their actions based on whether they match a local
		 * package or not.
		 */
		update_deps_spinner(istty);
		get_depends_recursive(p->rpkg->full, depshead, DEPENDS_REMOTE);
		deps_impact(impacthead, p, 0);
	}

	/*
	 * We now have a full list of dependencies for all local packages,
	 * process them in turn, adding to impact list.  As we may have already
	 * seen an entry as part of looping through all packages, but without
	 * its correct dependency depth, update it if we found a deeper path so
	 * that install ordering is correct.
	 */
	SLIST_FOREACH(dpkg, depshead, next) {
		if ((p = pkg_in_impact(impacthead, dpkg->rpkg->full))) {
			if (dpkg->level > p->level)
				p->level = dpkg->level;
			continue;
		}
		deps_impact(impacthead, dpkg, 0);
	}
	free_pkglist(&depshead);

	return impacthead;
}

/*
 * Return a list of package operations to perform, or NULL if nothing to do.
 *
 * If pkgargs is set we are performing an install operation with an input list
 * of packages and package matches, otherwise we are performing an upgrade
 * which loops through all local packages looking for upgrades.
 */
Plisthead *
pkg_impact(char **pkgargs, int *rc)
{
	Plisthead *impacthead;
	Pkglist *p, *tmpp;
	int istty;

	istty = isatty(fileno(stdout));

	TRACE("[>]-entering impact\n");
	start_deps_spinner(istty);

	if (pkgargs)
		impacthead = pkg_impact_install(pkgargs, rc);
	else
		impacthead = pkg_impact_upgrade();

	TRACE("[<]-leaving impact\n");
	finish_deps_spinner(istty);

	/*
	 * Remove DONOTHING entries to simplify processing in later stages,
	 * leaving only actionable entries.
	 */
	SLIST_FOREACH_SAFE(p, impacthead, next, tmpp) {
		if (p->action == DONOTHING) {
			SLIST_REMOVE(impacthead, p, Pkglist, next);
			free_pkglist_entry(&p);
		}
	}

	if (SLIST_EMPTY(impacthead))
		free_pkglist(&impacthead);

	return impacthead;
}
