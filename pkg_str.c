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
 * Return list of potential candidates from a package match, or NULL if no
 * valid matches are found.
 */
static Plistnumbered *
find_remote_pkgs(const char *pkgname)
{
	const char *query;

	query = exact_pkgfmt(pkgname) ? UNIQUE_EXACT_PKG : UNIQUE_PKG;

	return rec_pkglist(query, REMOTE_PKG, pkgname);
}

/*
 * Return best candidate for a remote package, taking into consideration any
 * preferred.conf matches.
 *
 * A return value of -1 indicates no remote packages matched the request.  A
 * return value of 0 with a NULL result indicates that all remote packages
 * failed to pass the preferred.conf requirements.  Otherwise 0 is returned
 * and result contains the best available package.
 */
int
find_preferred_pkg(const char *pkgname, char **result)
{
	Plistnumbered	*plist;
	Pkglist		*p, *pkg = NULL;

	*result = NULL;

	/* No matching packages available */
	if ((plist = find_remote_pkgs(pkgname)) == NULL)
		return -1;

	/* Find best match */
	SLIST_FOREACH(p, plist->P_Plisthead, next) {
		/*
		 * Check that the candidate matches any potential
		 * preferred.conf restrictions, if not then skip.
		 */
		if (chk_preferred(p->full, result) != 0)
			continue;

		/* Save best match */
		if (pkg == NULL)
			pkg = p;
		else if (dewey_cmp(p->version, DEWEY_GT, pkg->version))
			pkg = p;
	}

	if (pkg != NULL) {
		/* In case a previous version failed the match */
		if (*result != NULL)
			free(*result);
		*result = xstrdup(pkg->full);
	}

	free_pkglist(&plist->P_Plisthead);
	free(plist);

	return (pkg == NULL) ? 1 : 0;
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
 * Return first matching SLIST entry in package list, or NULL if no match.
 */
Pkglist *
find_pkg_match(Plisthead *plisthead, char *match)
{
	Pkglist	*plist;

	SLIST_FOREACH(plist, plisthead, next) {
		if (pkg_match(match, plist->full)) {
			TRACE("Matched %s with %s\n", match, plist->full);
			return plist;
		}
	}

	return NULL;
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

/* 
 * check whether or not pkgarg is a full pkgname (foo-1.0)
 */
char *
find_exact_pkg(Plisthead *plisthead, const char *pkgarg)
{
	Pkglist	*pkglist;
	char	*pkgname, *tmppkg;
	int		exact;
	size_t	tmplen;

	/* is it a versionned package ? */
	exact = exact_pkgfmt(pkgarg);

	/* check for package existence */
	SLIST_FOREACH(pkglist, plisthead, next) {
		tmppkg = xstrdup(pkglist->full);

		if (!exact) {
			/*
			 * pkgarg was not in exact format, i.e. foo-bar
			 * instead of foo-bar-1, truncate tmppkg :
			 * foo-bar-1.0 -> foo-bar
			 * and set len to tmppkg
			 */
			trunc_str(tmppkg, '-', STR_BACKWARD);
		}
		/* tmplen = strlen("foo-1{.vers}") */
		tmplen = strlen(tmppkg);

		if (strlen(pkgarg) == tmplen &&
			strncmp(tmppkg, pkgarg, tmplen) == 0) {
			XFREE(tmppkg);

			pkgname = xstrdup(pkglist->full);
			return pkgname;
		}
		XFREE(tmppkg);
	}

	return NULL;
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

static void
clear_pattern(char *depend)
{
	char *p;

	if ((p = strpbrk(depend, GLOBCHARS)) != NULL)
		*p = '\0';
	else
		return;

	/* get rid of trailing dash if any */
	if (*depend != '\0' && *--p == '-')
		*p = '\0';
}

static void
cleanup_version(char *pkgname)
{
	char	*exten;

	/* have we got foo-something ? */
	if ((exten = strrchr(pkgname, '-')) == NULL)
		return;

	/*
	 * -something has a dot, it's a version number
	 * unless it was something like clutter-gtk0.10
	 */
	if (isdigit((int)exten[1]) && strchr(exten, '.') != NULL)
		*exten = '\0';
}

uint8_t
non_trivial_glob(char *depend)
{
	if (charcount(depend, '[') > 1)
		return 1;

	return 0;
}

/*
 * AFAIK, here are the dewey/glob we can find as dependencies
 *
 * foo>=1.0 - 19129 entries
 * foo<1.0 - 1740 entries (only perl)
 * foo>1.0 - 44 entries
 * foo<=2.0 - 1
 * {foo>=1.0,bar>=2.0}
 * foo>=1.0<2.0
 * foo{-bar,-baz}>=1.0
 * foo{-bar,-baz}-[0-9]*
 * foo-{bar,baz}
 * foo-1.0{,nb[0-9]*} - 260
 * foo-[0-9]* - 3214
 * foo-[a-z]*-[0-9]* - not handled here, see pdb_rec_depends()
 * foo-1.0 - 20
 */
char *
get_pkgname_from_depend(char *depend)
{
	char	*pkgname, *tmp;

	if (depend == NULL || *depend == '\0')
		return NULL;

	/* 1. worse case, {foo>=1.0,bar-[0-9]*} */
	if (*depend == '{') {
		pkgname = xstrdup(depend + 1);
		tmp = strrchr(pkgname, '}');
		if (tmp != NULL)
			*tmp = '\0'; /* pkgname == "foo,bar" */

		/* {foo,bar} should always have comma */
		while ((tmp = strchr(pkgname, ',')) != NULL)
			*tmp = '\0'; /* pkgname == foo-[0-9]* or whatever */
	} else /* we should now have a "normal" pattern */
		pkgname = xstrdup(depend);

	/* 2. classic case, foo-[<>{?*\[] */
	clear_pattern(pkgname);

	/* 3. only foo-1.0 should remain */
	cleanup_version(pkgname);

	return pkgname;
}

char **
glob_to_pkgarg(char **globpkg, int *rc)
{
	size_t		count;
	int		i;
	char		**pkgargs = NULL;
	Pkglist		*plist;

	for (i = 0, count = 0; globpkg[i] != NULL; i++, count++) {
		pkgargs = xrealloc(pkgargs, (count + 2) * sizeof(char *));

		if ((plist = find_pkg_match(&r_plisthead, globpkg[i]))) {
			pkgargs[count] = xstrdup(plist->full);
		} else {
			fprintf(stderr, MSG_PKG_NOT_AVAIL, globpkg[i]);
			count--;
			*rc = EXIT_FAILURE;
		}
	}

	if (count == 0)
		XFREE(pkgargs);

	if (pkgargs != NULL)
		pkgargs[count] = NULL;

	return pkgargs;
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
