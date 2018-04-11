/* $Id: autoremove.c,v 1.21 2012/10/02 10:20:24 imilh Exp $ */

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
 *
 */

/**
 * \file autoremove.c
 *
 * Cleanup orphan dependencies, keep and unkeep packages
 */

#include "pkgin.h"

void
pkgin_autoremove()
{
	Plistnumbered	*plisthead;
	Plisthead	*keephead, *removehead, *orderedhead;
	Pkglist		*pkglist, *premove, *pdp;
	char		*toremove = NULL, preserve[BUFSIZ];
	int		is_keep_dep, removenb = 0;

	/*
	 * test if there's any keep package and record them
	 * KEEP_LOCAL_PKGS returns full packages names
	 */
	if ((plisthead = rec_pkglist(KEEP_LOCAL_PKGS)) == NULL)
		errx(EXIT_FAILURE, MSG_NO_PKGIN_PKGS, getprogname());

	keephead = init_head();

	/* record keep packages deps  */
	SLIST_FOREACH(pkglist, plisthead->P_Plisthead, next)
		full_dep_tree(pkglist->name, LOCAL_DIRECT_DEPS, keephead);

	free_pkglist(&plisthead->P_Plisthead, LIST);
	free(plisthead);

	/* record all unkeep / automatic packages */
	if ((plisthead = rec_pkglist(NOKEEP_LOCAL_PKGS)) == NULL) {
		free_pkglist(&keephead, DEPTREE);

		printf(MSG_ALL_KEEP_PKGS);
		return;
	}

	removehead = init_head();

	/* parse non-keepables packages */
	SLIST_FOREACH(pkglist, plisthead->P_Plisthead, next) {
		is_keep_dep = 0;
		/* is it a dependence for keepable packages ? */
		SLIST_FOREACH(pdp, keephead, next) {
			if (pkg_match(pdp->depend, pkglist->full)) {
				is_keep_dep = 1;
				break;
			}
		}
		snprintf(preserve, BUFSIZ, "%s/%s/%s",
		    pkgdb_get_dir(), pkglist->full, PRESERVE_FNAME);
		/* is or a dependency or a preserved package */
		if (is_keep_dep || access(preserve, F_OK) != -1)
			continue;

		/* package was not found, insert it on removelist */
		premove = malloc_pkglist(DEPTREE);

		premove->depend = xstrdup(pkglist->full);

		SLIST_INSERT_HEAD(removehead, premove, next);

		removenb++;
	} /* SLIST_FOREACH plisthead */

	free_pkglist(&keephead, DEPTREE);
	free_pkglist(&plisthead->P_Plisthead, LIST);
	free(plisthead);

	if (!removenb) {
		printf(MSG_NO_ORPHAN_DEPS);
		exit(EXIT_SUCCESS);
	}

	orderedhead = order_remove(removehead);

	free_pkglist(&removehead, DEPTREE);

	if (!SLIST_EMPTY(orderedhead)) {
		SLIST_FOREACH(premove, orderedhead, next)
			toremove = action_list(toremove, premove->depend);

		/* we want this action to be confirmed */
		yesflag = 0;

		if (noflag == 0)
			printf(MSG_AUTOREMOVE_WARNING);
		printf(MSG_AUTOREMOVE_PKGS, removenb, toremove);

		if (check_yesno(DEFAULT_YES)) {
			do_pkg_remove(orderedhead);

			(void)update_db(LOCAL_SUMMARY, NULL, 1);
		}
	}

	XFREE(toremove);
	free_pkglist(&orderedhead, DEPTREE);
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
		printf(MSG_MARK_PKG_KEEP, pkglist->full);

	free_pkglist(&plisthead->P_Plisthead, LIST);
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
		printf(MSG_MARK_PKG_NOKEEP, pkglist->full);

	free_pkglist(&plisthead->P_Plisthead, LIST);
	free(plisthead);
}

/**
 * \brief flag packages in pkgargs as non or autoremovable
 */
void
pkg_keep(int type, char **pkgargs)
{
	Pkglist	*pkglist = NULL;
	char   	**pkeep, *pkgname, query[BUFSIZ];

	if (!have_privs(PRIVS_PKGDB|PRIVS_PKGINDB))
		errx(EXIT_FAILURE, MSG_DONT_HAVE_RIGHTS);

	if (SLIST_EMPTY(&l_plisthead)) /* no packages recorded */
		return;

	/* parse packages by their command line names */
	for (pkeep = pkgargs; *pkeep != NULL; pkeep++) {
		/* find real package name */
		if ((pkgname = unique_pkg(*pkeep, LOCAL_PKG)) != NULL) {

			trunc_str(pkgname, '-', STR_BACKWARD);

			SLIST_FOREACH(pkglist, &l_plisthead, next)
				/* PKGNAME match */
				if (strcmp(pkgname, pkglist->name) == 0)
					break;

			XFREE(pkgname);
		} /* pkgname != NULL */

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
					/* KEEP_PKG query needs full pkgname */
					snprintf(query, BUFSIZ,
						KEEP_PKG, pkglist->name);
					/* mark as non-automatic in pkgdb */
					if (mark_as_automatic_installed(
							pkglist->full, 0) < 0)
						exit(EXIT_FAILURE);
				}
				break;
			case UNKEEP:
				printf(MSG_UNMARKING_PKG_KEEP, pkglist->full);
				/* UNKEEP_PKG query needs full pkgname */
				snprintf(query, BUFSIZ,
						UNKEEP_PKG, pkglist->name);
				/* mark as automatic in pkgdb */
				if (mark_as_automatic_installed(pkglist->full,
							1) < 0)
					exit(EXIT_FAILURE);
				break;
			}

			pkgindb_doquery(query, NULL, NULL);
		} else
			printf(MSG_PKG_NOT_INSTALLED, *pkeep);
	} /* for (pkeep) */
}

int
pkg_is_kept(Pkglist *pkgkeep)
{
	Plistnumbered	*plisthead;
	Pkglist		*pkglist;
	int		ret = 0;

	if ((plisthead = rec_pkglist(KEEP_LOCAL_PKGS)) == NULL)
		errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);

	SLIST_FOREACH(pkglist, plisthead->P_Plisthead, next) {
		if (strcmp(pkgkeep->name, pkglist->name) == 0) {
			ret = 1;
			break;
		}
	}

	free_pkglist(&plisthead->P_Plisthead, LIST);
	free(plisthead);
	return ret;
}
