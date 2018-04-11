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
#include <regex.h>

Plisthead	r_plisthead, l_plisthead;
int		r_plistcounter, l_plistcounter;

static void setfmt(char *, char *);

/**
 * \fn malloc_pkglist
 *
 * \brief Pkglist allocation for all types of lists
 */
Pkglist *
malloc_pkglist(uint8_t type)
{
	Pkglist *pkglist;

	pkglist = xmalloc(sizeof(Pkglist));

	/*!< Init all the things! (http://knowyourmeme.com/memes/x-all-the-y) */
	pkglist->type = type;
	pkglist->full = NULL;
	pkglist->name = NULL;
	pkglist->version = NULL;
	pkglist->depend = NULL;
	pkglist->size_pkg = 0;
	pkglist->old_size_pkg = -1;
	pkglist->file_size = 0;
	pkglist->level = 0;

	switch (type) {
	case LIST:
		pkglist->comment = NULL;
		pkglist->category = NULL;
		pkglist->pkgpath = NULL;
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

/**
 * \fn free_pkglist_entry
 *
 * \brief free a Pkglist single entry
 */
void
free_pkglist_entry(Pkglist **plist, uint8_t type)
{
	XFREE((*plist)->full);
	XFREE((*plist)->name);
	XFREE((*plist)->version);
	XFREE((*plist)->depend);
	switch (type) {
	case LIST:
		XFREE((*plist)->comment);
		XFREE((*plist)->category);
		XFREE((*plist)->pkgpath);
		break;
	case IMPACT:
		XFREE((*plist)->old);
	}
	XFREE(*plist);

	plist = NULL;
}

/**
 * \fn free_pkglist
 *
 * \brief Free all types of package list
 */
void
free_pkglist(Plisthead **plisthead, uint8_t type)
{
	Pkglist *plist;

	if (*plisthead == NULL)
		return;

	while (!SLIST_EMPTY(*plisthead)) {
		plist = SLIST_FIRST(*plisthead);
		SLIST_REMOVE_HEAD(*plisthead, next);

		free_pkglist_entry(&plist, type);
	}
	XFREE(*plisthead);

	plisthead = NULL;
}

void
init_global_pkglists()
{
	Plistnumbered plist;

	SLIST_INIT(&r_plisthead);
	plist.P_Plisthead = &r_plisthead;
	plist.P_count = 0;
	pkgindb_doquery(REMOTE_PKGS_QUERY_ASC, pdb_rec_list, &plist);
	r_plistcounter = plist.P_count;

	SLIST_INIT(&l_plisthead);
	plist.P_Plisthead = &l_plisthead;
	plist.P_count = 0;
	pkgindb_doquery(LOCAL_PKGS_QUERY_ASC, pdb_rec_list, &plist);
	l_plistcounter = plist.P_count;
}

static void
free_global_pkglist(Plisthead *plisthead)
{
	Pkglist *plist;

	while (!SLIST_EMPTY(plisthead)) {
		plist = SLIST_FIRST(plisthead);
		SLIST_REMOVE_HEAD(plisthead, next);

		free_pkglist_entry(&plist, LIST);
	}
}

void
free_global_pkglists()
{
	free_global_pkglist(&l_plisthead);
	free_global_pkglist(&r_plisthead);
}

/**
 * \fn init_head
 *
 * \brief Init a Plisthead
 */
Plisthead *
init_head(void)
{
	Plisthead *plisthead;

	plisthead = xmalloc(sizeof(Plisthead));
	SLIST_INIT(plisthead);

	return plisthead;
}

/**
 * \fn rec_pkglist
 *
 * Record package list to SLIST
 */
Plistnumbered *
rec_pkglist(const char *fmt, ...)
{
	char		query[BUFSIZ];
	va_list		ap;
	Plistnumbered	*plist;

	plist = (Plistnumbered *)malloc(sizeof(Plistnumbered));
	plist->P_Plisthead = init_head();
	plist->P_count = 0;

	va_start(ap, fmt);
	vsnprintf(query, BUFSIZ, fmt, ap);
	va_end(ap);

	if (pkgindb_doquery(query, pdb_rec_list, plist) == PDB_OK)
		return plist;

	XFREE(plist->P_Plisthead);
	XFREE(plist);

	return NULL;
}

/* compare pkg version */
int
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

static void
setfmt(char *sfmt, char *pfmt)
{
	if (pflag) {
		strncpy(sfmt, "%s;%c", 6); /* snprintf(outpkg) */
		strncpy(pfmt, "%s;%s\n", 7); /* final printf */
	} else {
		strncpy(sfmt, "%s %c", 6);
		strncpy(pfmt, "%-20s %s\n", 10);
	}
}

void
list_pkgs(const char *pkgquery, int lstype)
{
	Pkglist	   	*plist;
	Plistnumbered	*plisthead;
	int		rc;
	char		pkgstatus, outpkg[BUFSIZ];
	char		sfmt[10], pfmt[10];

	setfmt(&sfmt[0], &pfmt[0]);

	/* list installed packages + status */
	if (lstype == PKG_LLIST_CMD && lslimit != '\0') {

		/* check if local package list is empty */
		if (SLIST_EMPTY(&l_plisthead)) {
			fprintf(stderr, MSG_EMPTY_LOCAL_PKGLIST);
			return;
		}

		if (!SLIST_EMPTY(&r_plisthead)) {

			SLIST_FOREACH(plist, &r_plisthead, next) {
				rc = pkg_is_installed(&l_plisthead, plist);

				pkgstatus = '\0';

				if (lslimit == PKG_EQUAL && rc == 0)
					pkgstatus = PKG_EQUAL;
				if (lslimit == PKG_GREATER && rc == 1)
					pkgstatus = PKG_GREATER;
				if (lslimit == PKG_LESSER && rc == 2)
					pkgstatus = PKG_LESSER;

				if (pkgstatus != '\0') {
					snprintf(outpkg, BUFSIZ, sfmt,
						plist->full, pkgstatus);
					printf(pfmt, outpkg, plist->comment);
				}

			}
		}

		return;
	} /* lstype == LLIST && status */

	/* regular package listing */
	if ((plisthead = rec_pkglist(pkgquery)) == NULL) {
		fprintf(stderr, MSG_EMPTY_LIST);
		return;
	}

	SLIST_FOREACH(plist, plisthead->P_Plisthead, next)
		printf(pfmt, plist->full, plist->comment);

	free_pkglist(&plisthead->P_Plisthead, LIST);
	free(plisthead);
}

int
search_pkg(const char *pattern)
{
	Pkglist	   	*plist;
	regex_t		re;
	int		rc;
	char		eb[64], is_inst, outpkg[BUFSIZ];
	int		matched_pkgs;
	char		sfmt[10], pfmt[10];

	setfmt(&sfmt[0], &pfmt[0]);

	matched_pkgs = 0;

	if (!SLIST_EMPTY(&r_plisthead)) {

		if ((rc = regcomp(&re, pattern,
			REG_EXTENDED|REG_NOSUB|REG_ICASE)) != 0) {
			regerror(rc, &re, eb, sizeof(eb));
			errx(1, "regcomp: %s: %s", pattern, eb);
		}

		SLIST_FOREACH(plist, &r_plisthead, next) {
			is_inst = '\0';

			if (regexec(&re, plist->name, 0, NULL, 0) == 0 ||
				regexec(&re, plist->comment,
					0, NULL, 0) == 0) {

				matched_pkgs = 1;

				if (!SLIST_EMPTY(&l_plisthead)) {
					rc = pkg_is_installed(&l_plisthead,
								plist);

					if (rc == 0)
						is_inst = PKG_EQUAL;
					if (rc == 1)
						is_inst = PKG_GREATER;
					if (rc == 2)
						is_inst = PKG_LESSER;

				}

				snprintf(outpkg, BUFSIZ, sfmt,
						plist->full, is_inst);

				printf(pfmt, outpkg, plist->comment);
			}
		}

		regfree(&re);

		if (matched_pkgs == 1)
			printf(MSG_IS_INSTALLED_CODE);
		else {
			printf(MSG_NO_SEARCH_RESULTS, pattern);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

void
show_category(char *category)
{
	Pkglist	   	*plist;

	SLIST_FOREACH(plist, &r_plisthead, next) {
		if (strcmp(plist->category, category) == 0)
			printf("%-20s %s\n", plist->full, plist->comment);
	}
}

void
show_pkg_category(char *pkgname)
{
	Pkglist	   	*plist;

	SLIST_FOREACH(plist, &r_plisthead, next) {
		if (strcmp(plist->name, pkgname) == 0)
			printf("%-12s - %s\n", plist->category, plist->full);
	}
}

void
show_all_categories(void)
{
	Plistnumbered	*cathead;
	Pkglist			*plist;

	if ((cathead = rec_pkglist(SHOW_ALL_CATEGORIES)) == NULL) {
		fprintf(stderr, MSG_NO_CATEGORIES);
		return;
	}

	SLIST_FOREACH(plist, cathead->P_Plisthead, next)
		printf("%s\n", plist->full);

	free_pkglist(&cathead->P_Plisthead, LIST);
	free(cathead);
}
