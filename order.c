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

/*
 * Order package lists to ensure they are correctly ordered when passed to
 * pkg_add or pkg_delete.
 *
 * Incorrect ordering of pkg_add arguments results in packages being installed
 * twice, as pkg_add will pull in dependencies automatically.
 *
 * Incorrect ordering of pkg_delete arguments will work as we explicitly use
 * "pkg_delete -f" but we don't want to rely on that.
 */

/*
 * Order removals for pkg_delete.
 *
 * Note that this function removes entries from the supplied impact list as an
 * optimisation, as currently all callers of it do not re-use it.
 */
Plisthead *
order_remove(Plisthead *impacthead)
{
	int		i, maxlevel = 0;
	Pkglist		*p, *tmpp;
	Plisthead	*removehead;

	SLIST_FOREACH(p, impacthead, next)
		if (p->level > maxlevel)
			maxlevel = p->level;

	removehead = init_head();

	/*
	 * Move entries from impacthead to removehead according to dependency
	 * level.
	 */
	for (i = 0; i <= maxlevel; i++) {
		SLIST_FOREACH_SAFE(p, impacthead, next, tmpp) {
			if (p->level != i)
				continue;

			/*
			 * We do not support shooting yourself in the foot.
			 */
			if (strcmp(p->lpkg->name, "pkg_install") == 0) {
				fprintf(stderr, MSG_NOT_REMOVING_PKG_INSTALL);
				continue;
			}

			SLIST_REMOVE(impacthead, p, Pkglist, next);
			SLIST_INSERT_HEAD(removehead, p, next);
		}
	}

	return removehead;
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
		pdp->ipkg = pimpact;
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
		pi_dp = NULL;
		SLIST_FOREACH(pimpact, impacthead, next) {
			if (pimpact->level != i)
				continue;

			/*
			 * XXX: This is incorrect, removals need to be handled
			 * properly during upgrades, but is necessary for now
			 * to avoid issues with pkgurl being set to NULL.
			 */
			if (pimpact->action == TOREMOVE)
				continue;

			pdp = malloc_pkglist();
			pdp->ipkg = pimpact;

			/*
			 * Check for pkg_install, and if found, save for later
			 * insertion at the head of this level.
			 */
			if (!pi_dp && strcmp(pimpact->rpkg->name, "pkg_install") == 0) {
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
