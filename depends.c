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

/**
 * \fn full_dep_tree
 *
 * \brief recursively parse dependencies: this is our central function
 */
void
full_dep_tree(const char *pkgname, const char *depquery, Plisthead *pdphead)
{
	Pkglist		*pdp;
	int			level;
	char		query[BUFSIZ] = "";

	query[0] = '\0';
	if (depquery == DIRECT_DEPS) {
		/* first package to recurse on and exact pkg name, this is an
		 * exact match due to many versions of the package
		 */
		if (exact_pkgfmt(pkgname))
			snprintf(query, BUFSIZ, EXACT_DIRECT_DEPS, pkgname);
	} /* else, LOCAL_REVERSE_DEPS */

	TRACE("[>]-entering depends\n");

	/* getting direct dependencies */
	if (query[0] == '\0')
	    	snprintf(query, BUFSIZ, depquery, pkgname);

	TRACE("[+]-dependencies for %s (query: %s)\n", pkgname, query);

	/* first level of dependency for pkgname */
	if (pkgindb_doquery(query, pdb_rec_depends, pdphead) == PDB_ERR)
		return;

	level = 1;
	/*
	 * loop until head first member's level is == 0
	 * the first loop parses the first range of dependencies and set them to
	 * 1. If anything with a level == 0 appears in head, it is provided by
	 * pkgindb_doquery() from the following loop, its level  will then be
	 * set to level + 1 on the next pass.
	 * Example:
	 * sfd eterm
	 * . First pass:
	 * perl, pdp->level = 1   |
	 * libast, pdp->level = 1 |- insert their deps in head with level = 0
	 * imlib2, pdp->level = 1 |
	 * . Second pass: libast and imlib inserted new deps in head, with
     *                pdp->level = 0, level is now 2 (++level)
	 * pcre, pdp->level = 2
	 * imlib2, pdp->level = 2
	 *
	 */
	while (SLIST_FIRST(pdphead)->level == 0) {
		TRACE(" > looping through dependency level %d\n", level);
		/* loop through deptree */
		SLIST_FOREACH(pdp, pdphead, next) {
			if (pdp->level != 0)
				/* 
				 * dependency already processed, no need to
				 * proceed as the next pdp's already have a
				 * level too, remember head has the deps to
				 * process.
				 */
				break;
			/* set all this range to the current level */
			pdp->level = level;
			snprintf(query, BUFSIZ, depquery, pdp->name);
			/*
			 * record pdp->name's direct dependencies in head
			 * with level = 0
			 * */
			if (pkgindb_doquery(query, pdb_rec_depends, pdphead) == PDB_OK)
				TRACE(" > recording %s dependencies (will be level %d)\n",
					pdp->name, level + 1);
			TRACE(" |-%s-(deepness %d)\n", pdp->depend, pdp->level);
		} /* SLIST_FOREACH */
		/* increase level for next loop */
		++level;
	}
	TRACE("[<]-leaving depends\n");
}

int
show_direct_depends(const char *pkgarg)
{
	char		*pkgname, query[BUFSIZ];
	Pkglist		*pdp, *mapplist;
	Plisthead	*deptreehead;

	if (SLIST_EMPTY(&r_plisthead)) {
		printf("%s\n", MSG_EMPTY_AVAIL_PKGLIST);
		return EXIT_FAILURE;
	}

	if ((pkgname = unique_pkg(pkgarg, REMOTE_PKG)) == NULL) {
		fprintf(stderr, MSG_PKG_NOT_AVAIL, pkgarg);
		return EXIT_FAILURE;
	}

	deptreehead = init_head();

	snprintf(query, BUFSIZ, EXACT_DIRECT_DEPS, pkgname);

	if (pkgindb_doquery(query, pdb_rec_depends, deptreehead) == PDB_OK) {
		printf(MSG_DIRECT_DEPS_FOR, pkgname);
		SLIST_FOREACH(pdp, deptreehead, next) {
			if (package_version && 
				(mapplist = map_pkg_to_dep(&r_plisthead,
							   pdp->depend))
				!= NULL)
				printf("\t%s\n", mapplist->full);
			else
				printf("\t%s\n", pdp->depend);
		}
		free_pkglist(&deptreehead, DEPTREE);
	}
	XFREE(pkgname);

	return EXIT_SUCCESS;
}

int
show_full_dep_tree(const char *pkgarg, const char *depquery, const char *msg)
{
	Pkglist		*pdp, *mapplist;
	Plisthead	*deptreehead, *plisthead;
	char		*pkgname = NULL;

	if (depquery == LOCAL_REVERSE_DEPS) {
		plisthead = &l_plisthead;
		pkgname = xstrdup(pkgarg);
	} else {
		plisthead = &r_plisthead;
		pkgname = unique_pkg(pkgarg, REMOTE_PKG);
	}

	if (pkgname == NULL) {
		fprintf(stderr, MSG_PKG_NOT_AVAIL, pkgarg);
		return EXIT_FAILURE;
	}

	if (plisthead == NULL)
		errx(EXIT_FAILURE, MSG_EMPTY_AVAIL_PKGLIST);

	deptreehead = init_head();

	printf(msg, pkgname);
	full_dep_tree(pkgname, depquery, deptreehead);

	SLIST_FOREACH(pdp, deptreehead, next) {
		if (package_version && 
			(mapplist = map_pkg_to_dep(plisthead,
						   pdp->depend)) != NULL)
			printf("\t%s\n", mapplist->full);
		else
			printf("\t%s\n", pdp->depend);
	}

	XFREE(pkgname);
	free_pkglist(&deptreehead, DEPTREE);

	return EXIT_SUCCESS;
}
