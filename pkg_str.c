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

#define GLOBCHARS "{<>[]?*"

/*
 * Return best candidate for a remote package, taking into consideration any
 * preferred.conf matches.
 */
int
find_preferred_pkg(const char *pkgname, Pkglist **pkg, char **match)
{
	Pkglist *p, *best = NULL;
	char *result = NULL;

	/* Find best match */
	SLIST_FOREACH(p, &r_plisthead, next) {
		if (!pkg_match(pkgname, p->full))
			continue;

		/*
		 * Free any previous results first.  If we made it past the
		 * pkg_match then we should get the same result back.
		 */
		if (result != NULL) {
			free(result);
			result = NULL;
		}

		/*
		 * Check that the candidate matches any potential
		 * preferred.conf restrictions, if not then skip.
		 */
		if (chk_preferred(p->full, &result) != 0)
			continue;

		/* Save best match */
		if (best == NULL || version_check(best->full, p->full) == 2)
			best = p;
	}

	/*
	 * Save match if requested.  If there was a successful match then
	 * return the full package name, otherwise the unsuccessful match
	 */
	if (match != NULL)
		*match = best ? xstrdup(best->full) : result;

	/*
	 * Save pkglist entry if requested.
	 */
	if (pkg != NULL)
		*pkg = best;

	return (best == NULL) ? 1 : 0;
}

/**
 * \fn unique_pkg
 *
 * Returns greatest version package matching in full package name form
 */
char *
unique_pkg(const char *pkgname, const char *dest)
{
	char		*u_pkg = NULL;
	Plistnumbered	*plist;
	Pkglist		*best_match = NULL, *current;

	if (exact_pkgfmt(pkgname))
		plist = rec_pkglist(UNIQUE_EXACT_PKG, dest, pkgname);
	else
		plist = rec_pkglist(UNIQUE_PKG, dest, pkgname);

	if (plist == NULL)
		return NULL;

	SLIST_FOREACH(current, plist->P_Plisthead, next) {
		/* first result */
		if (best_match == NULL)
			best_match = current;
		else
			if (dewey_cmp(current->version, DEWEY_GT,
					best_match->version))
				best_match = current;
	}

	if (best_match != NULL)
		u_pkg = xstrdup(best_match->full);
	free_pkglist(&plist->P_Plisthead);
	free(plist);

	return u_pkg;
}

/*
 * Return best remote package for match and optional pkgpath, or NULL if no
 * valid match.
 */
Pkglist *
find_pkg_match(const char *match, const char *pkgpath)
{
	Pkglist	*rpkg, *pkg = NULL;

	SLIST_FOREACH(rpkg, &r_plisthead, next) {
		if (!pkg_match(match, rpkg->full))
			continue;

		/*
		 * If pkgpath is specified then the remote entry must match, to
		 * avoid newer releases being pulled in when the user may have
		 * specified a particular version in the past.
		 */
		if (pkgpath && pkgstrcmp(pkgpath, rpkg->pkgpath) != 0)
			continue;

		if (chk_preferred(rpkg->full, NULL) != 0)
			continue;

		/* Save best match and continue */
		if (pkg == NULL || version_check(pkg->full, rpkg->full) == 2)
			pkg = rpkg;
	}

	return pkg;
}

/*
 * Find first match of a locally-installed package for a package pattern.
 * Returns a Pkglist entry on success or NULL on failure.
 *
 * Just return the first match.  While technically we could find the "best"
 * result for an alternate match it doesn't make any practical sense.
 */
Pkglist *
find_local_pkg_match(const char *pattern)
{
	Pkglist	*pkg = NULL, *p;

	SLIST_FOREACH(p, &l_plisthead, next) {
		if (pkg_match(pattern, p->full)) {
			pkg = p;
			break;
		}
	}

	return pkg;
}

/* basic full package format detection */
int
exact_pkgfmt(const char *pkgname)
{
	char	*p;

	if ((p = strrchr(pkgname, '-')) == NULL)
		return 0;

	p++;

	/* naive assumption, will fail with foo-100bar, hopefully, there's
	 * only a few packages needing to be fully specified
	 */
	return isdigit((int)*p);
}

/* similar to opattern.c's pkg_order but without pattern */
int
version_check(char *first_pkg, char *second_pkg)
{
	char *first_ver, *second_ver;

	first_ver = strrchr(first_pkg, '-');
	second_ver = strrchr(second_pkg, '-');

	if (first_ver == NULL)
		return 2;
	if (second_ver == NULL)
		return 1;

	if (dewey_cmp(first_ver + 1, DEWEY_GT, second_ver + 1))
		return 1;
	else
		return 2;
}

/*
 * Compare string values between two packages.  Often in pkgin the comparison
 * in question allows both to be NULL, for example with pkg_summary fields
 * that are optional, so this function returns true if both are NULL.
 *
 * This also acts as a safe wrapper for strcmp().
 */
int
pkgstrcmp(const char *p1, const char *p2)
{
	if (p1 == NULL && p2 == NULL)
		return 0;

	if (p1 == NULL)
		return -1;
	else if (p2 == NULL)
		return 1;

	return (strcmp(p1, p2));
}

/*
 * qsort callback to sort alphabetically.
 */
int
sort_pkg_alpha(const void *a, const void *b)
{
	return strcmp(*(const char * const *)a, *(const char * const *)b);
}
