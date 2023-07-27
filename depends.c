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
 * SQLite callback for LOCAL_DIRECT_DEPS and REMOTE_DIRECT_DEPS, given a DEPENDS
 * pattern find packages that match and adds them to an SLIST.
 *
 * May be called in a recursive context, so checks to see if the entry has
 * already been added and skips if so.
 *
 * argv0: DEPENDS pattern
 */
static int
record_depend(void *param, int argc, char **argv, char **colname)
{
	Plisthead *deps = (Plisthead *)param;
	Pkglist *d, *p;

	if (argv == NULL)
		return PDB_ERR;

	/*
	 * If we've already searched for this exact DEPENDS pattern then return
	 * early and do not add to deps.
	 */
	SLIST_FOREACH(d, deps, next) {
		if (strcmp(d->pattern, argv[0]) == 0) {
			TRACE(" < dependency pattern %s already recorded\n",
			    d->pattern);
			return PDB_OK;
		}
	}

	d = malloc_pkglist();
	d->pattern = xstrdup(argv[0]);

	/*
	 * Check the column name to see if we're looking for local or remote
	 * packages.  The queries use "AS L" and "AS R" so we can just look at
	 * the first character for optimal processing.
	 *
	 * If we found the same package already via a different DEPENDS match,
	 * then this entry does not need to be processed.
	 */
	switch (colname[0][0]) {
	case 'L':
		/*
		 * Find matching entry in the local package list.  It should
		 * not be possible for this to return NULL other than database
		 * corruption, but handle it anyway by not creating an entry
		 * for it.
		 */
		if ((d->lpkg = find_local_pkg_match(argv[0])) == NULL)
			return PDB_ERR;

		SLIST_FOREACH(p, deps, next) {
			if (strcmp(p->lpkg->full, d->lpkg->full) == 0) {
				d->skip = 1;
				break;
			}
		}
		break;
	case 'R':
		/*
		 * Find the best matching package for the DEPENDS pattern, with
		 * no specific PKGPATH requested.  It is possible that this
		 * returns NULL, for example self-constructed packages with
		 * incorrect DEPENDS, and this is handled by upper layers.
		 */
		if ((d->rpkg = find_pkg_match(argv[0], NULL)) != NULL) {
			SLIST_FOREACH(p, deps, next) {
				if (strcmp(p->rpkg->full, d->rpkg->full) == 0) {
					d->skip = 1;
					break;
				}
			}
		}
		break;
	default:
		return PDB_ERR;
	}

	SLIST_INSERT_HEAD(deps, d, next);

	return PDB_OK;
}

/*
 * SQLite callback for LOCAL_REVERSE_DEPS, records REQUIRED_BY entries for a
 * package (its reverse dependencies) to a package SLIST.
 *
 * May be called in a recursive context, so checks to see if the entry has
 * already been added and skips if so.
 *
 * argv0: LOCAL_REQUIRED_BY.REQUIRED_BY
 * argv1: LOCAL_PKG.PKGNAME
 * argv2: LOCAL_PKG.PKG_KEEP
 */
static int
record_reverse_depend(void *param, int argc, char **argv, char **colname)
{
	Plisthead *deps = (Plisthead *)param;
	Pkglist *d;

	if (argv == NULL)
		return PDB_ERR;

	/* Skip if already in the SLIST */
	SLIST_FOREACH(d, deps, next) {
		if (strcmp(argv[0], d->lpkg->full) == 0) {
			TRACE(" < dependency %s already recorded\n",
			    d->lpkg->full);
			return PDB_OK;
		}
	}

	d = malloc_pkglist();

	/*
	 * Find matching entry in the local package list.  It should not be
	 * possible for this to return NULL other than database corruption, but
	 * handle it anyway by not creating an entry for it.
	 */
	if ((d->lpkg = find_local_pkg_match(argv[0])) == NULL)
		return PDB_ERR;

	d->keep = (argv[2] == NULL) ? 0 : 1;
	SLIST_INSERT_HEAD(deps, d, next);

	return PDB_OK;
}

/*
 * Record a single level of dependencies to a supplied SLIST.
 */
void
get_depends(const char *pkgname, Plisthead *deps, depends_t type)
{
	char query[BUFSIZ];

	switch (type) {
	case DEPENDS_LOCAL:
		sqlite3_snprintf(BUFSIZ, query, LOCAL_DIRECT_DEPS, pkgname);
		pkgindb_doquery(query, record_depend, deps);
		break;
	case DEPENDS_REMOTE:
		sqlite3_snprintf(BUFSIZ, query, REMOTE_DIRECT_DEPS, pkgname);
		pkgindb_doquery(query, record_depend, deps);
		break;
	case DEPENDS_REVERSE:
		sqlite3_snprintf(BUFSIZ, query, LOCAL_REVERSE_DEPS, pkgname);
		pkgindb_doquery(query, record_reverse_depend, deps);
		break;
	}
}

/*
 * Recursively record forward or reverse dependencies for a package to a
 * supplied SLIST.
 *
 * pkgname must be a fully-specified package name including version.
 */
void
get_depends_recursive(const char *pkgname, Plisthead *deps, depends_t type)
{
	Pkglist *d;
	int level;
	char *p;

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
	 * Get first level of dependencies.  If nothing returned then we're
	 * done.
	 */
	get_depends(pkgname, deps, type);
	if (SLIST_EMPTY(deps))
		return;

	/*
	 * Now recursively iterate over dependencies as they are inserted.
	 */
	level = 1;
	while (SLIST_FIRST(deps)->level == 0) {
		TRACE(" > looping through dependency level %d\n", level);
		SLIST_FOREACH(d, deps, next) {
			/*
			 * If we hit a package with a level already set then
			 * we've finished processing this current level, as
			 * entries are always added at the head.
			 */
			if (d->level)
				break;
			d->level = level;

			switch (type) {
			case DEPENDS_LOCAL:
			case DEPENDS_REVERSE:
				p = d->lpkg->full;
				break;
			case DEPENDS_REMOTE:
				p = d->rpkg->full;
				break;
			}

			/*
			 * Already handled this package via an alternate
			 * DEPENDS match.
			 */
			if (d->skip) {
				TRACE(" < dependency package %s "
				    "already recorded\n", p);
				continue;
			}

			TRACE(" > recording %s dependencies "
			    "(will be level %d)\n", p, level + 1);
			get_depends(p, deps, type);
		}
		level++;
	}
	TRACE("[<]-leaving depends\n");
}

int
show_direct_depends(const char *pkgarg)
{
	Plisthead	*pkghead;
	Pkglist		*p;
	char		*pkgname;

	if (SLIST_EMPTY(&r_plisthead)) {
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
			printf("\t%s\n", p->pattern);
	}
	free_pkglist(&pkghead);

done:
	XFREE(pkgname);

	return EXIT_SUCCESS;
}

int
show_full_dep_tree(const char *pkgarg)
{
	Plisthead	*pkghead;
	Pkglist		*p;
	char		*pkgname;

        if (SLIST_EMPTY(&r_plisthead))
		errx(EXIT_FAILURE, MSG_EMPTY_AVAIL_PKGLIST);

	if ((pkgname = unique_pkg(pkgarg, REMOTE_PKG)) == NULL) {
		fprintf(stderr, MSG_PKG_NOT_AVAIL, pkgarg);
		return EXIT_FAILURE;
	}

	pkghead = init_head();
	get_depends_recursive(pkgname, pkghead, DEPENDS_REMOTE);

	printf(MSG_FULLDEPTREE, pkgname);
	SLIST_FOREACH(p, pkghead, next) {
		if (package_version)
			printf("\t%s\n", p->rpkg->full);
		else
			printf("\t%s\n", p->pattern);
	}

	XFREE(pkgname);
	free_pkglist(&pkghead);

	return EXIT_SUCCESS;
}

int
show_rev_dep_tree(const char *match)
{
	Plisthead	*deps;
	Pkglist		*p;

	if ((p = find_local_pkg_match(match)) == NULL) {
		fprintf(stderr, MSG_PKG_NOT_INSTALLED, match);
		return EXIT_FAILURE;
	}

	deps = init_head();
	get_depends_recursive(p->full, deps, DEPENDS_REVERSE);

	printf(MSG_REVDEPTREE, p->full);
	SLIST_FOREACH(p, deps, next) {
		printf("\t%s\n", p->lpkg->full);
	}

	free_pkglist(&deps);
	return EXIT_SUCCESS;
}
