/* $Id: summary.c,v 1.33 2012/07/15 17:36:34 imilh Exp $ */

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

/**
 * Import pkg_summary to SQLite database
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
	[LOCAL_SUMMARY] = {
		LOCAL_SUMMARY,
		"LOCAL_PKG",
		"LOCAL_DEPS",
		"LOCAL_CONFLICTS",
		"LOCAL_REQUIRES",
		"LOCAL_PROVIDES",
		NULL
	},
	[REMOTE_SUMMARY] = {
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
	int	num;
	char	**name;
} cols;

typedef struct Insertlist {
	char	*field;
	char	*value;
	SLIST_ENTRY(Insertlist) next;
} Insertlist;

SLIST_HEAD(, Insertlist) inserthead;

static char		**fetch_summary(char *url);
static void		freecols(void);
static void		free_insertlist(void);
static void		prepare_insert(int, struct Summary, char *);
int			colnames(void *, int, char **, char **);

char			*env_repos, **pkg_repos;
char			**commit_list = NULL;
int			commit_idx = 0;
int			query_size = BUFSIZ;
/* column count for table fields, given by colnames callback */
int			colcount = 0;
/* force pkg_summary reload */
int			force_fetch = 0;

static const char *const sumexts[] = { "bz2", "gz", NULL };

/**
 * remote summary fetch
 */
static char **
fetch_summary(char *cur_repo)
{
	/* from pkg_install/files/admin/audit.c */
	Dlfile	*file = NULL;
	char	*decompressed_input;
	size_t	decompressed_len;
	time_t	sum_mtime;
	int	i;
	char	**out, buf[BUFSIZ];

	for (i = 0; sumexts[i] != NULL; i++) { /* try all extensions */
		if (!force_fetch && !force_update)
			sum_mtime = pkg_sum_mtime(cur_repo);
		else
			sum_mtime = 0; /* 0 sumtime == force reload */

		snprintf(buf, BUFSIZ, "%s/%s.%s",
				cur_repo, PKG_SUMMARY, sumexts[i]);

		if ((file = download_file(buf, &sum_mtime)) != NULL)
			break; /* pkg_summary found and not up-to-date */

		if (sum_mtime < 0) /* pkg_summary found, but up-to-date */
			return NULL;
	}

	if (file == NULL)
		errx(EXIT_FAILURE, MSG_COULDNT_FETCH, buf);

	snprintf(buf, BUFSIZ,
			UPDATE_REPO_MTIME, (long long)sum_mtime, cur_repo);
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

/**
 * progress percentage
 */
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

/**
 * check if the field is PKGNAME
 */
static int
chk_pkgname(char *field, char *last_field)
{
	if (strncmp(field, "PKGNAME=", 8) == 0)
		return 1;
	/* in some very rare cases, CONFLICTS appears *after* PKGNAME */
	if (strncmp(last_field, "PKGNAME=", 8) != 0 &&
		/* never seen many CONFLICTS after PKGNAME, just in case... */
		strncmp(last_field, "CONFLICTS=", 10) != 0 &&
		strncmp(field, "CONFLICTS=", 10) == 0)
		return 1;

	return 0;
}

/**
 * returns value for given field
 */
static char *
field_record(const char *field, char *line)
{
	char *pfield;

	if (strncmp(field, line, strlen(field)) == 0) {
		if ((pfield = strchr(line, '=')) == NULL)
			return NULL;
		trimcr(pfield++);

		/* weird buggy packages with empty fields, like LICENSE= */
		if (*pfield != '\0')
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

/**
 * sqlite callback, fill cols.name[] with available columns names
 */
int
colnames(void *unused, int argc, char **argv, char **colname)
{
	int i = 0;

	colcount++;

	cols.num = colcount;
	XREALLOC(cols.name, colcount * sizeof(char *));

	for (i = 0; i < argc; i++)
		if (argv[i] != NULL && strncmp(colname[i], "name", 4) == 0)
			XSTRDUP(cols.name[colcount - 1], argv[i]);

	return PDB_OK;
}

/**
 * for now, values are located on a SLIST, build INSERT line with them
 */
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
		XSNPRINTF(commit_query, query_size, "%s\"REPOSITORY\")",
				commit_query);
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

/**
 * add item to the main SLIST
 */
static void
add_to_slist(const char *field, const char *value)
{
	Insertlist	*insert;

	XMALLOC(insert, sizeof(Insertlist));
	XSTRDUP(insert->field, field);
	XSTRDUP(insert->value, value);

	SLIST_INSERT_HEAD(&inserthead, insert, next);
}

/**
 * fill-in secondary tables
 */
static void
child_table(const char *fmt, ...)
{
	char	buf[BUFSIZ];
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(buf, BUFSIZ, fmt, ap);
	va_end(ap);

	/* append query to commit_list */
	commit_idx++;
	XREALLOC(commit_list, (commit_idx + 1) * sizeof(char *));
	XSTRDUP(commit_list[commit_idx], buf);
}

static void
update_col(struct Summary sum, int pkgid, char *line)
{
	static uint8_t	said = 0;
	int		i;
	char		*val, *p, buf[BUFSIZ];

	/* check MACHINE_ARCH */
	if (!said && (val = field_record("MACHINE_ARCH", line)) != NULL) {
		if (strncmp(CHECK_MACHINE_ARCH,
				val, strlen(CHECK_MACHINE_ARCH))) {
			printf(MSG_ARCH_DONT_MATCH, val, CHECK_MACHINE_ARCH);
			if (!check_yesno(DEFAULT_NO))
				exit(EXIT_FAILURE);
			said = 1;
			printf("\r"MSG_UPDATING_DB);
		}
	}

	/* DEPENDS */
	if ((val = field_record("DEPENDS", line)) != NULL) {
		if ((p = get_pkgname_from_depend(val)) != NULL) {
			child_table(INSERT_DEPENDS_VALUES,
				sum.deps, sum.deps, sum.deps,
				pkgid, p, val);
			XFREE(p);
		} else
			printf(MSG_COULD_NOT_GET_PKGNAME, val);
	}
	/* REQUIRES */
	if ((val = field_record("REQUIRES", line)) != NULL)
		child_table(INSERT_SINGLE_VALUE,		\
			sum.requires, sum.requires, pkgid, val);
	/* PROVIDES */
	if ((val = field_record("PROVIDES", line)) != NULL)
		child_table(INSERT_SINGLE_VALUE,		\
			sum.provides, sum.provides, pkgid, val);

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

			add_to_slist(cols.name[i], val);

			/* update query size */
			query_size += strlen(cols.name[i]) + strlen(val) + 5;
			/* 5 = strlen(\"\",) */
		}
	}
}

/* default version for (rare and buggy) packages with a version */
#define NOVERSION "-0.0"

static void
insert_summary(struct Summary sum, char **summary, char *cur_repo)
{
	int		i;
	static int	pkgid = 1;
	char		*pkgname, *pkgvers, **psum;
	char		query[BUFSIZ], tmpname[BUFSIZ];
	const char	*alnum = ALNUM;

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

	if (!parsable) {
		printf(MSG_UPDATING_DB);
		fflush(stdout);
	}

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

			/* some rare packages have no version */
			if (!exact_pkgfmt(pkgname)) {
				snprintf(	tmpname, BUFSIZ,
						"%s%s", pkgname, NOVERSION);
				pkgname = tmpname;
			}

			add_to_slist("FULLPKGNAME", pkgname);

			/* split PKGNAME and VERSION */
			pkgvers = strrchr(pkgname, '-');
			if (pkgvers != NULL)
				*pkgvers++ = '\0';

			add_to_slist("PKGNAME", pkgname);
			add_to_slist("PKGVERS", pkgvers);

			/* nice little counter */
			if (!parsable)
				progress(pkgname[0]);
		}

		psum++;

		/* browse entries following PKGNAME and build the SQL query */
		while (*psum != NULL && !chk_pkgname(*psum, *(psum - 1))) {
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

	if (!parsable)
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
	char		buf[BUFSIZ];
	const char	**arr;

	/*
	 * delete repository related tables
	 * loop through sumsw structure to record table name
	 * and call associated SQL query
	 */
	/* (REMOTE[LOCAL)_PKG is first -> skip */
	for (arr = &(sum.tbl_name) + 1; *arr != NULL; ++arr) {
		snprintf(buf, BUFSIZ, DELETE_REMOTE,
			*arr, *arr, *arr, *arr, *arr, repo, *arr);
		pkgindb_doquery(buf, NULL, NULL);
	}

	snprintf(buf, BUFSIZ,
		"DELETE FROM REMOTE_PKG WHERE REPOSITORY = '%s';", repo);
	pkgindb_doquery(buf, NULL, NULL);
}

static void
handle_manually_installed(Pkglist *pkglist, char **pkgkeep)
{
	char buf[BUFSIZ];

	/* update local db with manually installed packages */
	if (pkgkeep == NULL && !is_automatic_installed(pkglist->full)) {
		snprintf(buf, BUFSIZ, KEEP_PKG, pkglist->name);
		pkgindb_doquery(buf, NULL, NULL);
		return;
	}
	mark_as_automatic_installed(pkglist->full, 1);
}

static void
update_localdb(char **pkgkeep)
{
	char		**summary = NULL, buf[BUFSIZ];
	Plistnumbered	*keeplisthead, *nokeeplisthead;
	Pkglist		*pkglist;

	/* has the pkgdb (pkgsrc) changed ? if not, continue */
	if (!pkg_db_mtime() || !pkgdb_open(ReadWrite))
		return;

	/* just checking */
	pkgdb_close();

	/* record the keep list */
	keeplisthead = rec_pkglist(KEEP_LOCAL_PKGS);
	/* delete local pkg table (faster than updating) */
	pkgindb_doquery(DELETE_LOCAL, NULL, NULL);

	printf(MSG_READING_LOCAL_SUMMARY);
	/* generate summary locally */
	summary = exec_list(PKGTOOLS "/pkg_info -Xa", NULL);

	printf(MSG_PROCESSING_LOCAL_SUMMARY);

	/* insert the summary to the database */
	insert_summary(sumsw[LOCAL_SUMMARY], summary, NULL);

	/* re-read local packages list as it may have changed */
	free_global_pkglists();
	init_global_pkglists();

	/* restore keep-list */
	if (keeplisthead != NULL) {
		SLIST_FOREACH(pkglist, keeplisthead->P_Plisthead, next) {
			snprintf(buf, BUFSIZ, KEEP_PKG, pkglist->name);
			pkgindb_doquery(buf, NULL, NULL);
		}
		free_pkglist(&keeplisthead->P_Plisthead, LIST);
		free(keeplisthead);

		/*
		 * packages are installed "manually" by pkgin_install()
		 * they are recorded as "non-automatic" in pkgdb, we
		 * need to mark unkeeps as "automatic".
		 */
		if ((nokeeplisthead = rec_pkglist(NOKEEP_LOCAL_PKGS)) != NULL) {
			SLIST_FOREACH(	pkglist,
					nokeeplisthead->P_Plisthead,
					next) {
				handle_manually_installed(pkglist, pkgkeep);
			}

			free_pkglist(&nokeeplisthead->P_Plisthead, LIST);
			free(nokeeplisthead);
		}
	} else { /* empty keep list */
		/*
		 * no packages are marked as keep in pkgin's db
		 * probably a fresh install or a rebuild
		 * restore keep flags with pkgdb informations
		 */
		SLIST_FOREACH(pkglist, &l_plisthead, next) {
			if (!is_automatic_installed(pkglist->full)) {
				snprintf(buf, BUFSIZ, KEEP_PKG, pkglist->name);
				pkgindb_doquery(buf, NULL, NULL);
			}
		}
	}

	/* insert new keep list if there's any */
	if (pkgkeep != NULL)
		/* installation: mark the packages as "keep" */
		pkg_keep(KEEP, pkgkeep);

	free_list(summary);
}

static int
pdb_clean_remote(void *param, int argc, char **argv, char **colname)
{
	int	i;
	size_t	repolen;
	char	**repos = pkg_repos, query[BUFSIZ];

	if (argv == NULL)
		return PDB_ERR;

	for (i = 0; repos[i] != NULL; i++) {
		repolen = strlen(repos[i]);
		if (repolen == strlen(argv[0]) &&
			strncmp(repos[i], argv[0], repolen) == 0 &&
					!force_update)
		   	return PDB_OK;
	}
	/* did not find argv[0] (db repository) in pkg_repos */
	printf(MSG_CLEANING_DB_FROM_REPO, argv[0]);

	delete_remote_tbl(sumsw[REMOTE_SUMMARY], argv[0]);

	snprintf(query, BUFSIZ,
		"DELETE FROM REPOS WHERE REPO_URL = \'%s\';", argv[0]);
	pkgindb_doquery(query, NULL, NULL);

	/* force pkg_summary reload for available repository */
	force_fetch = 1;

	return PDB_OK;
}

static void
update_remotedb(void)
{
	char	**summary = NULL, **prepos;
	uint8_t	cleaned = 0;

	/* loop through PKG_REPOS */
	for (prepos = pkg_repos; *prepos != NULL; prepos++) {

		/* load remote pkg_summary */
		if ((summary = fetch_summary(*prepos)) == NULL) {
			printf(MSG_DB_IS_UP_TO_DATE, *prepos);
			continue;
		}

		/*
		 * do not cleanup repos before being sure new repo is reachable
		 */
		if (!cleaned) {
			/* delete unused repositories */
			pkgindb_doquery("SELECT REPO_URL FROM REPOS;",
				pdb_clean_remote, NULL);
			cleaned = 1;
		}

		printf(MSG_PROCESSING_REMOTE_SUMMARY, *prepos);

		/* delete remote* associated to this repository */
		delete_remote_tbl(sumsw[REMOTE_SUMMARY], *prepos);
		/* update remote* table for this repository */
		insert_summary(sumsw[REMOTE_SUMMARY], summary, *prepos);

		free_list(summary);
	}

	/* remove empty rows (duplicates) */
	pkgindb_doquery(DELETE_EMPTY_ROWS, NULL, NULL);
}

int
update_db(int which, char **pkgkeep)
{
	if (!have_enough_rights())
		return EXIT_FAILURE;

	/* always check for LOCAL_SUMMARY updates */
	update_localdb(pkgkeep);

	if (which == REMOTE_SUMMARY)
		update_remotedb();

	/* columns name not needed anymore */
	if (cols.name != NULL) {
		/* reset colums count */
		colcount = 0;
		freecols();
	}

	return EXIT_SUCCESS;
}

void
split_repos(void)
{
	int	repocount;
	char	*p;

	XSTRDUP(env_repos, getenv("PKG_REPOS"));

	if (env_repos == NULL)
		if ((env_repos = read_repos()) == NULL)
			errx(EXIT_FAILURE, MSG_MISSING_PKG_REPOS);

	repocount = 2; /* 1st elm + NULL */

	XMALLOC(pkg_repos, repocount * sizeof(char *));
	*pkg_repos = env_repos;

	p = env_repos;

	while((p = strchr(p, ' ')) != NULL) {
		*p = '\0';
		p++;

		XREALLOC(pkg_repos, ++repocount * sizeof(char *));
		pkg_repos[repocount - 2] = p;
	}

	/* NULL last element */
	pkg_repos[repocount - 1] = NULL;

	repo_record(pkg_repos);
}

/* check if repositories listed in REPO_URL table are still relevant */
static int
cmp_repo_list(void *param, int argc, char **argv, char **colname)
{
	int	i, j, match;
	char	query[BUFSIZ];

	if (argv == NULL)
		return PDB_ERR; /* no repo yet? */

	for (i = 0; i < argc; i++) {
		match = 0;
		for (j = 0; pkg_repos[j] != NULL; j++)
			if (strcmp(argv[i], pkg_repos[j]) == 0)
				match = 1;
		if (match == 0) {
			printf(MSG_CLEANING_DB_FROM_REPO, argv[i]);
			snprintf(query, BUFSIZ,
			"DELETE FROM REPOS WHERE REPO_URL = '%s';", argv[i]);
			pkgindb_doquery(query, NULL, NULL);
			snprintf(query, BUFSIZ,
			"DELETE FROM REMOTE_PKG WHERE REPOSITORY = '%s';",
			argv[i]);
			pkgindb_doquery(query, NULL, NULL);

			force_fetch = 1;
		}
	}

	return PDB_OK;
}

void
chk_repo_list()
{
	pkgindb_doquery("SELECT REPO_URL FROM REPOS;", cmp_repo_list, NULL);
}
