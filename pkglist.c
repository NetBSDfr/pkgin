/* $Id: pkglist.c,v 1.2.2.1 2011/08/15 09:26:12 imilh Exp $ */

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
#include <regex.h>

void
free_pkglist(Plisthead *plisthead)
{
	Pkglist *plist;

	if (plisthead == NULL)
		return;

	while (!SLIST_EMPTY(plisthead)) {
		plist = SLIST_FIRST(plisthead);
		SLIST_REMOVE_HEAD(plisthead, next);
		XFREE(plist->fullpkgname);
		XFREE(plist->pkgname);
		XFREE(plist->pkgvers);
		XFREE(plist->comment);
		XFREE(plist);
	}
	XFREE(plisthead);
}

#define FULLPKGNAME	argv[0]
#define PKGNAME		argv[1]
#define PKGVERS		argv[2]
#define COMMENT		argv[3]
#define FILE_SIZE	argv[4]
#define SIZE_PKG	argv[5]
/* 
 * sqlite callback, record package list
 */
static int
pdb_rec_pkglist(void *param, int argc, char **argv, char **colname)
{
	Pkglist		*plist;
	Plisthead 	*plisthead = (Plisthead *)param;

	if (argv == NULL)
		return PDB_ERR;

	/* FULLPKGNAME was empty, probably a package installed
	 * from pkgsrc or wip that does not exist in
	 * pkg_summary(5), return
	 */
	if (FULLPKGNAME == NULL)
		return PDB_OK;

	XMALLOC(plist, sizeof(Pkglist));
	XSTRDUP(plist->fullpkgname, FULLPKGNAME);

	plist->size_pkg = 0;
	plist->file_size = 0;

	/* classic pkglist, has COMMENT and SIZEs */
	if (argc > 1) {
		if (COMMENT == NULL) {
			/* COMMENT or SIZEs were empty
			 * not a valid pkg_summary(5) entry, return
			 */
			XFREE(plist->fullpkgname);
			XFREE(plist);
			return PDB_OK;
		}

		XSTRDUP(plist->pkgname, PKGNAME);
		XSTRDUP(plist->pkgvers, PKGVERS);
		XSTRDUP(plist->comment, COMMENT);
		if (FILE_SIZE != NULL)
			plist->file_size = strtol(FILE_SIZE, (char **)NULL, 10);
		if (SIZE_PKG != NULL)
			plist->size_pkg = strtol(SIZE_PKG, (char **)NULL, 10);

	} else
		/* conflicts or requires list, only pkgname needed */
		plist->comment = NULL;

	SLIST_INSERT_HEAD(plisthead, plist, next);

	return PDB_OK;
}

Plisthead *
rec_pkglist(const char *pkgquery)
{
	Plisthead *plisthead;

	XMALLOC(plisthead, sizeof(Plisthead));

	SLIST_INIT(plisthead);

	if (pkgindb_doquery(pkgquery, pdb_rec_pkglist, plisthead) == 0)
		return plisthead;

	XFREE(plisthead);
	return NULL;
}

/* compare pkg version */
static int
pkg_is_installed(Plisthead *plisthead, Pkglist *pkg)
{
	Pkglist		*pkglist;

	SLIST_FOREACH(pkglist, plisthead, next) {
		/* make sure packages match */
		if (strcmp(pkglist->pkgname, pkg->pkgname) != 0)
			continue;

		/* exact same version */
		if (strcmp(pkglist->pkgvers, pkg->pkgvers) == 0)
			return 0;

		return version_check(pkglist->fullpkgname, pkg->fullpkgname);
	}

	return -1;
}

void
list_pkgs(const char *pkgquery, int lstype)
{
	Pkglist 	*plist;
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
						plist->fullpkgname, pkgstatus);
					printf("%-20s %s\n", outpkg, plist->comment);
				}

			}
			free_pkglist(plisthead);
		}
		free_pkglist(localplisthead);
		return;
	} /* lstype == LLIST && status */

	if ((plisthead = rec_pkglist(pkgquery)) != NULL) {
		SLIST_FOREACH(plist, plisthead, next)
			printf("%-20s %s\n", plist->fullpkgname, plist->comment);

		free_pkglist(plisthead);
	}


}

void
search_pkg(const char *pattern)
{
	Pkglist 	*plist;
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

			if (regexec(&re, plist->pkgname, 0, NULL, 0) == 0 ||
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

				snprintf(outpkg, BUFSIZ, "%s %c", plist->fullpkgname, is_inst);

				printf("%-20s %s\n", outpkg, plist->comment);
			}
		}

		free_pkglist(plisthead);

		if (localplisthead != NULL)
			free_pkglist(localplisthead);

		regfree(&re);

		if (matched_pkgs == 1)
			printf(MSG_IS_INSTALLED_CODE);
		else
			printf(MSG_NO_SEARCH_RESULTS, pattern);
	}
}

char *
find_exact_pkg(Plisthead *plisthead, const char *pkgarg)
{
	Pkglist	*pkglist;
	char	*pkgname, *tmppkg;
	int		tmplen, exact;

	/* truncate argument if it contains a version */
	exact = exact_pkgfmt(pkgarg);

	/* check for package existence */
	SLIST_FOREACH(pkglist, plisthead, next) {
		XSTRDUP(tmppkg, pkglist->pkgname);

		if (!exact) {
			/*
			 * pkgname was not in exact format, i.e. foo-bar
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

			XSTRDUP(pkgname, pkglist->pkgname);
			return pkgname;
		}
		XFREE(tmppkg);
	}

	return NULL;
}

/* end an expression with a delimiter */
char *
end_expr(Plisthead *plisthead, const char *str)
{
	Pkglist	*pkglist;
	char	*expr, *p;
	uint8_t	was_dep = 0;

	/* enough space to add a delimiter */
	XMALLOC(expr, (strlen(str) + 2) * sizeof(char));
	XSTRCPY(expr, str);

	/* really don't want to fight with {foo,bar>=2.0}, rely on pkg_match()
	 * XXX: this makes us load pkg_summary db in full_dep_tree()
	 */
	if (strpbrk(expr, "*?[{") != NULL) {
		SLIST_FOREACH(pkglist, plisthead, next) {
			if (pkg_match(expr, pkglist->pkgname)) {
				XFREE(expr);
				XSTRDUP(expr, pkglist->pkgname);
				break;
			}
		}
	}

	/* simple package/dependency case, parse string backwards
	 * until we find a delimiter: foo>=1.0, bar>=1.2<2.0
	 */
	while ((p = match_dep_ext(expr, "<>=")) != NULL) {
		*p++ = DELIMITER; /* terminate expr */
		*p = '\0';
		was_dep = 1;
	}
	/* expr was not a dewey pattern, it's a full package name,
	 * strip the final dash if found
	 */
	if (!was_dep && (p = strrchr(expr, '-')) != NULL) {
		*p++ = DELIMITER; /* terminate expr */
		*p = '\0';
	}

	return expr;
}
