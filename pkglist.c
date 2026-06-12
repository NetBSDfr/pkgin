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
 *
 * All defaults are zero except action, which is set explicitly.
 */
Pkglist *
malloc_pkglist(void)
{
	Pkglist *pkglist;

	pkglist = xcalloc(1, sizeof(Pkglist));
	pkglist->action = ACTION_NONE;

	return pkglist;
}

/*
 * Allocate a Pkglist entry for a DEPENDS-style pattern, with an optional
 * package name if it can be determined from the pattern.
 */
Pkglist *
pattern_pkglist(const char *pattern, const char *name)
{
	Pkglist *p;

	p = malloc_pkglist();
	p->patterns = xmalloc(2 * sizeof(char *));
	p->patterns[0] = xstrdup(pattern);
	p->patterns[1] = NULL;
	p->patcount = 1;
	if (name)
		p->name = xstrdup(name);

	return p;
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
#define NUM_OR_NULL(x, y)	x = (y) ? strtoll(y, (char **)NULL, 10) : 0
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

	/*
	 * Only LOCAL_PKG has PKG_KEEP.
	 */
	if (plist->P_type == 0 && argv[9])
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

/*
 * Check whether an array of Plistheads contains any entries.
 */
static int
is_empty_plisthead_array(Plisthead *plist, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (!SLIST_EMPTY(&plist[i]))
			return 0;
	}

	return 1;
}

int
is_empty_plistarray(Plistarray *a)
{
	return is_empty_plisthead_array(a->head, a->size);
}

int
is_empty_local_pkglist(void)
{
	return is_empty_plisthead_array(l_plisthead, LOCAL_PKG_HASH_SIZE);
}

int
is_empty_remote_pkglist(void)
{
	return is_empty_plisthead_array(r_plisthead, REMOTE_PKG_HASH_SIZE);
}

/*
 * These functions look for identical lpkg, rpkg, or pattern entries in a given
 * plist.  If size is non-zero then look through an array of plists.
 */
static Pkglist *
pkgname_in_pkglist(const char *pkgname, Plisthead *plist, int size, int remote)
{
	Pkglist *p, *cmp;
	int i;

	for (i = 0; i < size; i++) {
		SLIST_FOREACH(p, &plist[i], next) {
			cmp = remote ? p->rpkg : p->lpkg;
			if (cmp && strcmp(cmp->full, pkgname) == 0)
				return p;
		}
	}

	return NULL;
}
Pkglist *
pkgname_in_local_pkglist(const char *pkgname, Plisthead *plist, int size)
{
	return pkgname_in_pkglist(pkgname, plist, size, 0);
}
Pkglist *
pkgname_in_remote_pkglist(const char *pkgname, Plisthead *plist, int size)
{
	return pkgname_in_pkglist(pkgname, plist, size, 1);
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

/*
 * Free all entries from an array of Plistheads.
 */
static void
free_pkglist_entries(Plisthead *plist, int size)
{
	Pkglist *p;
	int i;

	for (i = 0; i < size; i++) {
		while (!SLIST_EMPTY(&plist[i])) {
			p = SLIST_FIRST(&plist[i]);
			SLIST_REMOVE_HEAD(&plist[i], next);
			free_pkglist_entry(&p);
		}
	}
}

void
free_local_pkglist(void)
{
	free_pkglist_entries(l_plisthead, LOCAL_PKG_HASH_SIZE);
}

void
free_remote_pkglist(void)
{
	free_pkglist_entries(r_plisthead, REMOTE_PKG_HASH_SIZE);
}

Plistarray *
init_array(int size)
{
	Plistarray *a;
	int i;

	a = xmalloc(sizeof(Plistarray));
	a->size = size;
	a->head = xmalloc(sizeof(*a->head) * size);

	for (i = 0; i < size; i++)
		SLIST_INIT(&a->head[i]);

	return a;
}

void
free_array(Plistarray *a)
{
	if (a == NULL)
		return;

	free_pkglist_entries(a->head, a->size);

	XFREE(a->head);
	XFREE(a);
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

/*
 * Some code paths use an indirect pointer via ipkg to a Pkglist entry and
 * others go direct.  Resolve to the entry that carries the package data.
 */
Pkglist *
get_pkglist_ptr(Pkglist *p)
{
	return (p->ipkg) ? p->ipkg : p;
}

/*
 * Return the full package name for an entry, resolving any impact indirection
 * and preferring the remote package name over the local one.
 */
char *
pkglist_full(Pkglist *p)
{
	p = get_pkglist_ptr(p);
	return (p->rpkg) ? p->rpkg->full : p->lpkg->full;
}

/*
 * qsort callback to sort an array of "Pkglist *" alphabetically by full
 * package name.
 */
int
sort_pkglist_alpha(const void *a, const void *b)
{
	Pkglist *pa = *(Pkglist * const *)a;
	Pkglist *pb = *(Pkglist * const *)b;

	return strcmp(pkglist_full(pa), pkglist_full(pb));
}

/*
 * Return a NULL-terminated, alphabetically sorted array of the entries in a
 * list.  If action is not ACTION_NONE then only entries with a matching action
 * are included.  The caller frees the returned array; the entries themselves
 * remain owned by the list.
 */
Pkglist **
sorted_pkglist(Plisthead *pkgs, action_t action)
{
	Pkglist *pkg, *p, **list;
	size_t i = 0;

	SLIST_FOREACH(pkg, pkgs, next) {
		p = get_pkglist_ptr(pkg);
		if (action == ACTION_NONE || p->action == action)
			i++;
	}

	list = xmalloc((i + 1) * sizeof(*list));

	i = 0;
	SLIST_FOREACH(pkg, pkgs, next) {
		p = get_pkglist_ptr(pkg);
		if (action == ACTION_NONE || p->action == action)
			list[i++] = pkg;
	}
	list[i] = NULL;

	qsort(list, i, sizeof(*list), sort_pkglist_alpha);

	return list;
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

/*
 * Set plain or parsable (-p) output formats used by list and search,
 * sfmt builds the package/status string, pfmt prints the final line.
 */
static void
setfmt(const char **sfmt, const char **pfmt)
{
	*sfmt = pflag ? "%s;%c" : "%s %c";
	*pfmt = pflag ? "%s;%s\n" : "%-20s %s\n";
}

void
list_pkgs(const char *pkgquery, int lstype)
{
	Pkglist	   	*plist;
	Plistnumbered	*plisthead;
	int		i, rc;
	char		pkgstatus, outpkg[BUFSIZ];
	const char	*sfmt, *pfmt;

	setfmt(&sfmt, &pfmt);

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
	const char	*sfmt, *pfmt;

	setfmt(&sfmt, &pfmt);

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
		    (plist->comment != NULL &&
		    regexec(&re, plist->comment, 0, NULL, 0) == 0)) {
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
			psort[pcount].comment =
			    xstrdup(plist->comment ? plist->comment : "");
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
