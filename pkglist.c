/* $Id: pkglist.c,v 1.2.2.8 2011/08/19 08:25:35 imilh Exp $ */

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

#include "pkgin.h"
#include <regex.h>

/**
 * Pkglist allocation
 */
Pkglist *
malloc_pkglist(uint8_t type)
{
	Pkglist *pkglist;

	XMALLOC(pkglist, sizeof(Pkglist));

	/*!< Init all the things ! (http://knowyourmeme.com/memes/x-all-the-y) */
	pkglist->type = type;
	pkglist->full = NULL;
	pkglist->name = NULL;
	pkglist->version = NULL;
	pkglist->depend = NULL;
	pkglist->size_pkg = 0;
	pkglist->file_size = 0;
	pkglist->level = 0;

	switch (type) {
	case LIST:
		pkglist->comment = NULL;
		break;
	case DEPTREE:
		pkglist->computed = 0;
		pkglist->keep = 0;
		break;
	case IMPACT:
		pkglist->action = DONOTHING;
		pkglist->old = NULL;
		break;
	}

	return pkglist;
}

void
free_pkglist(Plisthead *plisthead, uint8_t type)
{
	Pkglist *plist;

	if (plisthead == NULL)
		return;

	while (!SLIST_EMPTY(plisthead)) {
		plist = SLIST_FIRST(plisthead);
		SLIST_REMOVE_HEAD(plisthead, next);
		XFREE(plist->full);
		XFREE(plist->name);
		XFREE(plist->version);
		XFREE(plist->depend);
		switch (type) {
		case LIST:
			XFREE(plist->comment);
			break;
		case IMPACT:
			XFREE(plist->old);
		}
		XFREE(plist);
	}
	XFREE(plisthead);

	plisthead = NULL;
}

Plisthead *
rec_pkglist(const char *pkgquery)
{
	Plisthead *plisthead;

	XMALLOC(plisthead, sizeof(Plisthead));

	SLIST_INIT(plisthead);

	if (pkgindb_doquery(pkgquery, pdb_rec_list, plisthead) == 0)
		return plisthead;

	XFREE(plisthead);
	return NULL;
}

/* compare pkg version */
static int
pkg_is_installed(Plisthead *plisthead, Pkglist *pkg)
{
	Pkglist *pkglist;

	SLIST_FOREACH(pkglist, plisthead, next) {
		/* make sure packages match */
		if (strcmp(pkglist->name, pkg->name) != 0)
			continue;

		/* exact same version */
		if (strcmp(pkglist->version, pkg->version) == 0)
			return 0;

		return version_check(pkglist->full, pkg->full);
	}

	return -1;
}

void
list_pkgs(const char *pkgquery, int lstype)
{
	Pkglist		 	*plist;
	Plisthead 	*plisthead, *localplisthead = NULL;
	int			rc;
	char		pkgstatus, outpkg[BUFSIZ];

	/* list installed packages + status */
	if (lstype == PKG_LLIST_CMD && lslimit != '\0') {
		localplisthead = rec_pkglist(LOCAL_PKGS_QUERY);

		if (localplisthead == NULL)
			return;

		if ((plisthead = rec_pkglist(REMOTE_PKGS_QUERY)) != NULL) {

			SLIST_FOREACH(plist, plisthead, next) {
				rc = pkg_is_installed(localplisthead, plist);

				pkgstatus = '\0';

				if (lslimit == PKG_EQUAL && rc == 0)
					pkgstatus = PKG_EQUAL;
				if (lslimit == PKG_GREATER && rc == 1)
					pkgstatus = PKG_GREATER;
				if (lslimit == PKG_LESSER && rc == 2)
					pkgstatus = PKG_LESSER;

				if (pkgstatus != '\0') {
					snprintf(outpkg, BUFSIZ, "%s %c",
						plist->full, pkgstatus);
					printf("%-20s %s\n", outpkg, plist->comment);
				}

			}
			free_pkglist(plisthead, LIST);
		}
		free_pkglist(localplisthead, LIST);
		return;
	} /* lstype == LLIST && status */

	if ((plisthead = rec_pkglist(pkgquery)) != NULL) {
		SLIST_FOREACH(plist, plisthead, next)
			printf("%-20s %s\n", plist->full, plist->comment);

		free_pkglist(plisthead, LIST);
	}


}

void
search_pkg(const char *pattern)
{
	Pkglist		 	*plist;
	Plisthead 	*plisthead, *localplisthead;
	regex_t		re;
	int			rc;
	char		eb[64], is_inst, outpkg[BUFSIZ];
	int		matched_pkgs;

	localplisthead = rec_pkglist(LOCAL_PKGS_QUERY);
	matched_pkgs = 0;

	if ((plisthead = rec_pkglist(REMOTE_PKGS_QUERY)) != NULL) {
		if ((rc = regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB|REG_ICASE))
			!= 0) {
			regerror(rc, &re, eb, sizeof(eb));
			errx(1, "regcomp: %s: %s", pattern, eb);
		}

		SLIST_FOREACH(plist, plisthead, next) {
			is_inst = '\0';

			if (regexec(&re, plist->name, 0, NULL, 0) == 0 ||
				regexec(&re, plist->comment, 0, NULL, 0) == 0) {

				matched_pkgs = 1;

				if (localplisthead != NULL) {
					rc = pkg_is_installed(localplisthead, plist);

					if (rc == 0)
						is_inst = PKG_EQUAL;
					if (rc == 1)
						is_inst = PKG_GREATER;
					if (rc == 2)
						is_inst = PKG_LESSER;

				}

				snprintf(outpkg, BUFSIZ, "%s %c", plist->full, is_inst);

				printf("%-20s %s\n", outpkg, plist->comment);
			}
		}

		free_pkglist(plisthead,LIST);

		if (localplisthead != NULL)
			free_pkglist(localplisthead, LIST);

		regfree(&re);

		if (matched_pkgs == 1)
			printf(MSG_IS_INSTALLED_CODE);
		else
			printf(MSG_NO_SEARCH_RESULTS, pattern);
	}
}

/* count if there's many packages with the same basename */
int
count_samepkg(Plisthead *plisthead, const char *pkgname)
{
	Pkglist		*pkglist;
	char	*plistpkg = NULL, **samepkg = NULL;
	int		count = 0, num = 0, pkglen, pkgfmt = 0;

	/* record if it's a versionned pkgname */
	if (exact_pkgfmt(pkgname))
		pkgfmt = 1;

	/* count if there's many packages with this name */
	SLIST_FOREACH(pkglist, plisthead, next) {

		XSTRDUP(plistpkg, pkglist->full);

		pkglen = strlen(pkgname);

		/* pkgname len from list is smaller than argument */
		if (strlen(plistpkg) < pkglen) {
			XFREE(plistpkg);
			continue;
		}

		/* pkgname argument contains a version, foo-3.* */
		if (pkgfmt)
			/* cut plistpkg foo-3.2.1 to foo-3 */
			plistpkg[pkglen] = '\0';
		else {
			/* was not a versionned parameter,
			 * check if plistpkg next chars are -[0-9]
			 */
			if (plistpkg[pkglen] != '-' &&
				!isdigit((int)plistpkg[pkglen + 1])) {
				XFREE(plistpkg);
				continue;
			}
			/* truncate foo-3.2.1 to foo */
			trunc_str(plistpkg, '-', STR_BACKWARD);
			pkglen = max(strlen(pkgname), strlen(plistpkg));
		}

		if (strncmp(pkgname, plistpkg, pkglen) == 0) {
			XREALLOC(samepkg, (count + 2) * sizeof(char *));
			XSTRDUP(samepkg[count], pkglist->full);
			samepkg[count + 1] = NULL;

			count++;
		}

		XFREE(plistpkg);
	}

	if (count > 1) { /* there was more than one reference */
		printf(MSG_MORE_THAN_ONE_VER, getprogname());
		for (num = 0; num < count; num++)
			printf("%s\n", samepkg[num]);
	}

	free_list(samepkg);

	return count;
}

/* return a pkgname corresponding to a dependency */
Pkglist *
map_pkg_to_dep(Plisthead *plisthead, char *depname)
{
	Pkglist	*plist;

	SLIST_FOREACH(plist, plisthead, next)
		if (pkg_match(depname, plist->full))
			return plist;

	return NULL;
}
