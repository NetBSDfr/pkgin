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
 * /file order.c
 *
 * order.c has only one purpose: arrange lists order in order to satisfy
 * pkg_add / pkg_delete.
 */

/**
 * find dependency deepness for package removal and record it to pdp->level
 */
static void
remove_dep_deepness(Plisthead *deptreehead)
{
	char		*depname;
	Pkglist		*pdp;
	Plisthead	*lvldeptree;

	/* get higher recursion level */
	SLIST_FOREACH(pdp, deptreehead, next) {
		if (pdp->level == -1) { /* unique package, just return */
			pdp->level = 0;
			return;
		}

		pdp->level = 1;
		
		if (pdp->depend == NULL)
			/* there's something wrong with database's record,
			 * probably a mistaken dependency
			 */
			continue;

		/* depname received from deptreehead is in package format */
		depname = xstrdup(pdp->depend);

		trunc_str(depname, '-', STR_BACKWARD);

		lvldeptree = init_head();
		full_dep_tree(depname, LOCAL_REVERSE_DEPS, lvldeptree);

		if (!SLIST_EMPTY(lvldeptree))
		    	pdp->level = SLIST_FIRST(lvldeptree)->level + 1;

		XFREE(depname);
		free_pkglist(&lvldeptree, DEPTREE);

#if 0
		printf("%s -> %d\n", pdp->depend, pdp->level);
#endif
	}
}

/**
 * \fn order_remove
 *
 * \brief order the remove list according to dependency level
 */
Plisthead *
order_remove(Plisthead *deptreehead)
{
	int		i, maxlevel = 0;
	Pkglist		*pdp, *next;
	Plisthead	*ordtreehead;

	/* package removal cannot trust recorded dependencies, reorder */
	remove_dep_deepness(deptreehead);

	SLIST_FOREACH(pdp, deptreehead, next)
		if (pdp->level > maxlevel)
			maxlevel = pdp->level;

	ordtreehead = init_head();

	for (i = maxlevel; i >= 0; i--) {
		pdp = SLIST_FIRST(deptreehead);
		while (pdp != NULL) {
			next = SLIST_NEXT(pdp, next);
			if (pdp->level == i) {
				SLIST_REMOVE(deptreehead, pdp, Pkglist, next);
				SLIST_INSERT_HEAD(ordtreehead, pdp, next);
			}
			pdp = next;
		}
	}

	return ordtreehead;
}

/* find dependency deepness for upgrade and record it to pimpact->level */
static void
upgrade_dep_deepness(Plisthead *impacthead)
{
	char		*pkgname;
	Pkglist		*pimpact;
	Plisthead	*lvldeptree;

	/* get higher recursion level */
	SLIST_FOREACH(pimpact, impacthead, next) {
		if (pimpact->level == -1) { /* unique package, just return */
			pimpact->level = 0;
			return;
		}

		/* only deal with TOUPGRADE and TOREMOVE */
		if (pimpact->action == TOINSTALL)
			continue;

		pimpact->level = 1;

		/* depname received from impact is in full package format */
		pkgname = xstrdup(pimpact->full);

		trunc_str(pkgname, '-', STR_BACKWARD);

		lvldeptree = init_head();
		full_dep_tree(pkgname, LOCAL_REVERSE_DEPS, lvldeptree);

		if (!SLIST_EMPTY(lvldeptree))
		    	pimpact->level = SLIST_FIRST(lvldeptree)->level + 1;

#if 0
		printf("%s (%s) -> %d\n",
			pimpact->full, pkgname, pimpact->level);
#endif

		XFREE(pkgname);
		free_pkglist(&lvldeptree, DEPTREE);
	}
}

uint8_t pi_upgrade = 0;
/**
 * \fn order_upgrade_remove
 *
 * \brief order the remove-for-upgrade list according to dependency level
 */
Plisthead *
order_upgrade_remove(Plisthead *impacthead)
{
	Plisthead	*ordtreehead;
	Pkglist		*pimpact, *pdp;
	int		i, maxlevel = 0;

	upgrade_dep_deepness(impacthead);

	/* record higher dependency level on impact upgrade list */
	SLIST_FOREACH(pimpact, impacthead, next)
		if ((pimpact->action == TOUPGRADE ||
			pimpact->action == TOREMOVE) &&
			pimpact->level > maxlevel)

			maxlevel = pimpact->level;

	ordtreehead = init_head();

	for (i = maxlevel; i >= 0; i--)
		SLIST_FOREACH(pimpact, impacthead, next) {
			if ((pimpact->action == TOUPGRADE ||
				pimpact->action == TOREMOVE) &&
				pimpact->level == i) {

				if (pkg_in_impact(ordtreehead, pimpact->old))
					continue;

				pdp = malloc_pkglist(DEPTREE);

				pdp->depend = xstrdup(pimpact->old);
				pdp->name = NULL; /* safety */
				/* XXX: use the "computed" value to record
				 * action type. Ugly.
				 */
				pdp->computed = pimpact->action;
				/* informative only */
				pdp->level = pimpact->level;
				SLIST_INSERT_HEAD(ordtreehead, pdp, next);

			} /* action == TOUPGRADE || TOREMOVE */
		} /* for maxlevel */

	return ordtreehead;
}

/**
 * \fn order_install
 *
 * order the install list according to dependency level
 * here we only rely on basic level given by pkg_summary, the only drawback
 * is that pkg_add will install direct dependencies, giving a "failed,
 * package already installed"
 */
Plisthead *
order_install(Plisthead *impacthead)
{
	Plisthead	*ordtreehead;
	Pkglist		*pimpact, *pdp, *pi_dp = NULL;
	int		i, maxlevel = 0;
	char		tmpcheck[BUFSIZ];

	/* record higher dependency level on impact list */
	SLIST_FOREACH(pimpact, impacthead, next) {
		if ((pimpact->action == TOUPGRADE ||
			pimpact->action == TOINSTALL) &&
			pimpact->level > maxlevel)
			maxlevel = pimpact->level;
	}

	ordtreehead = init_head();

	for (i = 0; i <= maxlevel; i++) {
		SLIST_FOREACH(pimpact, impacthead, next) {
			if ((pimpact->action == TOUPGRADE ||
				pimpact->action == TOINSTALL) &&
				pimpact->level == i) {

				if (pkg_in_impact(ordtreehead, pimpact->full))
					continue;

				pdp = malloc_pkglist(DEPTREE);

				pdp->computed = pimpact->action; /* XXX: ugly*/
				pdp->depend = xstrdup(pimpact->full);
				pdp->name = NULL;
				pdp->level = pimpact->level;
				/* record package size for download check */
				pdp->file_size = pimpact->file_size;
				pdp->old = pimpact->old;
				/* check for pkg_install upgrade */
				strcpy(tmpcheck, pimpact->full);
				trunc_str(tmpcheck, '-', STR_BACKWARD);
				/* match on pkg_install */
				if (pi_dp == NULL &&
					strcmp(tmpcheck, PKG_INSTALL) == 0) {
					pi_upgrade = 1;
					/* backup pdp for future insertion */
					pi_dp = pdp;
				} else					
					SLIST_INSERT_HEAD(ordtreehead,
						pdp, next);
			} /* action == TOINSTALL */
		} /* SLIST_FOREACH pimpact */
	} /* for i < maxlevel */

	/* pkg_install is to be upgraded, make it first */
	if (pi_upgrade && pi_dp != NULL)
		SLIST_INSERT_HEAD(ordtreehead, pi_dp, next);

	return ordtreehead;
}
