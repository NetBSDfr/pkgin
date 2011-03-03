/* $Id: depends.c,v 1.1 2011/03/03 14:43:12 imilh Exp $ */

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

void
free_deptree(Deptreehead *deptreehead)
{
	Pkgdeptree *pdp;

	if (deptreehead == NULL)
		return;

	while (!SLIST_EMPTY(deptreehead)) {
		pdp = SLIST_FIRST(deptreehead);
		SLIST_REMOVE_HEAD(deptreehead, next);
		XFREE(pdp->depname);
		XFREE(pdp);
	}
}

/* match dependency extension */
char *
match_dep_ext(char *depname, const char *ext)
{
	char *pdep = NULL;

	for (; *ext != 0; ext++)
		if ((pdep = strrchr(depname, *ext)) != NULL)
			break;

	return pdep;
}

/* basic full package format detection */
int
exact_pkgfmt(const char *pkgname)
{
	char	*p;

	if ((p = strrchr(pkgname, '-')) == NULL)
		return 0;

	p++;

	/* naive assumption, will fail with foo-100bar, hopefully, there's
	 * only a few packages needing to be fully specified
	 */
	return isdigit((int)*p);
}

/* sqlite callback
 * DIRECT_DEPS or REVERSE_DEPS result, feeds a Pkgdeptree SLIST
 * Deptreehead is the head of Pkgdeptree
 */
static int
pdb_rec_direct_deps(void *param, int argc, char **argv, char **colname)
{
	int			argvlen;
	char		*depname;
	Pkgdeptree	*deptree, *pdp;
	Deptreehead	*pdphead = (Deptreehead *)param;

	if (argv == NULL)
		return PDB_ERR;

	depname = end_expr(plisthead, argv[0]); /* foo| */
	argvlen = strlen(depname);

	/* dependency already recorded, do not insert on list  */
	SLIST_FOREACH(pdp, pdphead, next) {
		if (strlen(pdp->matchname) + 1 == argvlen &&
		    strncmp(depname, pdp->matchname, argvlen - 1) == 0) {
			XFREE(depname);
			/* proceed to next result */
			return PDB_OK;
		}
	}

	/* remove delimiter */
	depname[argvlen - 1] = '\0';

	XMALLOC(deptree, sizeof(Pkgdeptree));
	XSTRDUP(deptree->depname, argv[0]);
	deptree->matchname = depname;
	deptree->computed = 0;
	deptree->level = 0;
	/* used in LOCAL_REVERSE_DEPS / autoremove.c */
	if (argc > 1 && argv[1] != NULL)
		deptree->pkgkeep = 1;
	else
		deptree->pkgkeep = 0;

	SLIST_INSERT_HEAD(pdphead, deptree, next);

	return PDB_OK;
}

/* recursively parse dependencies: this is our central function  */
void
full_dep_tree(const char *pkgname, const char * depquery, Deptreehead *pdphead)
{
	Pkgdeptree	*pdp;
	int		level;
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
	if (pkgindb_doquery(query, pdb_rec_direct_deps, pdphead) != 0)
		return;

	while (SLIST_FIRST(pdphead)->level == 0) {
		SLIST_FOREACH(pdp, pdphead, next) {
			if (pdp->level != 0)
			    	break;
			pdp->level = level;
			snprintf(query, BUFSIZ, depquery, pdp->matchname);
			pkgindb_doquery(query, pdb_rec_direct_deps, pdphead);

#if 0
			printf("%i: p: %s, l: %d\n", level, pdp->depname,
			    pdp->level);
#endif
		} /* SLIST_FOREACH */
		++level;
	}

	free_pkglist(plisthead);
}

void
show_direct_depends(const char *pkgname)
{
	char		query[BUFSIZ];
	Pkgdeptree	*pdp;
	Deptreehead	deptreehead;
	Pkglist		*mapplist;

	if ((plisthead = rec_pkglist(REMOTE_PKGS_QUERY)) == NULL) {
		printf("%s\n", MSG_EMPTY_AVAIL_PKGLIST);
		return;
	}

	/* warn if more than one package with this name is available */
	if (count_samepkg(plisthead, pkgname) < 2) {
		SLIST_INIT(&deptreehead);

		if (exact_pkgfmt(pkgname))
			snprintf(query, BUFSIZ, EXACT_DIRECT_DEPS, pkgname);
		else
			snprintf(query, BUFSIZ, DIRECT_DEPS, pkgname);

		if (pkgindb_doquery(query, pdb_rec_direct_deps, &deptreehead) == 0) {
			printf(MSG_DIRECT_DEPS_FOR, pkgname);
			SLIST_FOREACH(pdp, &deptreehead, next) {
				if (package_version && 
					(mapplist = map_pkg_to_dep(plisthead, pdp->depname))
					!= NULL)
					printf("\t%s\n", mapplist->pkgname);
				else
					printf("\t%s\n", pdp->depname);
			}
			free_deptree(&deptreehead);
		}
	}
	free_pkglist(plisthead);
}

void
show_full_dep_tree(const char *pkgname, const char *depquery, const char *msg)
{
	Pkgdeptree	*pdp;
	Deptreehead	deptreehead; /* replacement for SLIST_HEAD() */
	Pkglist		*mapplist;
	int			count;
	const char	*pkgquery;

	if (depquery == LOCAL_REVERSE_DEPS)
		pkgquery = LOCAL_PKGS_QUERY;
	else
		pkgquery = REMOTE_PKGS_QUERY;

	if ((plisthead = rec_pkglist(pkgquery)) == NULL)
		errx(EXIT_FAILURE, MSG_EMPTY_AVAIL_PKGLIST);

	count = count_samepkg(plisthead, pkgname);

	/* free plisthead now so it is NULL for full_dep_tree() */
	free_pkglist(plisthead);

	if (count > 1)
		return;

	SLIST_INIT(&deptreehead);

	printf(msg, pkgname);
	full_dep_tree(pkgname, depquery, &deptreehead);

	/* record plisthead once again for map_pkg_to_dep() */
	plisthead = rec_pkglist(pkgquery);

	SLIST_FOREACH(pdp, &deptreehead, next) {
		if (package_version && 
			(mapplist = map_pkg_to_dep(plisthead, pdp->depname)) != NULL)
			printf("\t%s\n", mapplist->pkgname);
		else
			printf("\t%s\n", pdp->depname);
	}

	free_pkglist(plisthead);
	free_deptree(&deptreehead);
}
