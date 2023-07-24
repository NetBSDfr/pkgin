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
 * \file autoremove.c
 *
 * Cleanup orphan dependencies, keep and unkeep packages
 */

#include <sqlite3.h>
#include "pkgin.h"

void
pkgin_autoremove(void)
{
	Plistnumbered	*nokeephead, *keephead;
	Plisthead	*depshead, *removehead, *orderedhead;
	Pkglist		*pkglist, *premove, *pdp, *p;
	char		*toremove = NULL, preserve[BUFSIZ];
	int		is_keep_dep, removenb = 0;

	/*
	 * Record all keep and no-keep packages.  If either are empty then
	 * we're done.
	 */
	if ((keephead = rec_pkglist(KEEP_LOCAL_PKGS)) == NULL)
		errx(EXIT_FAILURE, "no packages have been marked as keepable");

	if ((nokeephead = rec_pkglist(NOKEEP_LOCAL_PKGS)) == NULL) {
		free_pkglist(&keephead->P_Plisthead);
		printf(MSG_ALL_KEEP_PKGS);
		return;
	}


	/*
	 * Record all recursive dependencies for each keep package.  This then
	 * contains a list of all packages that are required.
	 */
	depshead = init_head();
	SLIST_FOREACH(p, keephead->P_Plisthead, next) {
		get_depends_recursive(p->full, depshead, DEPENDS_LOCAL);
	}

	removehead = init_head();

	/*
	 * For each non-keep package, get all of its reverse dependencies, and
	 * if any of them are in keeplist then this package is still required.
	 */
	SLIST_FOREACH(pkglist, nokeephead->P_Plisthead, next) {
		is_keep_dep = 0;
		SLIST_FOREACH(pdp, depshead, next) {
			if (strcmp(pdp->lpkg->full, pkglist->full) == 0) {
				is_keep_dep = 1;
				break;
			}
		}
		if (is_keep_dep)
			continue;

		/*
		 * Also keep the package if it is a "preserve" package (one
		 * that is specifically built to not be uninstalled, for
		 * example important bootstrap packages.
		 */
		snprintf(preserve, BUFSIZ, "%s/%s/%s", pkgdb_get_dir(),
		    pkglist->full, PRESERVE_FNAME);
		if (access(preserve, F_OK) != -1)
			continue;

		/*
		 * Package can be auto removed, add to the list.
		 */
		premove = malloc_pkglist();
		premove->full = xstrdup(pkglist->full);
		SLIST_INSERT_HEAD(removehead, premove, next);
		removenb++;
	}

	free_pkglist(&keephead->P_Plisthead);
	free(keephead);
	free_pkglist(&nokeephead->P_Plisthead);
	free(nokeephead);

	if (!removenb) {
		printf(MSG_NO_ORPHAN_DEPS);
		exit(EXIT_SUCCESS);
	}

	orderedhead = order_remove(removehead);

	free_pkglist(&removehead);

	if (!SLIST_EMPTY(orderedhead)) {
		SLIST_FOREACH(premove, orderedhead, next)
			toremove = action_list(toremove, premove->full);

		printf(MSG_AUTOREMOVE_PKGS, removenb, toremove);
		if (!noflag)
			printf("\n");
		if (check_yesno(DEFAULT_YES)) {
			do_pkg_remove(orderedhead);

			(void)update_db(LOCAL_SUMMARY, NULL, 1);
		}
	}

	XFREE(toremove);
	free_pkglist(&orderedhead);
}

void
show_pkg_keep(void)
{
	Plistnumbered	*plisthead;
	Pkglist		*pkglist;

	plisthead = rec_pkglist(KEEP_LOCAL_PKGS);

	if (plisthead == NULL) {
		printf("%s\n", MSG_EMPTY_KEEP_LIST);
		return;
	}

	SLIST_FOREACH(pkglist, plisthead->P_Plisthead, next)
		printf("%-20s %s\n", pkglist->full, pkglist->comment);

	free_pkglist(&plisthead->P_Plisthead);
	free(plisthead);
}

void
show_pkg_nokeep(void)
{
	Plistnumbered	*plisthead;
	Pkglist		*pkglist;

	plisthead = rec_pkglist(NOKEEP_LOCAL_PKGS);

	if (plisthead == NULL) {
		printf("%s\n", MSG_EMPTY_NOKEEP_LIST);
		return;
	}

	SLIST_FOREACH(pkglist, plisthead->P_Plisthead, next)
		printf("%-20s %s\n", pkglist->full, pkglist->comment);

	free_pkglist(&plisthead->P_Plisthead);
	free(plisthead);
}

/**
 * \brief flag packages in pkgargs as non or autoremovable
 */
void
pkg_keep(int type, char **pkgargs)
{
	Pkglist	*pkglist = NULL;
	char	**pkeep, query[BUFSIZ];

	if (!have_privs(PRIVS_PKGDB|PRIVS_PKGINDB))
		errx(EXIT_FAILURE, MSG_DONT_HAVE_RIGHTS);

	if (SLIST_EMPTY(&l_plisthead)) /* no packages recorded */
		return;

	/* parse packages by their command line names */
	for (pkeep = pkgargs; *pkeep != NULL; pkeep++) {
		/* find real package name */
		if ((pkglist = find_local_pkg_match(*pkeep)) == NULL) {
			printf(MSG_PKG_NOT_INSTALLED, *pkeep);
			continue;
		}

		if (pkglist != NULL && pkglist->full != NULL) {
			switch (type) {
			case KEEP:
				/*
				 * pkglist is a keep-package but marked as
				 * automatic, tag it
				 */
				if (is_automatic_installed(pkglist->full)) {
					printf(	MSG_MARKING_PKG_KEEP,
						pkglist->full);
					sqlite3_snprintf(BUFSIZ, query,
					    KEEP_PKG, pkglist->name);
					pkgindb_doquery(query, NULL, NULL);
					/* mark as non-automatic in pkgdb */
					if (mark_as_automatic_installed(
							pkglist->full, 0) < 0)
						exit(EXIT_FAILURE);
				}
				break;
			case UNKEEP:
				printf(MSG_UNMARKING_PKG_KEEP, pkglist->full);
				/* UNKEEP_PKG query needs full pkgname */
				sqlite3_snprintf(BUFSIZ, query,
				    UNKEEP_PKG, pkglist->name);
				pkgindb_doquery(query, NULL, NULL);
				/* mark as automatic in pkgdb */
				if (mark_as_automatic_installed(pkglist->full,
							1) < 0)
					exit(EXIT_FAILURE);
				break;
			}
		}
	} /* for (pkeep) */
}
