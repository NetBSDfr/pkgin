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

#include <sqlite3.h>
#include "pkgin.h"

/*
 * Table name lookup, as a convenience for tables that have identical LOCAL_
 * and REMOTE_ entities.
 */
static const struct Summary {
	const int	type;
	const char	*pkg;
	const char	*conflicts;
	const char	*depends;
	const char	*provides;
	const char	*requires;
	const char	*supersedes;
	const char	*end;
} sumsw[] = {
	[LOCAL_SUMMARY] = {
		LOCAL_SUMMARY,
		"LOCAL_PKG",
		"LOCAL_CONFLICTS",
		"LOCAL_DEPENDS",
		"LOCAL_PROVIDES",
		"LOCAL_REQUIRES",
		"LOCAL_SUPERSEDES",	/* Unused */
		NULL
	},
	[REMOTE_SUMMARY] = {
		REMOTE_SUMMARY,
		"REMOTE_PKG",
		"REMOTE_CONFLICTS",
		"REMOTE_DEPENDS",
		"REMOTE_PROVIDES",
		"REMOTE_REQUIRES",
		"REMOTE_SUPERSEDES",
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

static struct archive	*fetch_summary(char *, time_t *);
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

/*
 * Slow machines with limited memory will probably want to define this to
 * reduce memory usage and speed up decompression, at the cost of increased
 * bandwidth usage (pkg_summary approx sizes: gz=5MB, bz2=3MB, xz=2MB).
 */
#if defined(PREFER_GZIP_SUMMARY)
static const char *const sumexts[] = { "gz", "bz2", "xz", NULL };
#else
static const char *const sumexts[] = { "xz", "bz2", "gz", NULL };
#endif

/*
 * Open a remote summary and return an open libarchive handler to it.
 */
static struct archive *
fetch_summary(char *cur_repo, time_t *repo_mtime)
{
	struct	archive *a;
	struct	archive_entry *ae;
	Sumfile	*sum = NULL;
	int	i;
	char	buf[BUFSIZ];

	for (i = 0; sumexts[i] != NULL; i++) { /* try all extensions */
		if (!force_fetch)
			*repo_mtime = pkg_sum_mtime(cur_repo);
		else
			*repo_mtime = 0; /* 0 sumtime == force reload */

		snprintf(buf, BUFSIZ, "%s/%s.%s",
				cur_repo, PKG_SUMMARY, sumexts[i]);

		if ((sum = sum_open(buf, repo_mtime)) != NULL)
			break; /* pkg_summary found and not up-to-date */

		if (*repo_mtime < 0) /* pkg_summary found, but up-to-date */
			return NULL;
	}

	if (sum == NULL)
		errx(EXIT_FAILURE, MSG_COULDNT_FETCH, buf, fetchLastErrString);

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

	sqlite3_snprintf(sizeof(querybuf), querybuf, "INSERT INTO %w (PKG_ID",
	    sum.pkg);

	/* insert fields */
	SLIST_FOREACH(pi, &inserthead, next) {
		sqlite3_snprintf(sizeof(tmpbuf), tmpbuf, ",%w", pi->field);
		if (strlcat(querybuf, tmpbuf, sizeof(querybuf)) >=
		    sizeof(querybuf))
			goto err;
	}

	sqlite3_snprintf(sizeof(tmpbuf), tmpbuf, ") VALUES (%d", pkgid);
	if (strlcat(querybuf, tmpbuf, sizeof(querybuf)) >= sizeof(querybuf))
		goto err;

	/* insert values */
	SLIST_FOREACH(pi, &inserthead, next) {
		sqlite3_snprintf(sizeof(tmpbuf), tmpbuf, ",%Q", pi->value);
		if (strlcat(querybuf, tmpbuf, sizeof(querybuf)) >=
		    sizeof(querybuf))
			goto err;
	}

	sqlite3_snprintf(sizeof(tmpbuf), tmpbuf, ");");
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
	char		*val, *v, buf[BUFSIZ];

	if ((val = strchr(line, '=')) == NULL)
		errx(EXIT_FAILURE, "Invalid pkg_info entry: %s", line);

	val++;

	/*
	 * Check MACHINE_ARCH of the package matches the local machine.
	 */
	if (check_machine_arch && strncmp(line, "MACHINE_ARCH=", 13) == 0) {
		if (strncmp(MACHINE_ARCH, val, strlen(MACHINE_ARCH))) {
			alarm(0); /* Stop the progress meter */
			printf(MSG_ARCH_DONT_MATCH, val, MACHINE_ARCH);
			if (!check_yesno(DEFAULT_NO))
				exit(EXIT_FAILURE);
			check_machine_arch = 0;
			alarm(1); /* Restart progress XXX: UPDATE_INTERVAL */
		}
		return;
	}

	if (strncmp(line, "CONFLICTS=", 10) == 0) {
		v = pkgname_from_pattern(val);
		pkgindb_dovaquery(INSERT_CONFLICTS, sum.conflicts, pkgid, val,
		    v);
		free(v);
		return;
	}

	if (strncmp(line, "DEPENDS=", 8) == 0) {
		v = pkgname_from_pattern(val);
		pkgindb_dovaquery(INSERT_DEPENDS, sum.depends, pkgid, val, v);
		free(v);
		return;
	}

	if (strncmp(line, "PROVIDES=", 9) == 0) {
		pkgindb_dovaquery(INSERT_PROVIDES, sum.provides, pkgid, val);
		return;
	}

	if (strncmp(line, "REQUIRES=", 9) == 0) {
		pkgindb_dovaquery(INSERT_REQUIRES, sum.requires, pkgid, val);
		return;
	}

	if (strncmp(line, "SUPERSEDES=", 11) == 0) {
		/*
		 * Only remote SUPERSEDES are supported.
		 */
		if (sum.type == REMOTE_SUMMARY) {
			v = pkgname_from_pattern(val);
			pkgindb_dovaquery(INSERT_SUPERSEDES, sum.supersedes,
			    pkgid, val, v);
			free(v);
		}
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
	uint64_t	savepoint;

	if (fp == NULL) {
		pkgindb_close();
		errx(EXIT_FAILURE, "Couldn't read local pkg_info");
	}

	/* record columns names to cols */
	sqlite3_snprintf(BUFSIZ, buf, "PRAGMA table_info(%w);",
	    sumsw[LOCAL_SUMMARY].pkg);
	pkgindb_doquery(buf, colnames, NULL);

	SLIST_INIT(&inserthead);

	savepoint = pkgindb_savepoint();

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

	pkgindb_savepoint_release(savepoint);
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
	uint64_t	savepoint;

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
	sqlite3_snprintf(buflen, buf, "PRAGMA table_info(%w);",
	    sumsw[REMOTE_SUMMARY].pkg);
	pkgindb_doquery(buf, colnames, NULL);

	SLIST_INIT(&inserthead);

	savepoint = pkgindb_savepoint();

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

	if (r != ARCHIVE_OK) {
		pkgindb_savepoint_rollback(savepoint);
		errx(EXIT_FAILURE, "Short read of pkg_summary: %s",
		    archive_error_string(a));
	}

	pkgindb_savepoint_release(savepoint);

#if ARCHIVE_VERSION_NUMBER < 3000000
	archive_read_finish(a);
#else
	archive_read_free(a);
#endif
}

static void
delete_remote_tbl(struct Summary sum, char *repo)
{
	const char **table;

	/*
	 * Use results from REMOTE_PKG to delete entries from REMOTE_CONFLICTS
	 * etc first, then finally remove from REMOTE_PKG.
	 */
	for (table = &(sum.pkg) + 1; *table != NULL; ++table) {
		pkgindb_dovaquery(DELETE_REMOTE, *table, repo);
	}
	pkgindb_dovaquery(DELETE_REMOTE_PKG_REPO, repo);
}

/*
 * The following section is taken verbatim from pkg_install files/admin/main.c
 * to correctly handle REQUIRED_BY entries.
 */
struct reqd_by_entry {
	char *pkgname;
	SLIST_ENTRY(reqd_by_entry) entries;
};
SLIST_HEAD(reqd_by_entry_head, reqd_by_entry);

struct pkg_reqd_by {
	char *pkgname;
	struct reqd_by_entry_head required_by[PKG_HASH_SIZE];
	SLIST_ENTRY(pkg_reqd_by) entries;
};
SLIST_HEAD(pkg_reqd_by_head, pkg_reqd_by);

static void
add_required_by(const char *pattern, const char *pkgname, struct pkg_reqd_by_head *hash)
{
	struct pkg_reqd_by_head *phead;
	struct pkg_reqd_by *pkg;
	struct reqd_by_entry_head *ehead;
	struct reqd_by_entry *entry;
	char *best_installed;
	int i;

	best_installed = find_matching_installed_pkg(pattern, 1, 0);
	if (best_installed == NULL) {
		warnx("Dependency %s of %s unresolved", pattern, pkgname);
		return;
	}
	phead = &hash[PKG_HASH_ENTRY(best_installed)];
	SLIST_FOREACH(pkg, phead, entries) {
		if (strcmp(pkg->pkgname, best_installed) == 0) {
			ehead = &pkg->required_by[PKG_HASH_ENTRY(pkgname)];
			SLIST_FOREACH(entry, ehead, entries) {
				if (strcmp(entry->pkgname, pkgname) == 0)
					break;
			}
			if (entry == NULL) {
				entry = xmalloc(sizeof(*entry));
				entry->pkgname = xstrdup(pkgname);
				SLIST_INSERT_HEAD(ehead, entry, entries);
			}
			break;
		}
	}
	if (pkg == NULL) {
		pkg = xmalloc(sizeof(*pkg));
		pkg->pkgname = xstrdup(best_installed);
		for (i = 0; i < PKG_HASH_SIZE; i++)
			SLIST_INIT(&pkg->required_by[i]);
		ehead = &pkg->required_by[PKG_HASH_ENTRY(pkgname)];
		entry = xmalloc(sizeof(*entry));
		entry->pkgname = xstrdup(pkgname);
		SLIST_INSERT_HEAD(ehead, entry, entries);
		SLIST_INSERT_HEAD(phead, pkg, entries);
	}
	free(best_installed);
}

static int
add_depends_of(const char *pkgname, void *cookie)
{
	FILE *fp;
	struct pkg_reqd_by_head *h = cookie;
	plist_t *p;
	package_t plist;
	char *path;

	path = pkgdb_pkg_file(pkgname, CONTENTS_FNAME);
	if ((fp = fopen(path, "r")) == NULL)
		errx(EXIT_FAILURE, "Cannot read %s of package %s",
		    CONTENTS_FNAME, pkgname);
	free(path);
	read_plist(&plist, fp);
	fclose(fp);

	for (p = plist.head; p; p = p->next) {
		if (p->type == PLIST_PKGDEP)
			add_required_by(p->name, pkgname, h);
	}

	free_plist(&plist);

	return 0;
}

static void
insert_local_required_by(void)
{
	struct pkg_reqd_by_head pkgs[PKG_HASH_SIZE];
	struct pkg_reqd_by *p;
	struct reqd_by_entry *e;
	int i, j;

	for (i = 0; i < PKG_HASH_SIZE; i++)
		SLIST_INIT(&pkgs[i]);

	if (iterate_pkg_db(add_depends_of, &pkgs) == -1)
		errx(EXIT_FAILURE, "cannot iterate pkgdb");

	for (i = 0; i < PKG_HASH_SIZE; i++) {
		SLIST_FOREACH(p, &pkgs[i], entries) {
			for (j = 0; j < PKG_HASH_SIZE; j++) {
				SLIST_FOREACH(e, &p->required_by[j], entries) {
					pkgindb_dovaquery(INSERT_REQUIRED_BY,
					    p->pkgname, e->pkgname);
					free(e->pkgname);
				}
				while (!SLIST_EMPTY(&p->required_by[j])) {
					e = SLIST_FIRST(&p->required_by[j]);
					SLIST_REMOVE_HEAD(&p->required_by[j], entries);
					free(e);
				}
			}
			free(p->pkgname);
		}
		while (!SLIST_EMPTY(&pkgs[i])) {
			p = SLIST_FIRST(&pkgs[i]);
			SLIST_REMOVE_HEAD(&pkgs[i], entries);
			free(p);
		}
	}
}

static void
update_localdb(int verbose)
{
	FILE *pinfo;
	struct stat st;
	Pkglist *lpkg;
	int l;

	/*
	 * Start a write transaction, excluding other writers until committed.
	 */
	if (pkgindb_doquery("BEGIN IMMEDIATE;", NULL, NULL))
		errx(EXIT_FAILURE, "failed to begin immediate transaction");

	/*
	 * Only replace the database if forced or if the pkgdb changed.
	 */
	if (!pkg_db_mtime(&st) && !force_fetch)
		goto out;

	/*
	 * Delete and recreate the LOCAL_PKG table, as it's simpler and faster
	 * than updating.
	 */
	pkgindb_doquery(DELETE_LOCAL, NULL, NULL);

	if (verbose)
		printf(MSG_READING_LOCAL_SUMMARY);

	if ((pinfo = popen(PKG_INSTALL_DIR "/pkg_info -Xa", "r")) == NULL)
		errx(EXIT_FAILURE, "Couldn't run pkg_info");

	if (verbose)
		printf(MSG_PROCESSING_LOCAL_SUMMARY);

	insert_local_summary(pinfo);
	insert_local_required_by();
	pkg_db_update_mtime(&st);
	pclose(pinfo);

	/*
	 * Reread the local package list.  This updates l_plisthead.
	 */
	free_local_pkglist();
	init_local_pkglist();

	/*
	 * Insert PKG_KEEP database entries based on pkgdb data.
	 */
	for (l = 0; l < LOCAL_PKG_HASH_SIZE; l++) {
	SLIST_FOREACH(lpkg, &l_plisthead[l], next) {
		if (!is_automatic_installed(lpkg->full)) {
			pkg_keep(KEEP, lpkg->full);
		}
	}
	}
out:
	if (pkgindb_doquery("COMMIT;", NULL, NULL))
		errx(EXIT_FAILURE, "failed to commit transaction");
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
		    strncmp(repos[i], argv[0], repolen) == 0)
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
	time_t		repo_mtime;
	char		**prepos;
	uint8_t		cleaned = 0;

	/* loop through PKG_REPOS */
	for (prepos = pkg_repos; *prepos != NULL; prepos++) {

		if (verbose)
			printf(MSG_PROCESSING_REMOTE_SUMMARY, *prepos);

		/* load remote pkg_summary */
		if ((a = fetch_summary(*prepos, &repo_mtime)) == NULL) {
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

		/* mark repository as being updated with new mtime */
		pkgindb_dovaquery(UPDATE_REPO_MTIME, (long long)repo_mtime, *prepos);
	}

	/* remove empty rows (duplicates) */
	pkgindb_doquery(DELETE_EMPTY_ROWS, NULL, NULL);
}

int
update_db(int which, int verbose)
{
	if (!have_privs(PRIVS_PKGINDB))
		return EXIT_FAILURE;

	/* always check for LOCAL_SUMMARY updates */
	update_localdb(verbose);

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
chk_repo_list(int force)
{
	pkgindb_doquery(SELECT_REPO_URLS, cmp_repo_list, NULL);

	if (force)
		force_fetch = 1;

	return force_fetch;
}
