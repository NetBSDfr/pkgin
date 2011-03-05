/* $Id: autoremove.c,v 1.2 2011/03/05 22:33:16 imilh Exp $ */

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

/* autoremove.c: cleanup orphan dependencies */

#include "pkgin.h"

static int 			removenb = 0;

void
pkgin_autoremove()
{
	Plisthead	*plisthead;
	Pkglist		*pkglist;
	Deptreehead	keephead, removehead, *orderedhead;
	Pkgdeptree	*premove, *pdp;
	char		*pkgname, *toremove = NULL;
	int			exists;

	/* test if there's any keep package and record them */
	if ((plisthead = rec_pkglist(KEEP_LOCAL_PKGS)) == NULL)
		errx(EXIT_FAILURE, MSG_NO_PKGIN_PKGS, getprogname());

	SLIST_INIT(&keephead);

	/* record keep packages deps  */
	SLIST_FOREACH(pkglist, plisthead, next) {
		XSTRDUP(pkgname, pkglist->pkgname);
		trunc_str(pkgname, '-', STR_BACKWARD);

		full_dep_tree(pkgname, LOCAL_DIRECT_DEPS, &keephead);

		XFREE(pkgname);
	}
	free_pkglist(plisthead);

	/* record unkeep packages */
	if ((plisthead = rec_pkglist(NOKEEP_LOCAL_PKGS)) == NULL) {
		free_deptree(&keephead);

		printf(MSG_ALL_KEEP_PKGS);
		return;
	}

	SLIST_INIT(&removehead);

	/* parse non-keepables packages */
	SLIST_FOREACH(pkglist, plisthead, next) {
		XSTRDUP(pkgname, pkglist->pkgname);
		trunc_str(pkgname, '-', STR_BACKWARD);

		exists = 0;
		/* is it a dependence for keepable packages ? */
		SLIST_FOREACH(pdp, &keephead, next) {
			if (strncmp(pdp->depname, pkgname, strlen(pkgname)) == 0) {
				exists = 1;
				break;
			}
		}
		XFREE(pkgname);

		if (exists)
			continue;

		/* package was not found, insert it on removelist */
		XMALLOC(premove, sizeof(Pkgdeptree));
		XSTRDUP(premove->depname, pkglist->pkgname);
		premove->matchname = NULL; /* safety */
		premove->level = 0;

		SLIST_INSERT_HEAD(&removehead, premove, next);

		removenb++;
	} /* SLIST_FOREACH plisthead */

	free_deptree(&keephead);
	free_pkglist(plisthead);

#ifdef WITHOUT_ORDER
	orderedhead = &removehead;
#else
	orderedhead = order_remove(&removehead);
#endif
	if (!SLIST_EMPTY(orderedhead)) {
		SLIST_FOREACH(premove, orderedhead, next)
			toremove = action_list(toremove, premove->depname);

		/* we want this action to be confirmed */
		yesflag = 0;

		printf(MSG_AUTOREMOVE_WARNING);
		printf(MSG_AUTOREMOVE_PKGS, removenb, toremove);
		if (check_yesno()) {
			SLIST_FOREACH(premove, orderedhead, next) {
				printf(MSG_REMOVING, premove->depname);
#ifdef DEBUG
				printf("%s -f %s\n", PKG_DELETE, premove->depname);
#else
				fexec(PKG_DELETE, "-f", premove->depname, NULL);
#endif
			}
			update_db(LOCAL_SUMMARY, NULL);
		}
	}

	XFREE(toremove);
	free_deptree(orderedhead);
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
		printf(MSG_MARK_PKG_KEEP, pkglist->pkgname);

	free_pkglist(plisthead);
}

/* flag packages in pkgargs as non or autoremovable */
void
pkg_keep(int type, char **pkgargs)
{
	Plisthead			*plisthead;
	Pkglist				*pkglist;
	char				*p, **pkeep, *pkgname, query[BUFSIZ];

	plisthead = rec_pkglist(LOCAL_PKGS_QUERY);

	if (plisthead == NULL) /* no packages recorded */
		return;

	/* parse packages by their command line names */
	for (pkeep = pkgargs; *pkeep != NULL; pkeep++) {
		pkgname = NULL;
		/* find real package name */
		SLIST_FOREACH(pkglist, plisthead, next) {
			/* PKGNAME match */
			if (exact_pkgfmt(*pkeep)) /* argument was a full package name */
				trunc_str(*pkeep, '-', STR_BACKWARD);

			XSTRDUP(pkgname, pkglist->pkgname);
			if ((p = strrchr(pkgname, '-')) != NULL)
			    	*p = '\0';

			if (strcmp(*pkeep, pkgname) == 0) {
			    	if (p != NULL)
					*p = '-';
				break;
			}
			XFREE(pkgname);
		} /* SLIST pkglist */

		if (pkgname != NULL) {
			switch (type) {
			case KEEP:
				printf(MSG_MARKING_PKG_KEEP, pkgname);
				snprintf(query, BUFSIZ, KEEP_PKG, pkgname);
				/* mark as non-automatic in pkgdb */
				if (mark_as_automatic_installed(pkgname, 0) < 0)
					exit(EXIT_FAILURE);
				break;
			case UNKEEP:
				printf(MSG_UNMARKING_PKG_KEEP, pkgname);
				snprintf(query, BUFSIZ, UNKEEP_PKG, pkgname);
				/* mark as automatic in pkgdb */
				if (mark_as_automatic_installed(pkgname, 1) < 0)
					exit(EXIT_FAILURE);
				break;
			}

			pkgindb_doquery(query, NULL, NULL);
			XFREE(pkgname);
		} else
			printf(MSG_PKG_NOT_INSTALLED, *pkeep);
	} /* for (pkeep) */

	free_pkglist(plisthead);
}
