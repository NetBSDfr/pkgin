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

#include <sqlite3.h>
#include "pkgin.h"
#include <regex.h>

#define PKG_EQUAL	'='
#define PKG_GREATER	'>'
#define PKG_LESSER	'<'

Plistarray	*l_conflicthead;
Plisthead	l_plisthead[LOCAL_PKG_HASH_SIZE];
Plisthead	r_plisthead[REMOTE_PKG_HASH_SIZE];
int		r_plistcounter, l_plistcounter;

static void setfmt(char *, char *);

/*
 * Small structure for sorting package results.
 */
struct pkg_sort {
	char *full;
	char *name;
	char *version;
	char *comment;
	char flag;
};

static size_t
djb_hash(const char *s)
{
	size_t h = 5381;

	while (*s)
		h = h * 33 + (size_t)(unsigned char)*s++;
	return h;
}

size_t
pkg_hash_entry(const char *s, int size)
{
	return djb_hash(s) % size;
}

/**
 * \fn malloc_pkglist
 *
 * \brief Pkglist allocation for all types of lists
 */
Pkglist *
malloc_pkglist(void)
{
	Pkglist *pkglist;

	pkglist = xmalloc(sizeof(Pkglist));

	/*!< Init all the things! (http://knowyourmeme.com/memes/x-all-the-y) */
	pkglist->ipkg = NULL;
	pkglist->lpkg = NULL;
	pkglist->rpkg = NULL;
	pkglist->full = NULL;
	pkglist->name = NULL;
	pkglist->version = NULL;
	pkglist->build_date = NULL;
	pkglist->patterns = NULL;
	pkglist->patcount = 0;
	pkglist->replace = NULL;
	pkglist->size_pkg = 0;
	pkglist->file_size = 0;
	pkglist->level = 0;
	pkglist->download = 0;
	pkglist->pkgfs = NULL;
	pkglist->pkgurl = NULL;
	pkglist->comment = NULL;
	pkglist->category = NULL;
	pkglist->pkgpath = NULL;
	pkglist->skip = 0;
	pkglist->keep = 0;
	pkglist->sha256 = NULL;
	pkglist->action = ACTION_NONE;

	return pkglist;
}

/**
 * \fn free_pkglist_entry
 *
 * \brief free a Pkglist single entry
 */
void
free_pkglist_entry(Pkglist **plist)
{
	int i;

	XFREE((*plist)->sha256);
	XFREE((*plist)->pkgfs);
	XFREE((*plist)->pkgurl);
	XFREE((*plist)->full);
	XFREE((*plist)->name);
	XFREE((*plist)->version);
	XFREE((*plist)->build_date);
	XFREE((*plist)->category);
	XFREE((*plist)->pkgpath);
	XFREE((*plist)->comment);

	for (i = 0; i < (*plist)->patcount; i++)
		XFREE((*plist)->patterns[i]);

	XFREE((*plist)->patterns);
	XFREE((*plist)->replace);
	XFREE(*plist);

	plist = NULL;
}

/**
 * \fn free_pkglist
 *
 * \brief Free all types of package list
 */
void
free_pkglist(Plisthead **plisthead)
{
	Pkglist *plist;

	if (*plisthead == NULL)
		return;

	while (!SLIST_EMPTY(*plisthead)) {
		plist = SLIST_FIRST(*plisthead);
		SLIST_REMOVE_HEAD(*plisthead, next);

		free_pkglist_entry(&plist);
	}
	XFREE(*plisthead);

	plisthead = NULL;
}

/*
 * SQLite callback, record a local or remote package list entry.
 *
 * See LOCAL_PKGS_QUERY_ASC and REMOTE_PKGS_QUERY_ASC for the order of entries.
 */
#define DUP_OR_NULL(x, y)	x = (y) ? xstrdup(y) : NULL
#define NUM_OR_NULL(x, y)	x = (y) ? strtol(y, (char **)NULL, 10) : 0
static int
record_pkglist(void *param, int argc, char **argv, char **colname)
{
	Plistnumbered *plist = (Plistnumbered *)param;
	Pkglist *p;
	size_t val;

	if (argv == NULL)
		return PDB_ERR;

	p = malloc_pkglist();
	DUP_OR_NULL(p->full, argv[0]);
	DUP_OR_NULL(p->name, argv[1]);
	DUP_OR_NULL(p->version, argv[2]);
	DUP_OR_NULL(p->build_date, argv[3]);
	DUP_OR_NULL(p->comment, argv[4]);
	NUM_OR_NULL(p->file_size, argv[5]);
	NUM_OR_NULL(p->size_pkg, argv[6]);
	DUP_OR_NULL(p->category, argv[7]);
	DUP_OR_NULL(p->pkgpath, argv[8]);
	DUP_OR_NULL(p->sha256, argv[9]);

	/*
	 * Only LOCAL_PKG has PKG_KEEP.
	 */
	if (plist->P_type == 0 && argv[10])
		p->keep = 1;

	if (plist->P_type == 1) {
		if (p->file_size == 0) {
			fprintf(stderr, MSG_BAD_FILE_SIZE, p->full);
			free_pkglist_entry(&p);
			return PDB_ERR;
		}
	}

	if (plist->P_type == 0)
		val = pkg_hash_entry(p->name, LOCAL_PKG_HASH_SIZE);
	else {
		val = pkg_hash_entry(p->name, REMOTE_PKG_HASH_SIZE);
	}
	SLIST_INSERT_HEAD(&plist->P_Plisthead[val], p, next);

	plist->P_count++;

	return PDB_OK;
}
#undef DUP_OR_NULL
#undef NUM_OR_NULL

void
init_local_pkglist(void)
{
	Plistnumbered plist;
	int i;

	for (i = 0; i < LOCAL_PKG_HASH_SIZE; i++)
		SLIST_INIT(&l_plisthead[i]);

	plist.P_Plisthead = &l_plisthead[0];
	plist.P_count = 0;
	plist.P_type = 0;
	pkgindb_doquery(LOCAL_PKGS_QUERY_ASC, record_pkglist, &plist);
	l_plistcounter = plist.P_count;
}

void
init_remote_pkglist(void)
{
	Plistnumbered plist;
	int i;

	for (i = 0; i < REMOTE_PKG_HASH_SIZE; i++)
		SLIST_INIT(&r_plisthead[i]);

	plist.P_Plisthead = &r_plisthead[0];
	plist.P_count = 0;
	plist.P_type = 1;
	pkgindb_doquery(REMOTE_PKGS_QUERY_ASC, record_pkglist, &plist);
	r_plistcounter = plist.P_count;
}

int
is_empty_plistarray(Plistarray *a)
{
	int i;

	for (i = 0; i < a->size; i++) {
		if (!SLIST_EMPTY(&a->head[i]))
			return 0;
	}

	return 1;
}

int
is_empty_local_pkglist(void)
{
	int i;

	for (i = 0; i < LOCAL_PKG_HASH_SIZE; i++) {
		if (!SLIST_EMPTY(&l_plisthead[i]))
			return 0;
	}

	return 1;
}

int
is_empty_remote_pkglist(void)
{
	int i;

	for (i = 0; i < REMOTE_PKG_HASH_SIZE; i++) {
		if (!SLIST_EMPTY(&r_plisthead[i]))
			return 0;
	}

	return 1;
}

/*
 * These functions look for identical lpkg, rpkg, or pattern entries in a given
 * plist.  If size is non-zero then look through an array of plists.
 */
Pkglist *
pkgname_in_local_pkglist(const char *pkgname, Plisthead *plist, int size)
{
	Pkglist *p;
	int i;

	for (i = 0; i < size; i++) {
		SLIST_FOREACH(p, &plist[i], next) {
			if (p->lpkg && strcmp(p->lpkg->full, pkgname) == 0)
				return p;
		}
	}

	return NULL;
}
Pkglist *
pkgname_in_remote_pkglist(const char *pkgname, Plisthead *plist, int size)
{
	Pkglist *p;
	int i;

	for (i = 0; i < size; i++) {
		SLIST_FOREACH(p, &plist[i], next) {
			if (p->rpkg && strcmp(p->rpkg->full, pkgname) == 0)
				return p;
		}
	}

	return NULL;
}
Pkglist *
pattern_in_pkglist(const char *pattern, Plisthead *plist, int size)
{
	Pkglist *p;
	int c, i;

	for (i = 0; i < size; i++) {
		SLIST_FOREACH(p, &plist[i], next) {
			for (c = 0; c < p->patcount; c++) {
				if (strcmp(p->patterns[c], pattern) == 0)
					return p;
			}
		}
	}

	return NULL;
}

static void
free_global_pkglist(Plisthead *plisthead)
{
	Pkglist *plist;

	while (!SLIST_EMPTY(plisthead)) {
		plist = SLIST_FIRST(plisthead);
		SLIST_REMOVE_HEAD(plisthead, next);

		free_pkglist_entry(&plist);
	}
}

void
free_local_pkglist(void)
{
	int i;

	for (i = 0; i < LOCAL_PKG_HASH_SIZE; i++) {
		free_global_pkglist(&l_plisthead[i]);
	}
}

void
free_remote_pkglist(void)
{
	int i;

	for (i = 0; i < REMOTE_PKG_HASH_SIZE; i++) {
		free_global_pkglist(&r_plisthead[i]);
	}
}

Plistarray *
init_array(int size)
{
	Plistarray *a;
	int i;

	a = xmalloc(sizeof(Plistarray));
	a->size = size;
	a->head = xmalloc(sizeof(Plisthead *) * size);

	for (i = 0; i < size; i++)
		SLIST_INIT(&a->head[i]);

	return a;
}

void
free_array(Plistarray *a)
{
	Pkglist *p;
	int i;

	if (a == NULL)
		return;

	for (i = 0; i < a->size; i++) {
		while (!SLIST_EMPTY(&a->head[i])) {
			p = SLIST_FIRST(&a->head[i]);
			SLIST_REMOVE_HEAD(&a->head[i], next);
			free_pkglist_entry(&p);
		}
	}

	XFREE(a->head);
	XFREE(a);
	a = NULL;
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
	plist->P_type = 0;

	va_start(ap, fmt);
	sqlite3_vsnprintf(BUFSIZ, query, fmt, ap);
	va_end(ap);

	if (pkgindb_doquery(query, pdb_rec_list, plist) == PDB_OK)
		return plist;

	XFREE(plist->P_Plisthead);
	XFREE(plist);

	return NULL;
}

/* compare pkg version */
static int
pkg_is_installed(Pkglist *pkg)
{
	Pkglist *p;
	int l;

	for (l = 0; l < LOCAL_PKG_HASH_SIZE; l++ ) {
	SLIST_FOREACH(p, &l_plisthead[l], next) {
		/* make sure packages match */
		if (strcmp(p->name, pkg->name) != 0)
			continue;

		/* exact same version */
		if (strcmp(p->version, pkg->version) == 0)
			return 0;

		return version_check(p->full, pkg->full);
	}
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
	int		i, rc;
	char		pkgstatus, outpkg[BUFSIZ];
	char		sfmt[10], pfmt[10];

	setfmt(&sfmt[0], &pfmt[0]);

	/* list installed packages + status */
	if (lstype == PKG_LLIST_CMD && lslimit != '\0') {

		/* check if local package list is empty */
		if (is_empty_local_pkglist()) {
			fprintf(stderr, MSG_EMPTY_LOCAL_PKGLIST);
			return;
		}

		if (!is_empty_remote_pkglist()) {

			for (i = 0; i < REMOTE_PKG_HASH_SIZE; i++) {
			SLIST_FOREACH(plist, &r_plisthead[i], next) {
				rc = pkg_is_installed(plist);

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

	free_pkglist(&plisthead->P_Plisthead);
	free(plisthead);
}

/*
 * Sort a list of packages first by package name (alphabetically) and then by
 * version (highest first).
 */
static int
pkg_sort_cmp(const void *a, const void *b)
{
	const struct pkg_sort p1 = *(const struct pkg_sort *)a;
	const struct pkg_sort p2 = *(const struct pkg_sort *)b;

	/*
	 * First compare name, if they are the same then fall through to
	 * a version comparison.
	 */
	if (strcmp(p1.name, p2.name) > 0)
		return 1;
	else if (strcmp(p1.name, p2.name) < 0)
		return -1;

	if (dewey_cmp(p1.version, DEWEY_LT, p2.version))
		return 1;
	else if (dewey_cmp(p1.version, DEWEY_GT, p2.version))
		return -1;

	return 0;
}

int
search_pkg(const char *pattern)
{
	Pkglist	   	*plist;
	struct pkg_sort	*psort;
	regex_t		re;
	size_t		i, pcount = 0;
	int		rc;
	char		eb[64], is_inst, outpkg[BUFSIZ];
	int		matched = 0;
	char		sfmt[10], pfmt[10];

	setfmt(&sfmt[0], &pfmt[0]);

	if ((rc = regcomp(&re, pattern,
	    REG_EXTENDED|REG_NOSUB|REG_ICASE)) != 0) {
		regerror(rc, &re, eb, sizeof(eb));
		errx(EXIT_FAILURE, "regcomp: %s: %s", pattern, eb);
	}

	psort = xmalloc(sizeof(struct pkg_sort));

	for (i = 0; i < REMOTE_PKG_HASH_SIZE; i++) {
	SLIST_FOREACH(plist, &r_plisthead[i], next) {
		if (regexec(&re, plist->name, 0, NULL, 0) == 0 ||
		    regexec(&re, plist->full, 0, NULL, 0) == 0 ||
		    regexec(&re, plist->comment, 0, NULL, 0) == 0) {
			matched = 1;
			rc = pkg_is_installed(plist);

			if (rc == 0)
				is_inst = PKG_EQUAL;
			else if (rc == 1)
				is_inst = PKG_GREATER;
			else if (rc == 2)
				is_inst = PKG_LESSER;
			else
				is_inst = '\0';

			psort = xrealloc(psort, (pcount + 1) * sizeof(*psort));
			psort[pcount].full = xstrdup(plist->full);
			psort[pcount].name = xstrdup(plist->name);
			psort[pcount].version = xstrdup(plist->version);
			psort[pcount].comment = xstrdup(plist->comment);
			psort[pcount++].flag = is_inst;
		}
	}
	}

	qsort(psort, pcount, sizeof(struct pkg_sort), pkg_sort_cmp);

	for (i = 0; i < pcount; i++) {
		snprintf(outpkg, BUFSIZ, sfmt, psort[i].full, psort[i].flag);
		printf(pfmt, outpkg, psort[i].comment);

		XFREE(psort[i].full);
		XFREE(psort[i].name);
		XFREE(psort[i].version);
		XFREE(psort[i].comment);
	}

	XFREE(psort);

	regfree(&re);

	if (matched) {
		printf(MSG_IS_INSTALLED_CODE);
		return EXIT_SUCCESS;
	} else {
		printf(MSG_NO_SEARCH_RESULTS, pattern);
		return EXIT_FAILURE;
	}
}

void
show_category(char *category)
{
	Pkglist	   	*plist;
	int i;

	for (i = 0; i < REMOTE_PKG_HASH_SIZE; i++) {
	SLIST_FOREACH(plist, &r_plisthead[i], next) {
		if (plist->category == NULL)
			continue;
		if (strcmp(plist->category, category) == 0)
			printf("%-20s %s\n", plist->full, plist->comment);
	}
	}
}

int
show_pkg_category(char *pkgname)
{
	Pkglist	   	*plist;
	int		i, matched = 0;

	for (i = 0; i < REMOTE_PKG_HASH_SIZE; i++) {
	SLIST_FOREACH(plist, &r_plisthead[i], next) {
		if (strcmp(plist->name, pkgname) == 0) {
			matched = 1;
			if (plist->category == NULL)
				continue;
			printf("%-12s - %s\n", plist->category, plist->full);
		}
	}
	}

	if (matched)
		return EXIT_SUCCESS;
	else {
		fprintf(stderr, MSG_PKG_NOT_AVAIL, pkgname);
		return EXIT_FAILURE;
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

	free_pkglist(&cathead->P_Plisthead);
	free(cathead);
}
