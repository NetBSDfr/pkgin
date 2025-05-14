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

/*
 * Check for an existing entry in impact head for either a local pkg or remote.
 */
static Pkglist *
local_pkg_in_impact(Plistarray *impacthead, Pkglist *pkg)
{
	Plisthead *head;
	Pkglist *epkg;
	int slot;

	slot = pkg_hash_entry(pkg->name, impacthead->size);
	head = &impacthead->head[slot];

	epkg = pkgname_in_local_pkglist(pkg->full, head, 1);

	return epkg;
}
static Pkglist *
remote_pkg_in_impact(Plistarray *impacthead, Pkglist *pkg)
{
	Plisthead *head;
	Pkglist *epkg;
	int slot;

	slot = pkg_hash_entry(pkg->name, impacthead->size);
	head = &impacthead->head[slot];

	epkg = pkgname_in_remote_pkglist(pkg->full, head, 1);

	return epkg;
}

/*
 * Compare a local and matching remote package and determine what action needs
 * to be taken.  Requires both arguments be valid package list pointers.
 */
static action_t
calculate_action(Pkglist *lpkg, Pkglist *rpkg)
{
	int c;

        /*
	 * If a DEPENDS match is specified but the local package does not match
	 * it then it needs to be upgraded.
	 */
	if (rpkg->patcount) {
		for (c = 0; c < rpkg->patcount; c++) {
			if (pkg_match(rpkg->patterns[c], lpkg->full) == 0) {
				TRACE("  > upgrading %s to match %s\n",
				    lpkg->full, rpkg->patterns[c]);
				return ACTION_UPGRADE;
			}
		}
	}

	/*
	 * If the version does not match then it is considered an upgrade.
	 * Remote versions can go backwards in the event of a revert, however
	 * there is no distinction yet for these (e.g. ACTION_DOWNGRADE).
	 */
	if (strcmp(lpkg->full, rpkg->full) != 0) {
		TRACE("  > upgrading %s to %s\n", lpkg->full, rpkg->full);
		return ACTION_UPGRADE;
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
		return ACTION_REFRESH;
	}

	TRACE("  = %s is up-to-date\n", lpkg->full);
	return ACTION_NONE;
}

/*
 * SQLite callback for REMOTE_SUPERSEDES, look for any local package that
 * matches a SUPERSEDES pattern, using an optional PKGBASE for faster lookups.
 *
 * argv0: SUPERSEDES pattern
 * argv1: PKGBASE, may be NULL if it cannot be determined from pattern
 */
static int
record_supersedes(void *param, int argc, char **argv, char **colname)
{
	Plisthead *supersedes = (Plisthead *)param;
	Pkglist *p, *lpkg;

	if (argv == NULL)
		return PDB_ERR;

	/*
	 * If we've already searched for this exact SUPERSEDES pattern then
	 * return early and do not add to supersedes.
	 */
	SLIST_FOREACH(p, supersedes, next) {
		if (strcmp(p->patterns[0], argv[0]) == 0)
			return PDB_OK;
	}

	/*
	 * Find matching entry in the local package list.  If there are no
	 * matches we're done.
	 */
	if ((lpkg = find_local_pkg(argv[0], argv[1])) == NULL)
		return PDB_OK;

	/*
	 * Look to see if we already found this package via a different match.
	 */
	SLIST_FOREACH(p, supersedes, next) {
		if (strcmp(lpkg->full, p->lpkg->full) == 0)
			return PDB_OK;
	}

	/*
	 * An entry we've matched and haven't seen before, add it.
	 */
	p = malloc_pkglist();
	p->patterns = xmalloc(2 * sizeof(char *));
	p->patterns[0] = xstrdup(argv[0]);
	p->patterns[1] = NULL;
	p->patcount = 1;
	p->lpkg = lpkg;
	SLIST_INSERT_HEAD(supersedes, p, next);

	return PDB_OK;
}

static Plisthead *
find_supersedes(Plistarray *impacthead)
{
	Plisthead *supersedes;
	Pkglist *pkg;
	int i;
	char query[BUFSIZ];

	supersedes = init_head();

	/*
	 * Only consider packages that are going to be installed.
	 */
	for (i = 0; i < impacthead->size; i++) {
	SLIST_FOREACH(pkg, &impacthead->head[i], next) {
		if (!action_is_install(pkg->action))
			continue;

		sqlite3_snprintf(BUFSIZ, query, REMOTE_SUPERSEDES,
		    pkg->rpkg->full);
		pkgindb_doquery(query, record_supersedes, supersedes);
	}
	}

	if (SLIST_EMPTY(supersedes)) {
		free_pkglist(&supersedes);
		return NULL;
	}

	return supersedes;
}

/*
 * Calculate the impact for a remote package entry, and add to impacthead.
 * Returns the calculated action.
 */
static action_t
add_remote_to_impact(Plistarray *impacthead, Pkglist *pkg)
{
	Pkglist *lpkg;
	size_t slot;

	pkg->action = ACTION_NONE;

	/*
	 * If we didn't match any local packages then this is a new package to
	 * install, otherwise set lpkg and calculate the action.
	 */
	TRACE(" |- matching %s over installed packages\n", pkg->rpkg->full);
	if ((lpkg = find_local_pkg(pkg->rpkg->name, pkg->rpkg->name)) == NULL) {
		TRACE(" > recording %s as to install\n", pkg->rpkg->full);
		pkg->action = ACTION_INSTALL;
	} else {
		TRACE("  - found %s\n", lpkg->full);
		pkg->lpkg = lpkg;
		pkg->action = calculate_action(pkg->lpkg, pkg->rpkg);
	}

	slot = pkg_hash_entry(pkg->rpkg->name, impacthead->size);
	SLIST_INSERT_HEAD(&impacthead->head[slot], pkg, next);

	return pkg->action;
}

static void
update_level_if_higher(Pkglist *pkg, int level)
{
	if (pkg->level < level)
		pkg->level = level;
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
 * Compared to installs, upgrades are relatively easy to reason about.  Iterate
 * through all local packages and check each for upgrades.
 */
static Plistarray *
pkg_impact_upgrade(int verbose)
{
	Plistarray *deps, *impacthead;
	Plisthead *supersedes;
	Pkglist *dpkg, *epkg, *lpkg, *p, *save;
	size_t slot;
	int i, l, istty;

	istty = isatty(fileno(stdout));

	impacthead = init_array(PKGS_HASH_SIZE);
	deps = init_array(DEPS_HASH_SIZE);

	for (l = 0; l < LOCAL_PKG_HASH_SIZE; l++) {
	SLIST_FOREACH(lpkg, &l_plisthead[l], next) {
		if (local_pkg_in_impact(impacthead, lpkg))
			continue;

		TRACE("  [+]-impact for %s\n", lpkg->full);
		p = malloc_pkglist();
		p->action = ACTION_NONE;
		p->lpkg = lpkg;
		slot = pkg_hash_entry(lpkg->name, impacthead->size);
		SLIST_INSERT_HEAD(&impacthead->head[slot], p, next);

		/*
		 * Find a remote package that matches our PKGPATH (if we
		 * installed a specific version of e.g. nodejs then we don't
		 * want a newer version with a different PKGPATH to be
		 * considered).
		 *
		 * If there are no matches the package is just skipped, this
		 * can happen if for example the local package is self-built.
		 */
		p->rpkg = find_remote_pkg(lpkg->name, lpkg->name, lpkg->pkgpath);
		if (p->rpkg == NULL) {
			TRACE("   | - no remote match found\n");
			continue;
		}

		/* No upgrade or refresh found, we're done. */
		p->action = calculate_action(lpkg, p->rpkg);
		if (p->action == ACTION_NONE)
			continue;

		/*
		 * If the remote package is being installed, find all of the
		 * remote package dependencies as there may be new or updated
		 * dependencies that need to be installed.
		 */
		if (verbose)
			update_deps_spinner(istty);
		get_depends_recursive(p->rpkg->full, deps, DEPENDS_REMOTE);
	}
	}

	/*
	 * We now have a full list of dependencies for all local packages,
	 * process them in turn, adding to impact list.  As we may have already
	 * seen an entry as part of looping through all packages, but without
	 * its correct dependency depth, update it if we found a deeper path so
	 * that install ordering is correct.
	 */
	for (i = 0; i < deps->size; i++) {
	SLIST_FOREACH_SAFE(dpkg, &deps->head[i], next, save) {
		SLIST_REMOVE(&deps->head[i], dpkg, Pkglist, next);
		if ((epkg = remote_pkg_in_impact(impacthead, dpkg->rpkg))) {
			update_level_if_higher(epkg, dpkg->level);
			free_pkglist_entry(&dpkg);
			continue;
		}
		add_remote_to_impact(impacthead, dpkg);
	}
	}
	free_array(deps);

	/*
	 * Get SUPERSEDES entries matching local packages.  For each affected
	 * package, mark as superseded which will cause it to be removed.
	 */
	if ((supersedes = find_supersedes(impacthead))) {
		SLIST_FOREACH(lpkg, supersedes, next) {
			/*
			 * find_supersedes() ensures lpkg will be set and this
			 * will return a valid entry.
			 */
			p = local_pkg_in_impact(impacthead, lpkg->lpkg);
			p->action = ACTION_SUPERSEDED;
		}
		free_pkglist(&supersedes);
	}

	return impacthead;
}

/*
 * Recursively process a list of packages that are being upgraded, finding
 * their forward and reverse dependencies to ensure they are correctly
 * accounted for.
 *
 * If any do not exist in impacthead, add.
 */
static void
resolve_forward_deps(Plisthead *upgrades, Plistarray *impacthead, Pkglist *pkg)
{
	Plisthead *deps;
	Pkglist *p, *npkg, *save;

	deps = init_head();
	get_depends(pkg->rpkg->full, deps, DEPENDS_REMOTE);

	SLIST_FOREACH_SAFE(p, deps, next, save) {
		SLIST_REMOVE(deps, p, Pkglist, next);
		/*
		 * If we've already seen this package then we're done.
		 */
		if (remote_pkg_in_impact(impacthead, p->rpkg)) {
			free_pkglist_entry(&p);
			continue;
		}

		/*
		 * Otherwise set the dependency to the next level, add to
		 * impact, and if it's going to be an upgrade then add it to
		 * upgrades so it is considered in the next loop.
		 */
		p->level = pkg->level + 1;
		if ((add_remote_to_impact(impacthead, p)) == ACTION_UPGRADE) {
			npkg = malloc_pkglist();
			npkg->ipkg = p;
			SLIST_INSERT_HEAD(upgrades, npkg, next);
		}
	}

	free_pkglist(&deps);
}

static void
resolve_reverse_deps(Plisthead *upgrades, Plistarray *impacthead, Pkglist *pkg)
{
	Plisthead *revdeps;
	Pkglist *p, *npkg, *save;

	revdeps = init_head();
	get_depends(pkg->lpkg->full, revdeps, DEPENDS_REVERSE);

	SLIST_FOREACH_SAFE(p, revdeps, next, save) {
		SLIST_REMOVE(revdeps, p, Pkglist, next);
		/*
		 * If we've already seen this package then we're done.
		 */
		if (local_pkg_in_impact(impacthead, p)) {
			free_pkglist_entry(&p);
			continue;
		}

		/*
		 * Get suitable remote package.  In theory this shouldn't
		 * return NULL, but if it does then there's not much we can do
		 * about it other than log and skip.
		 */
		p->rpkg = find_remote_pkg(p->name, p->name, p->pkgpath);
		if (p->rpkg == NULL) {
			TRACE("ERROR: unable to find remote pkg for %s at %s\n",
			    p->name, p->pkgpath);
			free_pkglist_entry(&p);
			continue;
		}

		/*
		 * Otherwise set the dependency to a lower level, add to
		 * impact, and if it's going to be an upgrade then add it to
		 * upgrades so it is considered in the next loop.
		 */
		p->level = pkg->level - 1;
		if ((add_remote_to_impact(impacthead, p)) == ACTION_UPGRADE) {
			npkg = malloc_pkglist();
			npkg->ipkg = p;
			SLIST_INSERT_HEAD(upgrades, npkg, next);
		}
	}

	free_pkglist(&revdeps);
}

static void
recurse_upgrades(Plisthead *upgrades, Plistarray *impacthead)
{
	Pkglist *save, *u;

	while (!(SLIST_EMPTY(upgrades))) {
		SLIST_FOREACH_SAFE(u, upgrades, next, save) {
			SLIST_REMOVE(upgrades, u, Pkglist, next);
			resolve_forward_deps(upgrades, impacthead, u->ipkg);
			resolve_reverse_deps(upgrades, impacthead, u->ipkg);
			free_pkglist_entry(&u);
		}
	}
}

/*
 * For each package argument on the command line, look for suitable remote
 * packages and their dependencies to install.
 */
static Plistarray *
pkg_impact_install(char **pkgargs, int *rc, int verbose)
{
	Plistarray *deps, *impacthead;
	Plisthead *ipkgs;
	Pkglist *dpkg, *epkg, *rpkg, *p, *r, *save;
	char **arg, *pkgname = NULL;
	int i, istty, rv;

	istty = isatty(fileno(stdout));

	impacthead = init_array(PKGS_HASH_SIZE);

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
		if ((p = remote_pkg_in_impact(impacthead, rpkg))) {
			p->keep = 1;
			continue;
		}

		p = malloc_pkglist();
		p->keep = 1;
		p->rpkg = rpkg;
		add_remote_to_impact(impacthead, p);

		/*
		 * Get all recursive dependencies of the remote package and
		 * calculate their actions based on whether they match a local
		 * package or not.
		 */
		if (verbose)
			update_deps_spinner(istty);
		deps = init_array(DEPS_HASH_SIZE);
		get_depends_recursive(p->rpkg->full, deps, DEPENDS_REMOTE);
		for (i = 0; i < deps->size; i++) {
		SLIST_FOREACH_SAFE(dpkg, &deps->head[i], next, save) {
			SLIST_REMOVE(&deps->head[i], dpkg, Pkglist, next);
			if ((epkg = remote_pkg_in_impact(impacthead, dpkg->rpkg))) {
				update_level_if_higher(epkg, dpkg->level);
				free_pkglist_entry(&dpkg);
				continue;
			}
			add_remote_to_impact(impacthead, dpkg);
		}
		}
		free_array(deps);
	}

	/*
	 * For any package that is to be upgraded, we need to consider its
	 * direct local reverse dependencies, as they will need to be refreshed
	 * for any shared library bumps etc.
	 *
	 * This is only for install operations, as upgrades will already
	 * consider every package.
	 */
	ipkgs = init_head();

	/*
	 * First, get all upgrade packages from our current state.
	 */
	for (i = 0; i < impacthead->size; i++) {
		SLIST_FOREACH(dpkg, &impacthead->head[i], next) {
			if (dpkg->action != ACTION_UPGRADE)
				continue;
			r = malloc_pkglist();
			r->ipkg = dpkg;
			SLIST_INSERT_HEAD(ipkgs, r, next);
		}
	}

	/*
	 * Now recursively process them until they are all added to impacthead.
	 */
	recurse_upgrades(ipkgs, impacthead);
	free_pkglist(&ipkgs);

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
pkg_impact(char **pkgargs, int *rc, int verbose)
{
	Plistarray *pkgs;
	Plisthead *impacthead;
	Pkglist *p, *tmpp;
	int i, istty;

	istty = isatty(fileno(stdout));

	TRACE("[>]-entering impact\n");
	if (verbose)
		start_deps_spinner(istty);

	if (pkgargs)
		pkgs = pkg_impact_install(pkgargs, rc, verbose);
	else
		pkgs = pkg_impact_upgrade(verbose);

	TRACE("[<]-leaving impact\n");
	if (verbose)
		finish_deps_spinner(istty);

	impacthead = init_head();

	/*
	 * Remove ACTION_NONE entries to simplify processing in later stages,
	 * leaving only actionable entries.
	 *
	 * Convert back to a single layer for simpler processing by callers.
	 */
	for (i = 0; i < pkgs->size; i++) {
		SLIST_FOREACH_SAFE(p, &pkgs->head[i], next, tmpp) {
			SLIST_REMOVE(&pkgs->head[i], p, Pkglist, next);

			if (p->action == ACTION_NONE) {
				free_pkglist_entry(&p);
			} else {
				SLIST_INSERT_HEAD(impacthead, p, next);
			}
		}
	}
	free_array(pkgs);

	if (SLIST_EMPTY(impacthead))
		free_pkglist(&impacthead);

	return impacthead;
}
