/* $Id: depends.c,v 1.1.1.1.2.7 2011/08/19 23:41:55 imilh Exp $ */

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

static Plisthead *plisthead = NULL;

/**
 * match dependency extension
 */
char *
match_dep_ext(char *depname, const char *ext)
{
	char *pdep = NULL;

	for (; *ext != 0; ext++)
		if ((pdep = strrchr(depname, *ext)) != NULL)
			break;

	return pdep;
}

/**
 * recursively parse dependencies: this is our central function
 */
void
full_dep_tree(const char *pkgname, const char *depquery, Plisthead *pdphead)
{
	Pkglist		*pdp;
	int			level;
	char		query[BUFSIZ];

	query[0] = '\0';
	if (depquery == DIRECT_DEPS) {
		/* querying remote packages, load remote packages list */
		plisthead = rec_pkglist(REMOTE_PKGS_QUERY);

		/* first package to recurse on and exact pkg name, this is an
		 * exact match due to many versions of the package
		 */
		if (exact_pkgfmt(pkgname))
			snprintf(query, BUFSIZ, EXACT_DIRECT_DEPS, pkgname);

	} else if (depquery == LOCAL_REVERSE_DEPS ||
		   depquery == LOCAL_DIRECT_DEPS) {
		/* querying local packages, load local packages list */
		plisthead = rec_pkglist(LOCAL_PKGS_QUERY);
	} else {
	    	printf("oops\n");
	    	return;
	}
	level = 1;

	/* getting direct dependencies */
	if (query[0] == '\0')
	    	snprintf(query, BUFSIZ, depquery, pkgname);
	if (pkgindb_doquery(query, pdb_rec_depends, pdphead) != 0)
		return;

	while (SLIST_FIRST(pdphead)->level == 0) {
		SLIST_FOREACH(pdp, pdphead, next) {
			if (pdp->level != 0)
			    	break;
			pdp->level = level;
			snprintf(query, BUFSIZ, depquery, pdp->name);
			pkgindb_doquery(query, pdb_rec_depends, pdphead);
#if 0
			printf("%i: p: %s, l: %d\n", level, pdp->depname,
			    pdp->level);
#endif
		} /* SLIST_FOREACH */
		++level;
	}

	free_pkglist(plisthead, LIST);
}

void
show_direct_depends(const char *pkgarg)
{
	char		*pkgname, query[BUFSIZ];
	Pkglist		*pdp, *mapplist;
	Plisthead	*deptreehead;

	if ((plisthead = rec_pkglist(REMOTE_PKGS_QUERY)) == NULL) {
		printf("%s\n", MSG_EMPTY_AVAIL_PKGLIST);
		return;
	}

	if ((pkgname = unique_pkg(pkgarg)) == NULL) {
		fprintf(stderr, MSG_PKG_NOT_AVAIL, pkgname);
		return;
	}

	deptreehead = init_head();

	snprintf(query, BUFSIZ, EXACT_DIRECT_DEPS, pkgname);

	if (pkgindb_doquery(query, pdb_rec_depends, deptreehead) == 0) {
		printf(MSG_DIRECT_DEPS_FOR, pkgname);
		SLIST_FOREACH(pdp, deptreehead, next) {
			if (package_version && 
				(mapplist = map_pkg_to_dep(plisthead, pdp->depend))
				!= NULL)
				printf("\t%s\n", mapplist->full);
			else
				printf("\t%s\n", pdp->depend);
		}
		free_pkglist(deptreehead, DEPTREE);
	}
	XFREE(pkgname);
	free_pkglist(plisthead, LIST);
}

void
show_full_dep_tree(const char *pkgarg, const char *depquery, const char *msg)
{
	Pkglist		*pdp, *mapplist;
	Plisthead	*deptreehead; /* replacement for SLIST_HEAD() */
	const char	*pkgquery;
	char		*pkgname = NULL;

	if (depquery == LOCAL_REVERSE_DEPS) {
		pkgquery = LOCAL_PKGS_QUERY;
		XSTRDUP(pkgname, pkgarg);
	} else {
		pkgquery = REMOTE_PKGS_QUERY;
		pkgname = unique_pkg(pkgarg);
	}

	if (pkgname == NULL) {
		fprintf(stderr, MSG_PKG_NOT_AVAIL, pkgarg);
		return;
	}

	if ((plisthead = rec_pkglist(pkgquery)) == NULL)
		errx(EXIT_FAILURE, MSG_EMPTY_AVAIL_PKGLIST);

	/* free plisthead now so it is NULL for full_dep_tree() */
	free_pkglist(plisthead, LIST);

	deptreehead = init_head();

	printf(msg, pkgname);
	full_dep_tree(pkgname, depquery, deptreehead);

	/* record plisthead once again for map_pkg_to_dep() */
	plisthead = rec_pkglist(pkgquery);

	SLIST_FOREACH(pdp, deptreehead, next) {
		if (package_version && 
			(mapplist = map_pkg_to_dep(plisthead, pdp->depend)) != NULL)
			printf("\t%s\n", mapplist->full);
		else
			printf("\t%s\n", pdp->depend);
	}

	XFREE(pkgname);
	free_pkglist(plisthead, LIST);
	free_pkglist(deptreehead, DEPTREE);
}
