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
 * SQLite callback for LOCAL_DIRECT_DEPENDS and REMOTE_DIRECT_DEPENDS, add a
 * DEPENDS pattern and optional PKGBASE to an slist.
 *
 * argv0: DEPENDS pattern
 * argv1: PKGBASE, may be NULL if it cannot be determined from pattern
 */
static int
record_depends(void *param, int argc, char **argv, char **colname)
{
	Plisthead *depends = (Plisthead *)param;
	Pkglist *d;

	if (argv == NULL)
		return PDB_ERR;

	d = malloc_pkglist();
	d->patterns = xmalloc(2 * sizeof(char *));
	d->patterns[0] = xstrdup(argv[0]);
	d->patterns[1] = NULL;
	d->patcount = 1;
	d->name = (argv[1]) ? xstrdup(argv[1]) : NULL;
	SLIST_INSERT_HEAD(depends, d, next);

	return PDB_OK;
}

/*
 * SQLite callback for LOCAL_REVERSE_DEPENDS, records REQUIRED_BY entries for a
 * package (its reverse dependencies) to an slist.
 *
 * argv0: local_required_by.required_by
 * argv1: local_pkg.pkgname
 * argv2: local_pkg.pkg_keep
 */
static int
record_reverse_depends(void *param, int argc, char **argv, char **colname)
{
	Plisthead *depends = (Plisthead *)param;
	Pkglist *d;

	if (argv == NULL)
		return PDB_ERR;

	d = malloc_pkglist();
	d->full = xstrdup(argv[0]);
	d->name = xstrdup(argv[1]);
	d->keep = (argv[2] == NULL) ? 0 : 1;
	SLIST_INSERT_HEAD(depends, d, next);

	return PDB_OK;
}

/*
 * When performing recursive lookups, it is critical for performance that we do
 * not perform any unnecessary package searches, and so for initial dependency
 * queries we only fetch the matches themselves and first check that we haven't
 * already processed them.
 */
static void
get_depends_matches(const char *pkgname, Plisthead *depends, depends_t type)
{
	char query[BUFSIZ];

	switch (type) {
	case DEPENDS_LOCAL:
		sqlite3_snprintf(BUFSIZ, query, LOCAL_DIRECT_DEPENDS, pkgname);
		pkgindb_doquery(query, record_depends, depends);
		break;
	case DEPENDS_REMOTE:
		sqlite3_snprintf(BUFSIZ, query, REMOTE_DIRECT_DEPENDS, pkgname);
		pkgindb_doquery(query, record_depends, depends);
		break;
	case DEPENDS_REVERSE:
		sqlite3_snprintf(BUFSIZ, query, LOCAL_REVERSE_DEPENDS, pkgname);
		pkgindb_doquery(query, record_reverse_depends, depends);
		break;
	}
}

/*
 * Add a new DEPENDS pattern to the list of a package if we have found it via
 * different patterns.
 */
static void
add_new_pattern(Pkglist *p, const char *pattern)
{
	TRACE("    adding new DEPENDS match %s\n", pattern);
	p->patterns = xrealloc(p->patterns, (p->patcount + 2) * sizeof(char *));
	p->patterns[p->patcount++] = xstrdup(pattern);
	p->patterns[p->patcount] = NULL;
}

/*
 * Update the level for an existing pkg entry if we've since found it via a
 * deeper dependency path.  This ensures correct install ordering.
 */
static void
update_pkg_level(Pkglist *cur, Pkglist *new)
{
	if (cur->level < new->level) {
		TRACE("   update level %d -> %d\n", cur->level, new->level);
		cur->level = new->level;
	}
}

static Pkglist *
new_local_depend(Pkglist *pkg, Plisthead *depends, int depsize)
{
	Pkglist *epkg, *lpkg;

	/*
	 * Have we already seen this DEPENDS pattern?  If so update its level
	 * if previously seen via a shallower dependency tree.
	 */
	if ((epkg = pattern_in_pkglist(pkg->patterns[0], depends, depsize))) {
		TRACE(" < dependency %s already recorded\n", pkg->patterns[0]);
		update_pkg_level(epkg, pkg);
		return NULL;
	}

	/*
	 * It shouldn't be possible for a local package to not find its own
	 * dependencies, other than pkgdb corruption, so log and continue.
	 */
	if ((lpkg = find_local_pkg(pkg->patterns[0], pkg->name)) == NULL) {
		TRACE("ERROR: no match found for %s, corrupt pkgdb?\n",
		    pkg->patterns[0]);
		return NULL;
	}

	/*
	 * This package name is already in the depends list via a different
	 * DEPENDS match.  Add this match and update its level if ours is
	 * higher to ensure correct install ordering.
	 */
	if ((epkg = pkgname_in_local_pkglist(lpkg->full, depends, depsize))) {
		TRACE(" < package %s already recorded\n", lpkg->full);
		add_new_pattern(epkg, pkg->patterns[0]);
		update_pkg_level(epkg, pkg);
		return NULL;
	}

	pkg->lpkg = lpkg;
	return lpkg;
}

static Pkglist *
new_remote_depend(Pkglist *pkg, Plisthead *depends, int depsize)
{
	Pkglist *rpkg, *epkg;

	/*
	 * Have we already seen this DEPENDS pattern?  If so update its level
	 * if previously seen via a shallower dependency tree.
	 */
	if ((epkg = pattern_in_pkglist(pkg->patterns[0], depends, depsize))) {
		TRACE(" < dependency %s already recorded\n", pkg->patterns[0]);
		update_pkg_level(epkg, pkg);
		return NULL;
	}

	/*
	 * Generally it should not be possible for a remote package to not find
	 * its DEPENDS, and certainly not if using pbulk etc, but it can
	 * happen, for example with self-built repositories.
	 */
	rpkg = find_remote_pkg(pkg->patterns[0], pkg->name, NULL);
	if (rpkg == NULL) {
		TRACE(" < ERROR no match found for %s\n", pkg->patterns[0]);
		return NULL;
	}

	/*
	 * This package name is already in the depends list via a different
	 * DEPENDS match.  Add this match and update its level if ours is
	 * higher to ensure correct install ordering.
	 */
	if ((epkg = pkgname_in_remote_pkglist(rpkg->full, depends, depsize))) {
		TRACE(" < package %s already recorded\n", rpkg->full);
		add_new_pattern(epkg, pkg->patterns[0]);
		update_pkg_level(epkg, pkg);
		return NULL;
	}

	pkg->rpkg = rpkg;
	return rpkg;
}

static Pkglist *
new_reverse_depend(Pkglist *pkg, Plisthead *depends, int depsize)
{
	Pkglist *epkg, *lpkg;

	/*
	 * For reverse dependencies we already have the full package name, so
	 * can perform the cheaper check first for duplicates.
	 */
	if ((epkg = pkgname_in_local_pkglist(pkg->full, depends, depsize))) {
		TRACE(" < package %s already recorded\n", pkg->full);
		return NULL;
	}

	/*
	 * Retrieve the lpkg entry for this package.  This really should never
	 * return NULL, except for corruption?  Log an error.
	 */
	if ((lpkg = find_local_pkg(pkg->full, pkg->name)) == NULL) {
		TRACE(" < ERROR no lpkg entry for %s\n", pkg->full);
		TRACE("   possible pkgdb corruption?\n");
		return NULL;
	}

	pkg->lpkg = lpkg;
	return lpkg;
}

static const char *
new_depend(Pkglist *dep, Plisthead *depends, int depsize, depends_t type)
{
	Pkglist *d = NULL;

	switch (type) {
	case DEPENDS_LOCAL:
		d = new_local_depend(dep, depends, depsize);
		break;
	case DEPENDS_REMOTE:
		d = new_remote_depend(dep, depends, depsize);
		break;
	case DEPENDS_REVERSE:
		d = new_reverse_depend(dep, depends, depsize);
		break;
	}

	if (d == NULL)
		return NULL;

	return d->full;
}

/*
 * Fetch dependencies for a package, adding entries to the supplied Plisthead.
 */
void
get_depends(const char *pkgname, Plisthead *depends, depends_t type)
{
	Plisthead *deps;
	Pkglist *d, *save;

	/*
	 * Retrieve DEPENDS or REQUIRED_BY entries for this pattern or package.
	 */
	deps = init_head();
	get_depends_matches(pkgname, deps, type);

	/*
	 * For each entry, process and add to Plisthead if new.
	 */
	SLIST_FOREACH_SAFE(d, deps, next, save) {
		SLIST_REMOVE(deps, d, Pkglist, next);
		if ((new_depend(d, depends, 1, type)) == NULL) {
			free_pkglist_entry(&d);
			continue;
		}
		SLIST_INSERT_HEAD(depends, d, next);
	}
	free_pkglist(&deps);
}

/*
 * Recursively fetch dependencies for a package to a supplied Plistarray.
 */
void
get_depends_recursive(const char *pkgname, Plistarray *depends, depends_t type)
{
	Plisthead *deps, *dephead;
	Pkglist *d, *tmpd;
	const char *nextpkg;
	int level, slot, size;

	TRACE("[>]-entering depends\n");

	switch (type) {
	case DEPENDS_LOCAL:
		TRACE("[+]-forward local dependencies for %s\n", pkgname);
		break;
	case DEPENDS_REMOTE:
		TRACE("[+]-forward remote dependencies for %s\n", pkgname);
		break;
	case DEPENDS_REVERSE:
		TRACE("[+]-local reverse dependencies for %s\n", pkgname);
		break;
	}

	/*
	 * Get first level of DEPENDS.  If nothing returned then we're done.
	 */
	deps = init_head();
	get_depends_matches(pkgname, deps, type);
	if (SLIST_EMPTY(deps)) {
		free_pkglist(&deps);
		return;
	}

	/*
	 * Now recursively iterate over dependencies as they are inserted,
	 * removing from deps and adding to depends if not seen before.
	 */
	level = 1;
	while (!(SLIST_EMPTY(deps)) && SLIST_FIRST(deps)->level == 0) {
		TRACE(" > looping through dependency level %d\n", level);
		SLIST_FOREACH_SAFE(d, deps, next, tmpd) {
			SLIST_REMOVE(deps, d, Pkglist, next);
			d->level = level;
			/*
			 * Direct hash lookup if name is available, otherwise
			 * the full array needs to be traversed.
			 */
			slot = (d->name) ?
			    pkg_hash_entry(d->name, depends->size) : 0;
			size = (d->name) ? 1 : 0;
			dephead = &depends->head[slot];

			nextpkg = new_depend(d, dephead, size, type);
			if (nextpkg == NULL) {
				free_pkglist_entry(&d);
				continue;
			}

			SLIST_INSERT_HEAD(dephead, d, next);

			TRACE(" > recording %s dependencies "
			    "(will be level %d)\n", nextpkg, level + 1);
			get_depends_matches(nextpkg, deps, type);
		}
		level++;
	}
	TRACE("[<]-leaving depends\n");
	free_pkglist(&deps);
}

int
show_direct_depends(const char *pkgarg)
{
	Plisthead	*pkghead;
	Pkglist		*p;
	char		*pkgname;

	if (is_empty_remote_pkglist()) {
		printf("%s\n", MSG_EMPTY_AVAIL_PKGLIST);
		return EXIT_FAILURE;
	}

	if ((pkgname = unique_pkg(pkgarg, REMOTE_PKG)) == NULL) {
		fprintf(stderr, MSG_PKG_NOT_AVAIL, pkgarg);
		return EXIT_FAILURE;
	}

	pkghead = init_head();
	get_depends(pkgname, pkghead, DEPENDS_REMOTE);

	if (SLIST_EMPTY(pkghead))
		goto done;

	printf(MSG_DIRECT_DEPS_FOR, pkgname);
	SLIST_FOREACH(p, pkghead, next) {
		if (package_version)
			printf("\t%s\n", p->rpkg->full);
		else
			printf("\t%s\n", p->patterns[0]);
	}
	free_pkglist(&pkghead);

done:
	XFREE(pkgname);

	return EXIT_SUCCESS;
}

int
show_full_dep_tree(const char *pkgarg)
{
	Plistarray	*pkghead;
	Pkglist		*p;
	char		*pkgname;

        if (is_empty_remote_pkglist())
		errx(EXIT_FAILURE, MSG_EMPTY_AVAIL_PKGLIST);

	if ((pkgname = unique_pkg(pkgarg, REMOTE_PKG)) == NULL) {
		fprintf(stderr, MSG_PKG_NOT_AVAIL, pkgarg);
		return EXIT_FAILURE;
	}

	pkghead = init_array(1);
	get_depends_recursive(pkgname, pkghead, DEPENDS_REMOTE);

	printf(MSG_FULLDEPTREE, pkgname);
	SLIST_FOREACH(p, pkghead->head, next) {
		if (package_version)
			printf("\t%s\n", p->rpkg->full);
		else
			printf("\t%s\n", p->patterns[0]);
	}

	XFREE(pkgname);
	free_array(pkghead);

	return EXIT_SUCCESS;
}

int
show_rev_dep_tree(const char *match)
{
	Plistarray	*deps;
	Pkglist		*p;

	if ((p = find_local_pkg(match, NULL)) == NULL) {
		fprintf(stderr, MSG_PKG_NOT_INSTALLED, match);
		return EXIT_FAILURE;
	}

	deps = init_array(1);
	get_depends_recursive(p->full, deps, DEPENDS_REVERSE);

	printf(MSG_REVDEPTREE, p->full);
	SLIST_FOREACH(p, deps->head, next) {
		printf("\t%s\n", p->lpkg->full);
	}

	free_array(deps);
	return EXIT_SUCCESS;
}
