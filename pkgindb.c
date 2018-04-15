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

#define H_BUF   6

static sqlite3	*pdb;
static int              repo_counter = 0;

static const char *pragmaopts[] = {
	"locking_mode = EXCLUSIVE",
	"empty_result_callbacks = 1",
	"synchronous = OFF",
	"journal_mode = MEMORY",
	NULL
};

/*
 * Used to keep track of the current query so that it can be used in the
 * error log callback for diagnostics.
 *
 * Most database access goes through pkgindb_doquery which will set curquery
 * to the query it has been passed, and then resets it to NULL afterwards.
 * Any callers that perform their own database access should use curquery to
 * store their query so that errors are logged correctly.
 */
static const char *curquery = NULL;

char *pkgin_dbdir;
char *pkgin_sqldb;
char *pkgin_cache;
char *pkgin_errlog;
char *pkgin_sqllog;

void
setup_pkgin_dbdir(void)
{
	char *p;

	if ((p = getenv("PKGIN_DBDIR")) != NULL)
		pkgin_dbdir = xasprintf("%s", p);
	else
		pkgin_dbdir = xasprintf("%s", PKGIN_DBDIR);

	pkgin_sqldb = xasprintf("%s/pkgin.db", pkgin_dbdir);
	pkgin_cache = xasprintf("%s/cache", pkgin_dbdir);
	pkgin_errlog = xasprintf("%s/pkg_install-err.log", pkgin_dbdir);
	pkgin_sqllog = xasprintf("%s/sql.log", pkgin_dbdir);

	if (access(pkgin_dbdir, F_OK) != 0) {
		if (mkdir(pkgin_dbdir, 0755) < 0)
			err(1, "Failed to create %s", pkgin_dbdir);
	}

	if (access(pkgin_cache, F_OK) != 0) {
		if (mkdir(pkgin_cache, 0755) < 0)
			err(1, "Failed to create %s", pkgin_cache);
	}
}

uint8_t
have_privs(int reqd)
{
	if ((reqd & PRIVS_PKGDB) &&
	    (access(pkgdb_get_dir(), F_OK) == 0) &&
	    (access(pkgdb_get_dir(), W_OK) < 0))
		return 0;

	if ((reqd & PRIVS_PKGINDB) &&
	    (access(pkgin_dbdir, F_OK) == 0) &&
	    (access(pkgin_dbdir, W_OK) < 0))
		return 0;

	return 1;
}

const char *
pdb_version(void)
{
	return "SQLite "SQLITE_VERSION;
}

/*
 * unused: optional parameter given as 4th argument of sqlite3_exec
 * argc  : row number
 * argv  : row
 * col   : column
 *
 *    col[0]    col[1]          col[argc]
 *  ______________________________________
 * | argv[0] | argv[1] | ... | argv[argc] |
 *
 * WARNING: callback is called on every line
 */

/* sqlite callback, record a single value */
int
pdb_get_value(void *param, int argc, char **argv, char **colname)
{
	char *value = (char *)param;

	if (argv != NULL) {
		XSTRCPY(value, argv[0]);

		return PDB_OK;
	}

	return PDB_ERR;
}

/*
 * Record an error to the SQL log, optionally logging the query that caused
 * the failure.  Any errors opening the log for writing are ignored so that
 * regular users can issue queries.
 */
static void
pkgindb_sqlerr_cb(void *arg, int errcode, const char *errmsg)
{
	FILE *fp;
	struct tm tm;
	time_t now;
	char curtime[64];
	char **query = (char **)arg;

	now = time(NULL);
	tm = *(localtime(&now));
	strftime(curtime, sizeof(curtime), "%Y-%m-%d %H:%M:%S %Z", &tm);

	if ((fp = fopen(pkgin_sqllog, "a")) != NULL) {
		fprintf(fp, "SQL error at %s\n", curtime);
		fprintf(fp, "   errmsg: %s\n", errmsg);
		if (*query)
			fprintf(fp, "    query: %s\n", *query);
		fclose(fp);
	}
}

int
pkgindb_doquery(const char *query, int (*cb)(void *, int, char *[], char *[]),
    void *param)
{
	int rv;

	/*
	 * Save the current query for the error logging callback, then reset
	 * to NULL after the query has completed to avoid leaking into the
	 * error log of callers that use the sqlite3_exec interface directly.
	 */
	curquery = query;

	rv = sqlite3_exec(pdb, query, cb, param, NULL);

	curquery = NULL;

	return (rv == SQLITE_OK) ? PDB_OK : PDB_ERR;
}

int
pkgindb_dovaquery(const char *fmt, ...)
{
	char *buf;
	va_list ap;
	int rv;

	va_start(ap, fmt);
	if (vasprintf(&buf, fmt, ap) == -1)
		errx(EXIT_FAILURE, "Insufficient memory to construct query");
	va_end(ap);

	rv = pkgindb_doquery(buf, NULL, NULL);

	free(buf);

	return rv;
}

/*
 * Configure the pkgin database.  Returns 0 if opening an existing compatible
 * database, or 1 if the database needs to be created or recreated (in the case
 * of a schema upgrade).  Any other error is fatal.
 */
int
pkgindb_open(void)
{
	int create, i, oflags;
	char buf[128];

	/*
	 * Configure our sqlite error log callback function.
	 */
	sqlite3_config(SQLITE_CONFIG_LOG, pkgindb_sqlerr_cb, &curquery);

	/*
	 * Determine if we need to create a new database or can load an
	 * existing one.  The SQLITE_OPEN_READWRITE flag does not require that
	 * write privileges are available, if they are not then the database
	 * will be opened read-only, allowing normal users to perform queries.
	 */
recreate:
	create = 0;
	oflags = SQLITE_OPEN_READWRITE;
	if (access(pkgin_sqldb, F_OK) < 0) {
		create = 1;
		oflags |= SQLITE_OPEN_CREATE;
	}

	if (sqlite3_open_v2(pkgin_sqldb, &pdb, oflags, NULL) != SQLITE_OK)
		err(EXIT_FAILURE, "cannot open database");

	/*
	 * If we're creating or recreating a new database, attempt to populate
	 * the initial schema, otherwise perform a compatibility check.  If the
	 * compatibility check fails then we simply remove the existing database
	 * and recreate, there is no support for online upgrades at this time.
	 */
	if (create) {
		if (pkgindb_doquery(CREATE_DRYDB, NULL, NULL) != PDB_OK)
			errx(EXIT_FAILURE, "cannot create database: %s",
			    sqlite3_errmsg(pdb));
	} else {
		if (pkgindb_doquery("SELECT PKGDB_NTIME FROM PKGDB;",
		    NULL, NULL) != PDB_OK) {
			if (unlink(pkgin_sqldb) < 0)
				err(EXIT_FAILURE, "cannot recreate database");
			goto recreate;
		}
	}

	/* Apply PRAGMA properties */
	for (i = 0; pragmaopts[i] != NULL; i++) {
		snprintf(buf, sizeof(buf), "PRAGMA %s;", pragmaopts[i]);
		pkgindb_doquery(buf, NULL, NULL);
	}

	return create;
}

void
pkgindb_close(void)
{
	sqlite3_close(pdb);
}

static void
pkgindb_sqlfail(void)
{
	pkgindb_close();

	errx(EXIT_FAILURE, "SQL query failed, see %s", pkgin_sqllog);
}

int
pkg_db_mtime(void)
{
	sqlite3_stmt	*stmt;
	struct stat	st;
	time_t	   	db_mtime;
	long		db_ntime;
	int		rc;

	/* No pkgdb, just return up-to-date so we can start installing. */
	if (stat(pkgdb_get_dir(), &st) < 0)
		return 0;

	/*
	 * Fetch only the most recent rowid.
	 */
	curquery = "SELECT PKGDB_MTIME, PKGDB_NTIME FROM PKGDB "
		   "ORDER BY ROWID DESC LIMIT 1;";

	if (sqlite3_prepare_v2(pdb, curquery, -1, &stmt, NULL) != SQLITE_OK)
		pkgindb_sqlfail();

	rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		db_mtime = sqlite3_column_int64(stmt, 0);
		db_ntime = sqlite3_column_int64(stmt, 1);
	} else if (rc == SQLITE_DONE) {
		db_mtime = 0;
		db_ntime = 0;
	} else {
		/* This shouldn't happen, abort and investigate */
		pkgindb_sqlfail();
	}

	sqlite3_finalize(stmt);

	/* Databases matched pkgdb, we're up-to-date */
	if (db_mtime == st.st_mtime && db_ntime == st.pkgin_nanotime)
		return 0;

	/*
	 * Update database to current mtime and request a refresh.
	 */
	curquery = "INSERT INTO PKGDB (PKGDB_MTIME, PKGDB_NTIME) "
		   "VALUES (?, ?);";

	if (sqlite3_prepare_v2(pdb, curquery, -1, &stmt, NULL) != SQLITE_OK)
		pkgindb_sqlfail();

	if (sqlite3_bind_int64(stmt, 1, st.st_mtime) != SQLITE_OK)
		pkgindb_sqlfail();

	if (sqlite3_bind_int64(stmt, 2, st.pkgin_nanotime) != SQLITE_OK)
		pkgindb_sqlfail();

	if (sqlite3_step(stmt) != SQLITE_DONE)
		pkgindb_sqlfail();

	sqlite3_finalize(stmt);

	return 1;
}

void
repo_record(char **repos)
{
	int	i;
	char	query[BUFSIZ], value[20];

	for (i = 0; repos[i] != NULL; i++) {
		snprintf(query, BUFSIZ, EXISTS_REPO, repos[i]);
		pkgindb_doquery(query, pdb_get_value, &value[0]);
                repo_counter++;

		if (value[0] == '0') {
			/* repository does not exists */
			snprintf(query, BUFSIZ, INSERT_REPO, repos[i]);
			pkgindb_doquery(query, NULL, NULL);
		}
	}
}

time_t
pkg_sum_mtime(char *repo)
{
	time_t	db_mtime = 0;
	char	str_mtime[20], query[BUFSIZ];

	str_mtime[0] = '\0';

	snprintf(query, BUFSIZ,
		"SELECT REPO_MTIME FROM REPOS WHERE REPO_URL GLOB \'%s*\';",
		repo);
	pkgindb_doquery(query, pdb_get_value, str_mtime);

	if (str_mtime[0] != '\0')
		db_mtime = (time_t)strtol(str_mtime, (char **)NULL, 10);

	return db_mtime;
}

void
pkgindb_stats(void)
{
        int     i;
        int64_t local_size;
        int64_t remote_size;
        char    h_local_size[H_BUF];
        char    h_remote_size[H_BUF];
        char    query[BUFSIZ];

        struct {
                char value[BUFSIZ];
                const char *op;
                const char *term;
                const char *place;
        } stats[] = {
                { "", "COUNT", "PKG_ID", "LOCAL" },
                { "", "COUNT", "PKG_ID", "REMOTE" },
                { "", "SUM", "SIZE_PKG", "LOCAL" },
                { "", "SUM", "FILE_SIZE", "REMOTE" },
                { "", NULL, NULL, NULL },
        };

        for (i = 0; stats[i].op != NULL; i++) {
                snprintf(query, BUFSIZ, "SELECT %s(%s) FROM %s_PKG;",
                        stats[i].op, stats[i].term, stats[i].place);
                pkgindb_doquery(query, pdb_get_value, stats[i].value);
        }

        local_size = strtol(stats[2].value, NULL, 10);
        remote_size = strtol(stats[3].value, NULL, 10);

        (void)humanize_number(h_local_size, H_BUF, local_size, "",
                        HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
        (void)humanize_number(h_remote_size, H_BUF, remote_size, "",
                        HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);

        printf(MSG_LOCAL_STAT_TITLE
                MSG_LOCAL_PACKAGES
                MSG_LOCAL_PKG_SIZE
                MSG_REMOTE_STAT_TITLE
                MSG_REMOTE_NB_REPOS
                MSG_REMOTE_PACKAGES
                MSG_REMOTE_PKG_SIZE,
                stats[0].value, h_local_size, repo_counter, stats[1].value, h_remote_size);
}
