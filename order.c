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
		free_pkglist(&lvldeptree);

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

/*
 * Simple download order.  In the future it would be nice to sort this
 * alphabetically for prettier output.
 *
 * All we do is skip any packages not marked for download by pkgin_install().
 */
Plisthead *
order_download(Plisthead *impacthead)
{
	Plisthead	*ordtreehead;
	Pkglist		*pimpact, *pdp;

	ordtreehead = init_head();

	SLIST_FOREACH(pimpact, impacthead, next) {
		if (!pimpact->download)
			continue;

		pdp = malloc_pkglist();
		pdp->action = pimpact->action;
		pdp->depend = xstrdup(pimpact->full);
		pdp->download = pimpact->download;
		pdp->pkgurl = xstrdup(pimpact->pkgurl);
		pdp->file_size = pimpact->file_size;
		SLIST_INSERT_HEAD(ordtreehead, pdp, next);
	}

	return ordtreehead;
}

/*
 * Order the list of packages to install based on their dependency level, so
 * that dependencies are installed first.
 */
Plisthead *
order_install(Plisthead *impacthead)
{
	Plisthead	*ordtreehead;
	Pkglist		*pimpact, *pdp, *pi_dp = NULL;
	int		i, maxlevel = 0;
	char		tmpcheck[BUFSIZ];

	/* Record highest dependency level on impact list */
	SLIST_FOREACH(pimpact, impacthead, next) {
		if (pimpact->level > maxlevel)
			maxlevel = pimpact->level;
	}

	ordtreehead = init_head();

	/*
	 * Start at the highest level (leaf packages), inserting each entry at
	 * the head of the list, before moving down a level, resulting in core
	 * dependencies at the head of the list and leaf packages at the end.
	 *
	 * pkg_install is special, and if there is an upgrade available then we
	 * want it to be installed first so that it is used for all subsequent
	 * package upgrades.
	 */
	for (i = maxlevel; i >= 0; i--) {
		SLIST_FOREACH(pimpact, impacthead, next) {
			if (pimpact->level != i)
				continue;

			pdp = malloc_pkglist();

			pdp->action = pimpact->action;
			pdp->depend = xstrdup(pimpact->full);
			pdp->level = pimpact->level;
			pdp->download = pimpact->download;
			pdp->pkgurl = xstrdup(pimpact->pkgurl);
			pdp->file_size = pimpact->file_size;

			if (pimpact->build_date)
				pdp->build_date = xstrdup(pimpact->build_date);

			if (pimpact->old)
				pdp->old = xstrdup(pimpact->old);

			/*
			 * Check for pkg_install, and if found, save for later
			 * insertion at the head of this level.
			 */
			strcpy(tmpcheck, pimpact->full);
			trunc_str(tmpcheck, '-', STR_BACKWARD);
			if (!pi_dp && strcmp(tmpcheck, "pkg_install") == 0) {
				pi_dp = pdp;
			} else {
				SLIST_INSERT_HEAD(ordtreehead, pdp, next);
			}
		}

		/*
		 * Put pkg_install at the head of this level.  It isn't
		 * guaranteed that this is the lowest level, there are cases
		 * where pkg_install can depend on other packages, in which
		 * case they will be installed using the currently-installed
		 * version first.
		 */
		if (pi_dp != NULL)
			SLIST_INSERT_HEAD(ordtreehead, pi_dp, next);
	}

	return ordtreehead;
}
