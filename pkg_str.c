/* $Id: pkg_str.c,v 1.12 2012/11/24 18:37:42 imilh Exp $ */

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

#include "pkgin.h"

#define GLOBCHARS "{<>[]?*"

/**
 * \fn unique_pkg
 *
 * Returns greatest version package matching in full package name form
 */
char *
unique_pkg(const char *pkgname, const char *dest)
{
	uint8_t		ispref = 0;
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
		/*
		 * there was a preferred.conf file and the current package
		 * matches one of the lines
		 */
		if (chk_preferred(current->full)) {
			/*
			 * package is listed in preferred.conf but the
			 * version doesn't match requirement
			 */
			ispref = 1;
			continue;
		}

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
	free_pkglist(&plist->P_Plisthead, LIST);
	free(plist);

	/* chosen package has no installation candidate */
	if (u_pkg == NULL && !ispref)
		printf(MSG_PKG_NOT_INSTALLABLE, pkgname);

	/* u_pkg might be NULL if a version is preferred and not available */
	return u_pkg;
}

/* return a pkgname corresponding to a dependency */
Pkglist *
map_pkg_to_dep(Plisthead *plisthead, char *depname)
{
	Pkglist	*plist;

	SLIST_FOREACH(plist, plisthead, next)
		if (pkg_match(depname, plist->full)) {
#ifdef DEBUG
			printf("match ! %s -> %s\n", depname, plist->full);
#endif
			return plist;
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
	int			i = 0, count = 0;
	char		**pkgargs = NULL;
	Pkglist		*plist;

	for (i = 0, count = 0; globpkg[i] != NULL; i++, count++) {
		pkgargs = xrealloc(pkgargs, (count + 2) * sizeof(char *));

		if (strpbrk(globpkg[i], GLOBCHARS) == NULL)
			pkgargs[count] = xstrdup(globpkg[i]);
		else {
			if ((plist = map_pkg_to_dep(&r_plisthead,
					globpkg[i])) == NULL) {
				fprintf(stderr, MSG_PKG_NOT_AVAIL, globpkg[i]);
				count--;
				*rc = EXIT_FAILURE;
			} else
				pkgargs[count] = xstrdup(plist->full);
		}
	}

	if (count == 0)
		XFREE(pkgargs);

	if (pkgargs != NULL)
		pkgargs[count] = NULL;

	return pkgargs;
}
