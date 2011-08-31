/* $Id: autoremove.c,v 1.8 2011/08/31 16:58:26 imilh Exp $ */

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

void
pkgin_autoremove()
{
	int			removenb = 0;
	Plisthead	*plisthead;
	Pkglist		*premove;
	char		*toremove = NULL;

	if ((plisthead = rec_pkglist(GET_ORPHAN_PACKAGES)) == NULL)	
		return;

	SLIST_FOREACH(premove, plisthead, next) {
			toremove = action_list(toremove, premove->full);
			removenb++;
	}

	/* we want this action to be confirmed */
	yesflag = 0;

	printf(MSG_AUTOREMOVE_WARNING);
	printf(MSG_AUTOREMOVE_PKGS, removenb, toremove);
	if (check_yesno(DEFAULT_YES)) {
		SLIST_FOREACH(premove, plisthead, next) {
			printf(MSG_REMOVING, premove->full);
#ifdef DEBUG
			printf("%s -f %s\n", PKG_DELETE, premove->full);
#else
			fexec(PKG_DELETE, "-f", premove->full, NULL);
#endif
		}
		update_db(LOCAL_SUMMARY, NULL);
	}

	XFREE(toremove);
	free_pkglist(&plisthead, DEPTREE);
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

	free_pkglist(&plisthead, LIST);
}

/**
 * \brief flag packages in pkgargs as non or autoremovable
 */
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
				printf(MSG_MARKING_PKG_KEEP, pkglist->full);
				/* KEEP_PKG query needs full pkgname */
				snprintf(query, BUFSIZ, KEEP_PKG, pkglist->name);
				/* mark as non-automatic in pkgdb */
				if (mark_as_automatic_installed(pkglist->full, 0) < 0)
					exit(EXIT_FAILURE);
				break;
			case UNKEEP:
				printf(MSG_UNMARKING_PKG_KEEP, pkglist->full);
				/* UNKEEP_PKG query needs full pkgname */
				snprintf(query, BUFSIZ, UNKEEP_PKG, pkglist->name);
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
