/* $Id: order.c,v 1.1.1.1.2.2 2011/08/16 21:17:55 imilh Exp $ */

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

/*
 * order.c has only one purpose: arrange lists order in order to satisfy
 * pkg_add / pkg_delete.
 */

/* find dependency deepness for package removal and record it to pdp->level */
static void
remove_dep_deepness(Deptreehead *deptreehead)
{
	char *depname;
	Pkgdeptree *pdp;
	Deptreehead lvldeptree;

	SLIST_INIT(&lvldeptree);

	/* get higher recursion level */
	SLIST_FOREACH(pdp, deptreehead, next) {
		if (pdp->level == -1) { /* unique package, just return */
			pdp->level = 0;
			return;
		}

		pdp->level = 1;
		
		if (pdp->depname == NULL)
			/* there's something wrong with database's record, probably
			 * a mistaken dependency
			 */
			continue;

		/* depname received from deptreehead is in package format */
		XSTRDUP(depname, pdp->depname);

		trunc_str(depname, '-', STR_BACKWARD);

		full_dep_tree(depname, LOCAL_REVERSE_DEPS, &lvldeptree);

		if (!SLIST_EMPTY(&lvldeptree))
		    	pdp->level = SLIST_FIRST(&lvldeptree)->level + 1;

		XFREE(depname);
		free_deptree(&lvldeptree);

#if 0
		printf("%s -> %d\n", pdp->depname, pdp->level);
#endif
	}
}

/* order the remove list according to dependency level */
Deptreehead *
order_remove(Deptreehead *deptreehead)
{
	int i, maxlevel = 0;
	Pkgdeptree *pdp, *next;
	Deptreehead *ordtreehead;

	/* package removal cannot trust recorded dependencies, reorder */
	remove_dep_deepness(deptreehead);

	SLIST_FOREACH(pdp, deptreehead, next)
		if (pdp->level > maxlevel)
			maxlevel = pdp->level;

	XMALLOC(ordtreehead, sizeof(Deptreehead));
	SLIST_INIT(ordtreehead);

	for (i = maxlevel; i >= 0; i--) {
		pdp = SLIST_FIRST(deptreehead);
		while (pdp != NULL) {
			next = SLIST_NEXT(pdp, next);
			if (pdp->level == i) {
				SLIST_REMOVE(deptreehead, pdp, Pkgdeptree, next);
				SLIST_INSERT_HEAD(ordtreehead, pdp, next);
			}
			pdp = next;
		}
	}

	return ordtreehead;
}

/* find dependency deepness for upgrade and record it to pimpact->level */
static void
upgrade_dep_deepness(Impacthead *impacthead)
{
	char		*pkgname, *p;
	Pkgimpact	*pimpact;
	Deptreehead	lvldeptree;
	Plisthead	*plisthead;

	plisthead = rec_pkglist(LOCAL_PKGS_QUERY);

	SLIST_INIT(&lvldeptree);

	/* get higher recursion level */
	SLIST_FOREACH(pimpact, impacthead, next) {
		if (pimpact->level == -1) { /* unique package, just return */
			pimpact->level = 0;
			goto endupdep;
		}

		/* only deal with TOUPGRADE and TOREMOVE */
		if (pimpact->action == TOINSTALL)
			continue;

		pimpact->level = 1;

		/* depname received from impact is in full package format */
		XSTRDUP(pkgname, pimpact->fullpkgname);

		if ((p = strrchr(pkgname, '-')) != NULL)
			*p = '\0';

		full_dep_tree(pkgname, LOCAL_REVERSE_DEPS, &lvldeptree);

		if (!SLIST_EMPTY(&lvldeptree))
		    	pimpact->level = SLIST_FIRST(&lvldeptree)->level + 1;

#if 0
		printf("%s (%s) -> %d\n",
			pimpact->fullpkgname, pkgname, pimpact->level);
#endif

		XFREE(pkgname);
		free_deptree(&lvldeptree);
	}

endupdep:
	free_pkglist(plisthead);
}

/* order the remove-for-upgrade list according to dependency level */
Deptreehead *
order_upgrade_remove(Impacthead *impacthead)
{
	Deptreehead *ordtreehead;
	Pkgimpact *pimpact;
	Pkgdeptree *pdp;
	int i, maxlevel = 0;

	upgrade_dep_deepness(impacthead);

	/* record higher dependency level on impact upgrade list */
	SLIST_FOREACH(pimpact, impacthead, next)
		if ((pimpact->action == TOUPGRADE || pimpact->action == TOREMOVE)
				&& pimpact->level > maxlevel)
			maxlevel = pimpact->level;

	XMALLOC(ordtreehead, sizeof(Deptreehead));
	SLIST_INIT(ordtreehead);

	for (i = maxlevel; i >= 0; i--)
		SLIST_FOREACH(pimpact, impacthead, next) {
			if ((pimpact->action == TOUPGRADE ||  pimpact->action == TOREMOVE)
				&& pimpact->level == i) {

				XMALLOC(pdp, sizeof(Pkgdeptree));
				XSTRDUP(pdp->depname, pimpact->oldpkg);
				pdp->matchname = NULL; /* safety */
				pdp->computed = pimpact->action; /* XXX: ugly */
				SLIST_INSERT_HEAD(ordtreehead, pdp, next);
			}
		}

	return ordtreehead;
}

/*
 * order the install list according to dependency level
 * here we only rely on basic level given by pkg_summary, the only drawback
 * is that pkg_add will install direct dependencies, giving a "failed,
 * package already installed"
 */
Deptreehead *
order_install(Impacthead *impacthead)
{
	Deptreehead *ordtreehead;
	Pkgimpact *pimpact;
	Pkgdeptree *pdp;
	int i, maxlevel = 0;

	/* record higher dependency level on impact list */
	SLIST_FOREACH(pimpact, impacthead, next) {
		if ((pimpact->action == TOUPGRADE || pimpact->action == TOINSTALL) &&
			pimpact->level > maxlevel)
			maxlevel = pimpact->level;
	}

	XMALLOC(ordtreehead, sizeof(Deptreehead));
	SLIST_INIT(ordtreehead);

	for (i = 0; i <= maxlevel; i++) {
		SLIST_FOREACH(pimpact, impacthead, next) {
			if ((pimpact->action == TOUPGRADE ||
					pimpact->action == TOINSTALL) && pimpact->level == i) {
				XMALLOC(pdp, sizeof(Pkgdeptree));
				XSTRDUP(pdp->depname, pimpact->fullpkgname);
				pdp->matchname = NULL; /* safety */
				pdp->level = pimpact->level;
				/* record package size for download check */
				pdp->file_size = pimpact->file_size;

				SLIST_INSERT_HEAD(ordtreehead, pdp, next);
			}
		}
	}

	return ordtreehead;
}
