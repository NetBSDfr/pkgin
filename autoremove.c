/* $Id: autoremove.c,v 1.2.2.11 2011/08/23 11:46:47 imilh Exp $ */

/*
 * Copyright (c) 2009, 2010, 2011 The NetBSD Foundation, Inc.
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

/**
 * \file autoremove.c
 *
 * Cleanup orphan dependencies, keep and unkeep packages
 */

#include "pkgin.h"

static int removenb = 0;

void
pkgin_autoremove()
{
	Plisthead	*plisthead, *keephead, *removehead, *orderedhead;
	Pkglist		*pkglist, *premove, *pdp;
	char		*toremove = NULL;
	int			exists;

	/*
	 * test if there's any keep package and record them
	 * KEEP_LOCAL_PKGS returns full packages names
	 */
	if ((plisthead = rec_pkglist(KEEP_LOCAL_PKGS)) == NULL)
		errx(EXIT_FAILURE, MSG_NO_PKGIN_PKGS, getprogname());

	keephead = init_head();

	/* record keep packages deps  */
	SLIST_FOREACH(pkglist, plisthead, next)
		full_dep_tree(pkglist->name, LOCAL_DIRECT_DEPS, keephead);

	free_pkglist(plisthead, LIST);

	/* record unkeep packages */
	if ((plisthead = rec_pkglist(NOKEEP_LOCAL_PKGS)) == NULL) {
		free_pkglist(keephead, DEPTREE);

		printf(MSG_ALL_KEEP_PKGS);
		return;
	}

	removehead = init_head();

	/* parse non-keepables packages */
	SLIST_FOREACH(pkglist, plisthead, next) {
		exists = 0;
		/* is it a dependence for keepable packages ? */
		SLIST_FOREACH(pdp, keephead, next) {
			if (strcmp(pdp->name, pkglist->name) == 0) {
				exists = 1;
				break;
			}
		}

		if (exists)
			continue;

		/* package was not found, insert it on removelist */
		premove = malloc_pkglist(DEPTREE);

		XSTRDUP(premove->depend, pkglist->name);

		SLIST_INSERT_HEAD(removehead, premove, next);

		removenb++;
	} /* SLIST_FOREACH plisthead */

	free_pkglist(keephead, DEPTREE);
	free_pkglist(plisthead, LIST);

#ifdef WITHOUT_ORDER
	orderedhead = removehead;
#else
	orderedhead = order_remove(removehead);
#endif
	if (!SLIST_EMPTY(orderedhead)) {
		SLIST_FOREACH(premove, orderedhead, next)
			toremove = action_list(toremove, premove->depend);

		/* we want this action to be confirmed */
		yesflag = 0;

		printf(MSG_AUTOREMOVE_WARNING);
		printf(MSG_AUTOREMOVE_PKGS, removenb, toremove);
		if (check_yesno()) {
			SLIST_FOREACH(premove, orderedhead, next) {
				printf(MSG_REMOVING, premove->depend);
#ifdef DEBUG
				printf("%s -f %s\n", PKG_DELETE, premove->depend);
#else
				fexec(PKG_DELETE, "-f", premove->depend, NULL);
#endif
			}
			update_db(LOCAL_SUMMARY, NULL);
		}
	}

	XFREE(toremove);
	free_pkglist(orderedhead, DEPTREE);
#ifndef WITHOUT_ORDER
	XFREE(orderedhead);
#endif
}

void
show_pkg_keep(void)
{
	Plisthead	*plisthead;
	Pkglist		*pkglist;

	plisthead = rec_pkglist(KEEP_LOCAL_PKGS);

	if (plisthead == NULL) {
		printf("%s\n", MSG_EMPTY_KEEP_LIST);
		return;
	}

	SLIST_FOREACH(pkglist, plisthead, next)
		printf(MSG_MARK_PKG_KEEP, pkglist->full);

	free_pkglist(plisthead, LIST);
}

/* flag packages in pkgargs as non or autoremovable */
void
pkg_keep(int type, char **pkgargs)
{
	Pkglist	*pkglist = NULL;
	char   	**pkeep, *pkgname, query[BUFSIZ];

	if (SLIST_EMPTY(&l_plisthead)) /* no packages recorded */
		return;

	/* parse packages by their command line names */
	for (pkeep = pkgargs; *pkeep != NULL; pkeep++) {
		/* find real package name */
		if ((pkgname = unique_pkg(*pkeep)) != NULL) {
			SLIST_FOREACH(pkglist, &l_plisthead, next)
				/* PKGNAME match */
				if (strcmp(pkgname, pkglist->full) == 0)
					break;

			XFREE(pkgname);
		} /* pkgname != NULL */

		if (pkglist != NULL && pkglist->full != NULL) {
			switch (type) {
			case KEEP:
				printf(MSG_MARKING_PKG_KEEP, pkglist->full);
				/* KEEP_PKG query needs full pkgname */
				snprintf(query, BUFSIZ, KEEP_PKG, pkglist->full);
				/* mark as non-automatic in pkgdb */
				if (mark_as_automatic_installed(pkglist->full, 0) < 0)
					exit(EXIT_FAILURE);
				break;
			case UNKEEP:
				printf(MSG_UNMARKING_PKG_KEEP, pkglist->full);
				/* UNKEEP_PKG query needs full pkgname */
				snprintf(query, BUFSIZ, UNKEEP_PKG, pkglist->full);
				/* mark as automatic in pkgdb */
				if (mark_as_automatic_installed(pkglist->full, 1) < 0)
					exit(EXIT_FAILURE);
				break;
			}

			pkgindb_doquery(query, NULL, NULL);
		} else
			printf(MSG_PKG_NOT_INSTALLED, *pkeep);
	} /* for (pkeep) */
}
