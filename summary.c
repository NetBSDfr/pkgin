/* $Id: summary.c,v 1.1.1.1 2011/03/03 14:43:13 imilh Exp $ */

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

#include "tools.h"
#include "pkgin.h"

static const struct Summary {
	const int	type;
	const char	*tbl_name;
	const char	*deps;
	const char	*conflicts;
	const char	*requires;
	const char	*provides;
	const char	*end;
} sumsw[] = {
	{
		LOCAL_SUMMARY,
		"LOCAL_PKG",
		"LOCAL_DEPS",
		"LOCAL_CONFLICTS",
		"LOCAL_REQUIRES",
		"LOCAL_PROVIDES",
		NULL
	},
	{
		REMOTE_SUMMARY,
		"REMOTE_PKG",
		"REMOTE_DEPS",
		"REMOTE_CONFLICTS",
		"REMOTE_REQUIRES",
		"REMOTE_PROVIDES",
		NULL
	},
};

struct Columns {
	int		num;
	char	**name;
} cols;

typedef struct Insertlist {
	char	*field;
	char	*value;
	SLIST_ENTRY(Insertlist) next;
} Insertlist;

SLIST_HEAD(, Insertlist) inserthead;

static char	**fetch_summary(char *url);
static void	freecols(void);
static void	free_insertlist(void);
static void	prepare_insert(int, struct Summary, char *);
int			colnames(void *, int, char **, char **);

char		**commit_list = NULL;
int			commit_idx = 0;
int			query_size = BUFSIZ;
/* column count for table fields, given by colnames callback */
int			colcount = 0;
/* force pkg_summary reload */
int			force_fetch = 0;

static const char *const sumexts[] = { "bz2", "gz", NULL };

/* remote summary fetch */
static char **
fetch_summary(char *cur_repo)
{
	/* from pkg_install/files/admin/audit.c */
	Dlfile	*file = NULL;
	char	*decompressed_input;
	size_t	decompressed_len;
	time_t	sum_mtime;
	int		i;
	char	**out, buf[BUFSIZ];

	for (i = 0; sumexts[i] != NULL; i++) { /* try all extensions */
		if (!force_fetch && !force_update)
			sum_mtime = pkg_sum_mtime(cur_repo);
		else
			sum_mtime = 0; /* 0 sumtime == force reload */

		snprintf(buf, BUFSIZ, "%s/%s.%s", cur_repo, PKG_SUMMARY, sumexts[i]);

		if ((file = download_file(buf, &sum_mtime)) != NULL)
			break; /* pkg_summary found and not up-to-date */

		if (sum_mtime < 0) /* pkg_summary found, but up-to-date */
			return NULL;
	}

	if (file == NULL) {
		fprintf(stderr, MSG_COULDNT_FETCH, buf);

		return NULL;
	}

	snprintf(buf, BUFSIZ, UPDATE_REPO_MTIME, (long long)sum_mtime, cur_repo);
	pkgindb_doquery(buf, NULL, NULL);

	if (decompress_buffer(file->buf, file->size, &decompressed_input,
			&decompressed_len)) {

		out = splitstr(decompressed_input, "\n");

		XFREE(file->buf);
		XFREE(file);

		return out;
	}

	XFREE(file->buf);
	XFREE(file);

	return NULL;
}

/* progress percentage */
static void
progress(char c)
{
	const char	*alnum = ALNUM;
	int 		i, alnumlen = strlen(alnum);
	float		percent = 0;

	for (i = 0; i < alnumlen; i++)
		if (c == alnum[i])
			percent = ((float)(i + 1)/ (float)alnumlen) * 100;

	printf(MSG_UPDATING_DB_PCT, (int)percent);
	fflush(stdout);
}

/* check if the field is PKGNAME */
static int
chk_pkgname(char *field)
{
	if (strncmp(field, "PKGNAME=", 8) == 0 ||
		strncmp(field, "CONFLICTS=", 10) == 0)
		return 1;

	return 0;
}

/* returns value for given field */
static char *
field_record(const char *field, char *line)
{
	char *pfield;

	if (strncmp(field, line, strlen(field)) == 0) {
		pfield = strchr(line, '=');
		trimcr(pfield++);

		return pfield;
	}

	return NULL;
}

static void
freecols()
{
	int i;

	for (i = 0; i < cols.num; i++)
		XFREE(cols.name[i]);

	XFREE(cols.name);
}

static void
free_insertlist()
{
	Insertlist *pi;

	while (!SLIST_EMPTY(&inserthead)) {
		pi = SLIST_FIRST(&inserthead);
		SLIST_REMOVE_HEAD(&inserthead, next);
		XFREE(pi->field);
		XFREE(pi->value);
		XFREE(pi);
	}
}

/* sqlite callback, fill cols.name[] with available columns names */
int
colnames(void *unused, int argc, char **argv, char **colname)
{
	int			i = 0;

	colcount++;

	cols.num = colcount;
	XREALLOC(cols.name, colcount * sizeof(char *));

	for (i = 0; i < argc; i++)
		if (argv[i] != NULL && strncmp(colname[i], "name", 4) == 0)
			XSTRDUP(cols.name[colcount - 1], argv[i]);

	return PDB_OK;
}

/* for now, values are located on a SLIST, build INSERT line with them */
static void
prepare_insert(int pkgid, struct Summary sum, char *cur_repo)
{
	char		*commit_query;
	Insertlist	*pi;

	if (sum.type == REMOTE_SUMMARY)
		query_size = (query_size + strlen(cur_repo)) * sizeof(char);

	XMALLOC(commit_query, query_size);

	snprintf(commit_query, BUFSIZ, "INSERT INTO %s ( PKG_ID,", sum.tbl_name);

	/* insert fields */
	SLIST_FOREACH(pi, &inserthead, next)
		XSNPRINTF(commit_query, query_size,
			"%s\"%s\",", commit_query, pi->field);

	/* insert REPOSITORY field */
	if (sum.type == REMOTE_SUMMARY)
		XSNPRINTF(commit_query, query_size, "%s\"REPOSITORY\")", commit_query);
	else
		commit_query[strlen(commit_query) - 1] = ')';

	XSNPRINTF(commit_query, query_size, "%s VALUES ( %d, ",
		commit_query, pkgid);

	/* insert values */
	SLIST_FOREACH(pi, &inserthead, next)
		XSNPRINTF(commit_query, query_size,
			"%s\"%s\",", commit_query, pi->value);

	/* insert repository URL if it's a remote pkg_summary */
	if (sum.type == REMOTE_SUMMARY) {
		XSNPRINTF(commit_query, query_size, "%s\"%s\");",
			commit_query, cur_repo);
	} else {
		commit_query[strlen(commit_query) - 1] = ')';
		strcat(commit_query, ";");
	}

	/* append query to commit list */
	commit_idx++;
	XREALLOC(commit_list, (commit_idx + 1) * sizeof(char *));
	commit_list[commit_idx] = commit_query;
}

static void
child_table(int pkgid, const char *table, char *val)
{
	char buf[BUFSIZ];

	snprintf(buf, BUFSIZ,
		"INSERT INTO %s (PKG_ID,%s_PKGNAME) VALUES (%d,\"%s\");",
		table, table, pkgid, val);

	/* append query to commit_list */
	commit_idx++;
	XREALLOC(commit_list, (commit_idx + 1) * sizeof(char *));
	XSTRDUP(commit_list[commit_idx], buf);
}

static void
update_col(struct Summary sum, int pkgid, char *line)
{
	static uint8_t	said = 0;
	int				i;
	char			*val, *p, buf[BUFSIZ];
	Insertlist		*insert;

	/* check MACHINE_ARCH */
	if (!said && (val = field_record("MACHINE_ARCH", line)) != NULL) {
		if (strncmp(CHECK_MACHINE_ARCH, val, strlen(CHECK_MACHINE_ARCH))) {
			printf(MSG_ARCH_DONT_MATCH, val, CHECK_MACHINE_ARCH);
			if (!check_yesno())
				exit(EXIT_FAILURE);
			said = 1;
			printf("\r"MSG_UPDATING_DB);
		}
	}

	/* DEPENDS */
	if ((val = field_record("DEPENDS", line)) != NULL)
		child_table(pkgid, sum.deps, val);
	/* REQUIRES */
	if ((val = field_record("REQUIRES", line)) != NULL)
		child_table(pkgid, sum.requires, val);
	/* PROVIDES */
	if ((val = field_record("PROVIDES", line)) != NULL)
		child_table(pkgid, sum.provides, val);

	for (i = 0; i < cols.num; i++) {
		snprintf(buf, BUFSIZ, "%s=", cols.name[i]);

		val = field_record(cols.name[i], line);

		/* XXX: handle that later */
		if (strncmp(cols.name[i], "DESCRIPTION", 11) == 0)
			continue;

		if (val != NULL && strncmp(buf, line, strlen(buf)) == 0) {
			/* nasty little hack to prevent double quotes */
			if (strchr(line, '"') != NULL)
				for (p = line; *p != '\0'; p++)
					if (*p == '"')
						*p = '`';


			XMALLOC(insert, sizeof(Insertlist));
			XSTRDUP(insert->field, cols.name[i]);
			XSTRDUP(insert->value, val);

			SLIST_INSERT_HEAD(&inserthead, insert, next);

			/* update query size */
			query_size += strlen(insert->field) + strlen(insert->value) + 5;
			/* 5 = strlen(\"\",) */
		}
	}
}

static void
insert_summary(struct Summary sum, char **summary, char *cur_repo)
{
	int			i;
	static int	pkgid = 1;
	char		*pkgname, **psum, query[BUFSIZ];
	const char	*alnum = ALNUM;
	Insertlist	*insert;

	if (summary == NULL) {
		pkgindb_close();
		errx(EXIT_FAILURE, "could not read summary");
	}

	snprintf(query, BUFSIZ, "PRAGMA table_info(%s);", sum.tbl_name);

	/* record columns names to cols */
	pkgindb_doquery(query, colnames, NULL);

	SLIST_INIT(&inserthead);

	XMALLOC(commit_list, sizeof(char *));
	/* begin transaction */
	XSTRDUP(commit_list[0], "BEGIN;");

	printf(MSG_UPDATING_DB);
	fflush(stdout);

	psum = summary;
	/* main pkg_summary analysis loop */
	while (*psum != NULL) {
		/* CONFLICTS may appear before PKGNAME... */
		if ((pkgname = field_record("CONFLICTS", *psum)) != NULL) {
			snprintf(query, BUFSIZ,
				"INSERT INTO %s (PKG_ID,%s_PKGNAME) VALUES (%d,\"%s\");",
				sum.conflicts, sum.conflicts, pkgid, pkgname);

			/* append query to commit_list */
			commit_idx++;
			XREALLOC(commit_list, (commit_idx + 1) * sizeof(char *));
			XSTRDUP(commit_list[commit_idx], query);

			psum++;
			continue; /* there may be more */
		}

		/* PKGNAME record, should always be true  */
		if ((pkgname = field_record("PKGNAME", *psum)) != NULL) {

			XMALLOC(insert, sizeof(Insertlist));
			XSTRDUP(insert->field, "PKGNAME");
			XSTRDUP(insert->value, pkgname);

			SLIST_INSERT_HEAD(&inserthead, insert, next);

			/* nice little counter */
			progress(pkgname[0]);
		}

		psum++;

		/* browse entries following PKGNAME and build the SQL query */
		while (*psum != NULL && !chk_pkgname(*psum)) {
			update_col(sum, pkgid, *psum);
			psum++;
		}

		/* build INSERT query */
		prepare_insert(pkgid, sum, cur_repo);

		/* next PKG_ID */
		pkgid++;

		/* free the SLIST containing this package's key/vals */
		free_insertlist();

		/* reset max query size */
		query_size = BUFSIZ;

	} /* while *psum != NULL */

	commit_idx++;
	XREALLOC(commit_list, (commit_idx + 2) * sizeof(char *));
	XSTRDUP(commit_list[commit_idx], "COMMIT;");
	commit_list[commit_idx + 1] = NULL;

	/* do the insert */
	for (i = 0; commit_list[i] != NULL; i++)
		pkgindb_doquery(commit_list[i], NULL, NULL);

	progress(alnum[strlen(alnum) - 1]); /* XXX: nasty. */

	free_list(commit_list);
	commit_idx = 0;

	/* reset pkgid */
	if (sum.type == LOCAL_SUMMARY)
		pkgid = 1;

	printf("\n");
}

static void
delete_remote_tbl(struct Summary sum, char *repo)
{
	char	*ptbl, buf[BUFSIZ];
	int		i, nelms;

	/* number of elements in sum */
	nelms = (sizeof(sum) - sizeof(int)) / sizeof(char *) - 1;
	/*
	 * delete repository related tables
	 * loop through sumsw structure to record table name
	 * and call associated SQL query
	 */
	for (ptbl = (char *)sum.tbl_name, i = 0;
		 i < nelms;
		 ptbl += ((strlen(ptbl) + 1) * sizeof(char)), i++) {

		if (strstr(ptbl, "_PKG") != NULL)
			continue;

		snprintf(buf, BUFSIZ, DELETE_REMOTE, ptbl, ptbl, repo, ptbl);
		pkgindb_doquery(buf, NULL, NULL);
	}

	snprintf(buf, BUFSIZ,
		"DELETE FROM REMOTE_PKG WHERE REPOSITORY = '%s';", repo);
	pkgindb_doquery(buf, NULL, NULL);
}

static int
pdb_clean_remote(void *param, int argc, char **argv, char **colname)
{
	int		i, repolen;
	char	**repos = pkg_repos, query[BUFSIZ];

	if (argv == NULL)
		return PDB_ERR;

	for (i = 0; repos[i] != NULL; i++) {
		repolen = strlen(repos[i]);
		if (repolen == strlen(argv[0]) &&
			strncmp(repos[i], argv[0], repolen) == 0 && !force_update)
		   	return PDB_OK;
	}
	/* did not find argv[0] (db repository) in pkg_repos */
	printf(MSG_CLEANING_DB_FROM_REPO, argv[0]);

	delete_remote_tbl(sumsw[1], argv[0]);

	snprintf(query, BUFSIZ,
		"DELETE FROM REPOS WHERE REPO_URL = \'%s\';", argv[0]);
	pkgindb_doquery(query, NULL, NULL);

	/* force pkg_summary reload for available repository */
	force_fetch = 1;

	return PDB_OK;
}

void
update_db(int which, char **pkgkeep)
{
	int			i;
	Plisthead	*keeplisthead, *nokeeplisthead, *plisthead;
	Pkglist		*pkglist;
	char		**summary = NULL, **prepos, buf[BUFSIZ];

	for (i = 0; i < 2; i++) {

		switch (sumsw[i].type) {
		case LOCAL_SUMMARY:
			/* has the pkgdb changed ? if not, continue */
			if (!pkg_db_mtime() || !pkgdb_open(ReadWrite))
				continue;

			/* just checking */
			pkgdb_close();

			/* record the keep list */
			keeplisthead = rec_pkglist(KEEP_LOCAL_PKGS);
			/* delete local pkg table (faster than updating) */
			pkgindb_doquery(DELETE_LOCAL, NULL, NULL);

			/* generate summary locally */
			summary = exec_list(PKGTOOLS "/pkg_info -Xa", NULL);

			printf(MSG_PROCESSING_LOCAL_SUMMARY);

			insert_summary(sumsw[i], summary, NULL);
			free_list(summary);

			/* restore keep-list */
			if (keeplisthead != NULL) {
				SLIST_FOREACH(pkglist, keeplisthead, next) {
					snprintf(buf, BUFSIZ, KEEP_PKG, pkglist->pkgname);
					pkgindb_doquery(buf, NULL, NULL);
				}
				free_pkglist(keeplisthead);

				/*
				 * packages are installed "manually" by pkgin_install()
				 * they are recorded as "non-automatic" in pkgdb, we
				 * need to mark unkeeps as "automatic"
				 */
				if ((nokeeplisthead =
						rec_pkglist(NOKEEP_LOCAL_PKGS)) != NULL) {
					SLIST_FOREACH(pkglist, nokeeplisthead, next)
						mark_as_automatic_installed(pkglist->pkgname, 1);

					free_pkglist(nokeeplisthead);
				}
			} else { /* empty keep list */
				/*
				 * no packages are marked as keep in pkgin's db
				 * probably a fresh install or a rebuild
				 * restore keep flags with pkgdb informations
				 */
				if ((plisthead = rec_pkglist(LOCAL_PKGS_QUERY)) != NULL) {
					SLIST_FOREACH(pkglist, plisthead, next)
						if (!is_automatic_installed(pkglist->pkgname)) {
							snprintf(buf, BUFSIZ, KEEP_PKG, pkglist->pkgname);
							pkgindb_doquery(buf, NULL, NULL);
						}

					free_pkglist(plisthead);
				}
			}

			/* insert new keep list if there's any */
			if (pkgkeep != NULL)
				/* installation: mark the packages as "keep" */
				pkg_keep(KEEP, pkgkeep);

			break;
		case REMOTE_SUMMARY:
			if (which == LOCAL_SUMMARY)
				continue;

			/* delete unused repositories */
			pkgindb_doquery("SELECT REPO_URL FROM REPOS;",
				pdb_clean_remote, NULL);

			/* loop through PKG_REPOS */
			for (prepos = pkg_repos; *prepos != NULL; prepos++) {

				/* load remote pkg_summary */
				if ((summary = fetch_summary(*prepos)) == NULL) {
					printf(MSG_DB_IS_UP_TO_DATE, *prepos);
					continue;
				}

				printf(MSG_PROCESSING_REMOTE_SUMMARY, *prepos);

				/* delete remote* associated to this repository */
				delete_remote_tbl(sumsw[i], *prepos);
				/* update remote* table for this repository */
				insert_summary(sumsw[i], summary, *prepos);
				free_list(summary);
			}

			/* remove empty rows (duplicates) */
			pkgindb_doquery(DELETE_EMPTY_ROWS, NULL, NULL);

			break;
		}

	} /* for sumsw */
	/* columns name not needed anymore */
	if (cols.name != NULL) {
		/* reset colums count */
		colcount = 0;
		freecols();
	}

}
