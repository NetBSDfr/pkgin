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
 * Download order, sorted alphabetically.
 *
 * All we do is skip any packages not marked for download by pkgin_install()
 * and then insert based on sorted PKGNAME.
 */
Plisthead *
order_download(Plisthead *impacthead)
{
	Plisthead	*dlhead;
	Pkglist		*d, *dsave, *p, *pkg;

	dlhead = init_head();

	SLIST_FOREACH(p, impacthead, next) {
		if (!p->download)
			continue;

		pkg = malloc_pkglist();
		pkg->ipkg = p;

		if (SLIST_EMPTY(dlhead)) {
			SLIST_INSERT_HEAD(dlhead, pkg, next);
			continue;
		}

		/*
		 * Find first existing entry that sorts after us.  No doubt
		 * there is a better algorithm that could be used here...
		 */
		dsave = NULL;
		SLIST_FOREACH(d, dlhead, next) {
			if (strcmp(pkg->ipkg->rpkg->full,
			    d->ipkg->rpkg->full) < 0)
				break;
			dsave = d;
		}
		if (dsave)
			SLIST_INSERT_AFTER(dsave, pkg, next);
		else
			SLIST_INSERT_HEAD(dlhead, pkg, next);
	}

	return dlhead;
}

/*
 * Order the list of packages to install based on their dependency level, so
 * that dependencies are installed first.
 */
Plisthead *
order_install(Plisthead *impacthead)
{
	Plisthead	*installhead;
	Pkglist		*p, *pkg, *savepi = NULL;
	int		i, minlevel = 0, maxlevel = 0;

	/* Record highest dependency level on impact list */
	SLIST_FOREACH(p, impacthead, next) {
		if (p->level > maxlevel)
			maxlevel = p->level;
		if (p->level < minlevel)
			minlevel = p->level;
	}

	installhead = init_head();

	/*
	 * Perform the first loop only considering packages that are being
	 * installed.
	 */
	for (i = minlevel; i <= maxlevel; i++) {
		SLIST_FOREACH(p, impacthead, next) {
			if (p->level != i)
				continue;

			if (!action_is_install(p->action))
				continue;

			pkg = malloc_pkglist();
			pkg->ipkg = p;

			/*
			 * Check for pkg_install, and if found, save for later
			 * insertion at the head of this level.
			 */
			if (strcmp(p->rpkg->name, "pkg_install") == 0) {
				savepi = pkg;
				continue;
			}

			SLIST_INSERT_HEAD(installhead, pkg, next);
		}

		/*
		 * Put pkg_install at the head of this level so that the newer
		 * version is used for as many installs as possible.  It isn't
		 * guaranteed that this is the lowest level as there are cases
		 * where pkg_install can depend on other packages.
		 */
		if (savepi != NULL) {
			SLIST_INSERT_HEAD(installhead, savepi, next);
			savepi = NULL;
		}
	}

	/*
	 * Now handle removals.  These must be performed first in case there
	 * are file conflicts.
	 */
	for (i = 0; i <= maxlevel; i++) {
		SLIST_FOREACH(p, impacthead, next) {
			if (p->level != i)
				continue;
			if (!action_is_remove(p->action))
				continue;
			pkg = malloc_pkglist();
			pkg->ipkg = p;
			SLIST_INSERT_HEAD(installhead, pkg, next);
		}
	}

	return installhead;
}
