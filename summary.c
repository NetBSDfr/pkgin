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

static struct archive	*fetch_summary(char *url);
static void		freecols(void);
static void		free_insertlist(void);
static void		insert_local_summary(FILE *);
static void		insert_remote_summary(struct archive *, char *);
static void		delete_remote_tbl(struct Summary, char *);
static void		prepare_insert(int, struct Summary);
int			colnames(void *, int, char **, char **);

char			*env_repos, **pkg_repos;
/* column count for table fields, given by colnames callback */
int			colcount = 0;
/* force pkg_summary reload */
int			force_fetch = 0;

static const char *const sumexts[] = { "xz", "bz2", "gz", NULL };

/*
 * Open a remote summary and return an open libarchive handler to it.
 */
static struct archive *
fetch_summary(char *cur_repo)
{
	struct	archive *a;
	struct	archive_entry *ae;
	Sumfile	*sum = NULL;
	time_t	sum_mtime;
	int	i;
	char	buf[BUFSIZ];

	for (i = 0; sumexts[i] != NULL; i++) { /* try all extensions */
		if (!force_fetch && !force_update)
			sum_mtime = pkg_sum_mtime(cur_repo);
		else
			sum_mtime = 0; /* 0 sumtime == force reload */

		snprintf(buf, BUFSIZ, "%s/%s.%s",
				cur_repo, PKG_SUMMARY, sumexts[i]);

		if ((sum = sum_open(buf, &sum_mtime)) != NULL)
			break; /* pkg_summary found and not up-to-date */

		if (sum_mtime < 0) /* pkg_summary found, but up-to-date */
			return NULL;
	}

	if (sum == NULL)
		errx(EXIT_FAILURE, MSG_COULDNT_FETCH, buf);

	pkgindb_dovaquery(UPDATE_REPO_MTIME, (long long)sum_mtime, cur_repo);

	if ((a = archive_read_new()) == NULL)
		errx(EXIT_FAILURE, "Cannot initialise archive");

#if ARCHIVE_VERSION_NUMBER < 3000000
	if (archive_read_support_compression_all(a) != ARCHIVE_OK ||
#else
	if (archive_read_support_filter_all(a) != ARCHIVE_OK ||
#endif
	    archive_read_support_format_raw(a) != ARCHIVE_OK ||
	    archive_read_open(a, sum, sum_start, sum_read, sum_close) != ARCHIVE_OK)
		errx(EXIT_FAILURE, "Cannot open pkg_summary: %s",
		    archive_error_string(a));

        if (archive_read_next_header(a, &ae) != ARCHIVE_OK)
		errx(EXIT_FAILURE, "Cannot read pkg_summary: %s",
		    archive_error_string(a));

	return a;
}

static void
freecols(void)
{
	int i;

	for (i = 0; i < cols.num; i++)
		XFREE(cols.name[i]);

	XFREE(cols.name);
}

static void
free_insertlist(void)
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
	cols.name = xrealloc(cols.name, (size_t)colcount * sizeof(char *));

	for (i = 0; i < argc; i++)
		if (argv[i] != NULL && strncmp(colname[i], "name", 4) == 0)
			cols.name[colcount - 1] = xstrdup(argv[i]);

	return PDB_OK;
}

/**
 * for now, values are located on a SLIST, build INSERT line with them
 */
static void
prepare_insert(int pkgid, struct Summary sum)
{
	Insertlist	*pi;
	/*
	 * Currently INSERT lengths are under 1K, this should be plenty until
	 * we support more columns.
	 */
	char		querybuf[4096];
	char		tmpbuf[1024];

	snprintf(querybuf, sizeof(querybuf), "INSERT INTO %s (PKG_ID", sum.tbl_name);

	/* insert fields */
	SLIST_FOREACH(pi, &inserthead, next) {
		snprintf(tmpbuf, sizeof(tmpbuf), ",\"%s\"", pi->field);
		if (strlcat(querybuf, tmpbuf, sizeof(querybuf)) >= sizeof(querybuf))
			goto err;
	}

	snprintf(tmpbuf, sizeof(tmpbuf), ") VALUES (%d", pkgid);
	if (strlcat(querybuf, tmpbuf, sizeof(querybuf)) >= sizeof(querybuf))
		goto err;

	/* insert values */
	SLIST_FOREACH(pi, &inserthead, next) {
		snprintf(tmpbuf, sizeof(tmpbuf), ",\"%s\"", pi->value);
		if (strlcat(querybuf, tmpbuf, sizeof(querybuf)) >= sizeof(querybuf))
			goto err;
	}

	snprintf(tmpbuf, sizeof(tmpbuf), ");");
	if (strlcat(querybuf, tmpbuf, sizeof(querybuf)) >= sizeof(querybuf))
		goto err;

	/* Apply the query */
	pkgindb_doquery(querybuf, NULL, NULL);

	return;
err:
	errx(EXIT_FAILURE, "Increase query buffer");
}

/**
 * add item to the main SLIST
 */
static void
add_to_slist(const char *field, const char *value)
{
	Insertlist	*insert;

	insert = xmalloc(sizeof(Insertlist));
	insert->field = xstrdup(field);
	insert->value = xstrdup(value);

	SLIST_INSERT_HEAD(&inserthead, insert, next);
}

/*
 * Parse a KEY=value line.
 */
static void
parse_entry(struct Summary sum, int pkgid, char *line)
{
	static uint8_t	check_machine_arch = 1;
	int		i;
	char		*val, *v, *pkg, buf[BUFSIZ];

	if ((val = strchr(line, '=')) == NULL)
		errx(EXIT_FAILURE, "Invalid pkg_info entry: %s", line);

	val++;

	/*
	 * Check MACHINE_ARCH of the package matches the local machine.
	 */
	if (check_machine_arch && strncmp(line, "MACHINE_ARCH=", 13) == 0) {
		if (strncmp(CHECK_MACHINE_ARCH, val,
		    strlen(CHECK_MACHINE_ARCH))) {
			alarm(0); /* Stop the progress meter */
			printf(MSG_ARCH_DONT_MATCH, val, CHECK_MACHINE_ARCH);
			if (!check_yesno(DEFAULT_NO))
				exit(EXIT_FAILURE);
			check_machine_arch = 0;
			alarm(1); /* Restart progress XXX: UPDATE_INTERVAL */
		}
		return;
	}

	/* CONFLICTS */
	if (strncmp(line, "CONFLICTS=", 10) == 0) {
		pkgindb_dovaquery(INSERT_SINGLE_VALUE, sum.conflicts,
		    sum.conflicts, pkgid, val);
		return;
	}

	/* DEPENDS */
	if (strncmp(line, "DEPENDS=", 8) == 0) {
		if ((pkg = get_pkgname_from_depend(val)) != NULL) {
			pkgindb_dovaquery(INSERT_DEPENDS_VALUES, sum.deps,
			    sum.deps, sum.deps, pkgid, pkg, val);
			XFREE(pkg);
		} else
			printf(MSG_COULD_NOT_GET_PKGNAME, val);
		return;
	}

	/* REQUIRES */
	if (strncmp(line, "REQUIRES=", 9) == 0) {
		pkgindb_dovaquery(INSERT_SINGLE_VALUE, sum.requires,
		    sum.requires, pkgid, val);
		return;
	}

	/* PROVIDES */
	if (strncmp(line, "PROVIDES=", 9) == 0) {
		pkgindb_dovaquery(INSERT_SINGLE_VALUE, sum.provides,
		    sum.provides, pkgid, val);
		return;
	}

	/*
	 * Currently we ignore DESCRIPTION entries as they are multi-line
	 * which aren't supported.
	 */
	if (strncmp(line, "DESCRIPTION=", 12) == 0)
		return;

	/*
	 * Skip empty values like LICENSE=
	 */
	if (*val == '\0')
		return;

	/*
	 * Handle remaining columns.
	 */
	for (i = 0; i < cols.num; i++) {
		snprintf(buf, BUFSIZ, "%s=", cols.name[i]);

		if (strncmp(buf, line, strlen(buf)) == 0) {
			/*
			 * Avoid double quotes in our query by using
			 * the MySQL-compatible "`" instead.
			 */
			if (strchr(val, '"') != NULL)
				for (v = val; *v != '\0'; v++)
					if (*v == '"')
						*v = '`';

			/* Split PKGNAME into parts */
			if (strncmp(cols.name[i], "PKGNAME", 7) == 0) {
				/* some rare packages have no version */
				if (!exact_pkgfmt(val)) {
					snprintf(buf, BUFSIZ, "%s%s", val,
					    "-0.0");
					val = buf;
				}
				add_to_slist("FULLPKGNAME", val);

				/* split PKGNAME and VERSION */
				v = strrchr(val, '-');
				if (v != NULL)
					*v++ = '\0';
				add_to_slist("PKGNAME", val);
				add_to_slist("PKGVERS", v);
			} else
				add_to_slist(cols.name[i], val);

			break;
		}
	}
}

/*
 * Stream the local pkg_info information into the local summary.
 */
static void
insert_local_summary(FILE *fp)
{
	static int	pkgid = 1;
	char		buf[BUFSIZ];

	if (fp == NULL) {
		pkgindb_close();
		errx(EXIT_FAILURE, "Couldn't read local pkg_info");
	}

	/* record columns names to cols */
	snprintf(buf, BUFSIZ, "PRAGMA table_info(%s);",
			sumsw[LOCAL_SUMMARY].tbl_name);
	pkgindb_doquery(buf, colnames, NULL);

	SLIST_INIT(&inserthead);

        pkgindb_doquery("BEGIN;", NULL, NULL);

	while (fgets(buf, BUFSIZ, fp) != NULL) {
		/*
		 * End of current package entry, commit and reset.
		 */
		if (*buf == '\n') {
			prepare_insert(pkgid++, sumsw[LOCAL_SUMMARY]);
			free_insertlist();
			continue;
		}
		trimcr(buf);
		parse_entry(sumsw[LOCAL_SUMMARY], pkgid, buf);
	}

        pkgindb_doquery("COMMIT;", NULL, NULL);
}

/*
 * Stream a remote pkg_summary via libarchive into the local database.
 */
static void
insert_remote_summary(struct archive *a, char *cur_repo)
{
	static int	pkgid = 1;
	size_t		buflen, offset;
	ssize_t		r;
	char		*buf, *pe, *pi, *npi;

	if (a == NULL) {
		pkgindb_close();
		errx(EXIT_FAILURE, "Couldn't read pkg_summary");
	}

	/*
	 * Initial archive buffer, we grow if required.  Try to hit the
	 * sweet spot between memory usage and CPU time required to move
	 * the buffer back to the beginning each time.
	 */
	buflen = 32768;
	buf = xmalloc(buflen + 1);

	/* record columns names to cols */
	snprintf(buf, buflen, "PRAGMA table_info(%s);",
	    sumsw[REMOTE_SUMMARY].tbl_name);
	pkgindb_doquery(buf, colnames, NULL);

	SLIST_INIT(&inserthead);

	pkgindb_doquery("BEGIN;", NULL, NULL);

	/*
	 * Main loop.  Read in archive, split into package records and parse
	 * each entry, then insert packge.  If we are in the middle of a
	 * package, copy it to the beginning and read the remainder.
	 */
	offset = 0;
	for (;;) {
		r = archive_read_data(a, buf + offset, buflen - offset);

		/* We're done with reading from the archive. */
		if (r <= 0)
			break;

		pi = buf;
		buf[(size_t)r + offset] = '\0';

		/*
		 * Highly unlikely, but if we can't fit a single pkg_info entry
		 * into our reasonably sized buffer, then we have no choice but
		 * to increase the buffer size.
		 */
		if (strstr(pi, "\n\n") == NULL) {
			offset = buflen;
			buflen *= 2;
			buf = xrealloc(buf, buflen + 1);
			continue;
		}

		/*
		 * Packages are delimited by an empty line, we split the buffer
		 * by package with pi pointing to the beginning and npi the end.
		 */
		for (;;) {
			/*
			 * No remaining complete package entries, move the
			 * leftover to the beginning and start again.
			 */
			if ((npi = strstr(pi, "\n\n")) == NULL) {
				offset = strlen(pi);
				memmove(buf, pi, offset + 1);
				break;
			}

			*npi = '\0';
			npi += 2;

			/*
			 * Handle each KEY=value pkg_info entry.
			 */
			while ((pe = strsep(&pi, "\n")) != NULL)
				parse_entry(sumsw[REMOTE_SUMMARY], pkgid, pe);

			/* Add REPOSITORY information */
			add_to_slist("REPOSITORY", cur_repo);

			/*
			 * At this point we should have a fully populated slist
			 * and all the data we need to construct the INSERT.
			 */
			prepare_insert(pkgid++, sumsw[REMOTE_SUMMARY]);

			/* Set up for next pkg_info */
			free_insertlist();
			pi = npi;
		}
	}

	XFREE(buf);

	pkgindb_doquery("COMMIT;", NULL, NULL);

	if (r != ARCHIVE_OK) {
		delete_remote_tbl(sumsw[REMOTE_SUMMARY], cur_repo);
		errx(EXIT_FAILURE, "Short read of pkg_summary: %s",
		    archive_error_string(a));
	}

#if ARCHIVE_VERSION_NUMBER < 3000000
	archive_read_finish(a);
#else
	archive_read_free(a);
#endif
}

static void
delete_remote_tbl(struct Summary sum, char *repo)
{
	const char	**arr;

	/*
	 * delete repository related tables
	 * loop through sumsw structure to record table name
	 * and call associated SQL query
	 */
	/* (REMOTE[LOCAL)_PKG is first -> skip */
	for (arr = &(sum.tbl_name) + 1; *arr != NULL; ++arr) {
		pkgindb_dovaquery(DELETE_REMOTE, *arr, *arr, *arr, *arr, *arr,
		    repo, *arr);
	}

	pkgindb_dovaquery(DELETE_REMOTE_PKG_REPO, repo);
}

static void
handle_manually_installed(Pkglist *pkglist, char **pkgkeep)
{
	/* update local db with manually installed packages */
	if (pkgkeep == NULL && !is_automatic_installed(pkglist->full)) {
		pkgindb_dovaquery(KEEP_PKG, pkglist->name);
		return;
	}
	mark_as_automatic_installed(pkglist->full, 1);
}

static void
update_localdb(char **pkgkeep)
{
	FILE		*pinfo;
	Plistnumbered	*keeplisthead, *nokeeplisthead;
	Pkglist		*pkglist;

	/* has the pkgdb (pkgsrc) changed ? if not, continue */
	if (!pkg_db_mtime())
		return;

	/* record the keep list */
	keeplisthead = rec_pkglist(KEEP_LOCAL_PKGS);
	/* delete local pkg table (faster than updating) */
	pkgindb_doquery(DELETE_LOCAL, NULL, NULL);

	printf(MSG_READING_LOCAL_SUMMARY);
	/* generate summary locally */
	if ((pinfo = popen(PKGTOOLS "/pkg_info -Xa", "r")) == NULL)
		errx(EXIT_FAILURE, "Couldn't run pkg_info");

	printf(MSG_PROCESSING_LOCAL_SUMMARY);

	/* insert the summary to the database */
	insert_local_summary(pinfo);
	pclose(pinfo);

	/* re-read local packages list as it may have changed */
	free_global_pkglists();
	init_global_pkglists();

	/* restore keep-list */
	if (keeplisthead != NULL) {
		SLIST_FOREACH(pkglist, keeplisthead->P_Plisthead, next) {
			pkgindb_dovaquery(KEEP_PKG, pkglist->name);
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
				pkgindb_dovaquery(KEEP_PKG, pkglist->name);
			}
		}
	}

	/* insert new keep list if there's any */
	if (pkgkeep != NULL)
		/* installation: mark the packages as "keep" */
		pkg_keep(KEEP, pkgkeep);
}

static int
pdb_clean_remote(void *param, int argc, char **argv, char **colname)
{
	int	i;
	size_t	repolen;
	char	**repos = pkg_repos;

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

	pkgindb_dovaquery(DELETE_REPO_URL, argv[0]);

	/* force pkg_summary reload for available repository */
	force_fetch = 1;

	return PDB_OK;
}

static void
update_remotedb(int verbose)
{
	struct archive	*a;
	char		**prepos;
	uint8_t		cleaned = 0;

	/* loop through PKG_REPOS */
	for (prepos = pkg_repos; *prepos != NULL; prepos++) {

		if (verbose)
			printf(MSG_PROCESSING_REMOTE_SUMMARY, *prepos);

		/* load remote pkg_summary */
		if ((a = fetch_summary(*prepos)) == NULL) {
			if (verbose)
				printf(MSG_DB_IS_UP_TO_DATE, *prepos);
			continue;
		}

		/*
		 * do not cleanup repos before being sure new repo is reachable
		 */
		if (!cleaned) {
			/* delete unused repositories */
			pkgindb_doquery(SELECT_REPO_URLS, pdb_clean_remote,
			    NULL);
			cleaned = 1;
		}

		/* delete remote* associated to this repository */
		delete_remote_tbl(sumsw[REMOTE_SUMMARY], *prepos);
		/* update remote* table for this repository */
		insert_remote_summary(a, *prepos);
	}

	/* remove empty rows (duplicates) */
	pkgindb_doquery(DELETE_EMPTY_ROWS, NULL, NULL);
}

int
update_db(int which, char **pkgkeep, int verbose)
{
	if (!have_privs(PRIVS_PKGINDB))
		return EXIT_FAILURE;

	/* always check for LOCAL_SUMMARY updates */
	update_localdb(pkgkeep);

	if (which == REMOTE_SUMMARY)
		update_remotedb(verbose);

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
	size_t	repocount;
	char	*p;

	if ((p = getenv("PKG_REPOS")) != NULL) {
		env_repos = xstrdup(p);
	} else {
		if ((env_repos = read_repos()) == NULL)
			errx(EXIT_FAILURE, MSG_MISSING_PKG_REPOS);
	}

	repocount = 2; /* 1st elm + NULL */

	pkg_repos = xmalloc(repocount * sizeof(char *));
	*pkg_repos = env_repos;

	p = env_repos;

	while((p = strchr(p, ' ')) != NULL) {
		*p = '\0';
		p++;

		pkg_repos = xrealloc(pkg_repos, ++repocount * sizeof(char *));
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

	if (argv == NULL)
		return PDB_ERR; /* no repo yet? */

	for (i = 0; i < argc; i++) {
		match = 0;
		for (j = 0; pkg_repos[j] != NULL; j++)
			if (strcmp(argv[i], pkg_repos[j]) == 0)
				match = 1;
		if (match == 0) {
			printf(MSG_CLEANING_DB_FROM_REPO, argv[i]);
			delete_remote_tbl(sumsw[REMOTE_SUMMARY], argv[i]);
			pkgindb_dovaquery(DELETE_REPO_URL, argv[i]);

			force_fetch = 1;
		}
	}

	return PDB_OK;
}

int
chk_repo_list(void)
{
	pkgindb_doquery(SELECT_REPO_URLS, cmp_repo_list, NULL);

	return force_fetch;
}
