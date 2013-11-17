/* $Id: pkgindb_queries.c,v 1.27 2012/11/24 18:37:42 imilh Exp $ */

/*
 * Copyright (c) 2009, 2010, 2011, 2012 The NetBSD Foundation, Inc.
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

const char DROP_LOCAL_TABLES[] =
    "DROP TABLE IF EXISTS LOCAL_DEPS;"
    "DROP TABLE IF EXISTS LOCAL_PKG;"
    "DROP TABLE IF EXISTS LOCAL_CONFLICTS;"
    "DROP TABLE IF EXISTS LOCAL_REQUIRES;"
    "DROP TABLE IF EXISTS LOCAL_PROVIDES;";

const char DROP_REMOTE_TABLES[] =
    "DROP TABLE IF EXISTS REMOTE_DEPS;"
    "DROP TABLE IF EXISTS REMOTE_PKG;"
    "DROP TABLE IF EXISTS REMOTE_CONFLICTS;"
    "DROP TABLE IF EXISTS REMOTE_REQUIRES;"
    "DROP TABLE IF EXISTS REMOTE_PROVIDES;";

const char DELETE_LOCAL[] =
    "DELETE FROM LOCAL_DEPS;"
    "DELETE FROM LOCAL_PKG;"
    "DELETE FROM LOCAL_CONFLICTS;"
    "DELETE FROM LOCAL_REQUIRES;"
    "DELETE FROM LOCAL_PROVIDES;";

const char DELETE_REMOTE[] =
	"DELETE FROM %s WHERE %s_ID IN "
	"(SELECT %s.%s_ID FROM REMOTE_PKG, %s "
	"WHERE REMOTE_PKG.REPOSITORY GLOB '%s*' AND "
	"REMOTE_PKG.PKG_ID = %s.PKG_ID);";

const char DIRECT_DEPS[] = /* prefer higher version */
	"SELECT REMOTE_DEPS_DEWEY, REMOTE_DEPS_PKGNAME "
	"FROM REMOTE_DEPS WHERE PKG_ID = "
	"(SELECT PKG_ID FROM REMOTE_PKG WHERE PKGNAME = '%s' "
	"ORDER BY FULLPKGNAME DESC LIMIT 1);";

const char LOCAL_DIRECT_DEPS[] =
	"SELECT LOCAL_DEPS_DEWEY, LOCAL_DEPS_PKGNAME "
	"FROM LOCAL_DEPS WHERE PKG_ID = "
	"(SELECT PKG_ID FROM LOCAL_PKG WHERE PKGNAME = '%s' "
	"ORDER BY FULLPKGNAME DESC LIMIT 1);";

const char EXACT_DIRECT_DEPS[] =
	"SELECT REMOTE_DEPS.REMOTE_DEPS_DEWEY, REMOTE_DEPS.REMOTE_DEPS_PKGNAME "
	"FROM REMOTE_DEPS,REMOTE_PKG "
	"WHERE REMOTE_PKG.FULLPKGNAME = '%s' "
	"AND REMOTE_DEPS.PKG_ID = REMOTE_PKG.PKG_ID;";

const char LOCAL_REVERSE_DEPS[] =
    "SELECT LOCAL_PKG.FULLPKGNAME, LOCAL_PKG.PKGNAME, LOCAL_PKG.PKG_KEEP "
    "FROM LOCAL_PKG, LOCAL_DEPS "
	"WHERE LOCAL_DEPS.LOCAL_DEPS_PKGNAME  = '%s' "
    "AND LOCAL_PKG.PKG_ID = LOCAL_DEPS.PKG_ID;";

const char REMOTE_REVERSE_DEPS[] =
    "SELECT REMOTE_PKG.FULLPKGNAME, REMOTE_PKG.PKGNAME "
    "FROM REMOTE_PKG, REMOTE_DEPS, LOCAL_DEPS "
	"WHERE LOCAL_DEPS.LOCAL_DEPS_PKGNAME  = '%s' "
    "AND REMOTE_PKG.PKG_ID = REMOTE_DEPS.PKG_ID;";

const char LOCAL_CONFLICTS[] =
    "SELECT LOCAL_CONFLICTS_PKGNAME FROM LOCAL_CONFLICTS;";

const char GET_CONFLICT_QUERY[] =
    "SELECT LOCAL_PKG.FULLPKGNAME FROM LOCAL_CONFLICTS,LOCAL_PKG "
    "WHERE LOCAL_CONFLICTS.LOCAL_CONFLICTS_PKGNAME = '%s' "
    "AND LOCAL_CONFLICTS.PKG_ID = LOCAL_PKG.PKG_ID;";

/* naming dirt, this is not really a PKGNAME, but this shortcut permits
 * the use of a generic function in summary.c (child_table)
 */
const char GET_REQUIRES_QUERY[] =
    "SELECT REMOTE_REQUIRES.REMOTE_REQUIRES_PKGNAME "
    "FROM REMOTE_REQUIRES,REMOTE_PKG "
    "WHERE REMOTE_PKG.FULLPKGNAME = '%s' "
    "AND REMOTE_REQUIRES.PKG_ID = REMOTE_PKG.PKG_ID;";

const char GET_PROVIDES_QUERY[] =
    "SELECT REMOTE_PROVIDES.REMOTE_PROVIDES_PKGNAME "
    "FROM REMOTE_PROVIDES,REMOTE_PKG "
    "WHERE REMOTE_PKG.FULLPKGNAME = '%s' "
    "AND REMOTE_PROVIDES.PKG_ID = REMOTE_PKG.PKG_ID;";

const char LOCAL_PROVIDES[] =
    "SELECT LOCAL_PROVIDES_PKGNAME FROM LOCAL_PROVIDES;";

const char KEEP_PKG[] =
    "UPDATE LOCAL_PKG SET PKG_KEEP = 1 WHERE PKGNAME = \'%s\';";
const char UNKEEP_PKG[] =
    "UPDATE LOCAL_PKG SET PKG_KEEP = NULL WHERE PKGNAME = \'%s\';";

/* for upgrades, prefer higher versions to be at the top of SLIST */
const char LOCAL_PKGS_QUERY_ASC[] =
    "SELECT FULLPKGNAME,PKGNAME,PKGVERS,"
	"COMMENT,FILE_SIZE,SIZE_PKG,CATEGORIES,PKGPATH "
	"FROM LOCAL_PKG "
    "ORDER BY FULLPKGNAME ASC;";

const char REMOTE_PKGS_QUERY_ASC[] =
    "SELECT FULLPKGNAME,PKGNAME,PKGVERS,"
	"COMMENT,FILE_SIZE,SIZE_PKG,CATEGORIES,PKGPATH "
	"FROM REMOTE_PKG "
    "ORDER BY FULLPKGNAME ASC;";

/* for displays, prefer lower versions to be at the top of SLIST*/
const char LOCAL_PKGS_QUERY_DESC[] =
    "SELECT FULLPKGNAME,PKGNAME,PKGVERS,"
	"COMMENT,FILE_SIZE,SIZE_PKG,CATEGORIES,PKGPATH "
	"FROM LOCAL_PKG "
    "ORDER BY FULLPKGNAME DESC;";

const char REMOTE_PKGS_QUERY_DESC[] =
    "SELECT FULLPKGNAME,PKGNAME,PKGVERS,"
	"COMMENT,FILE_SIZE,SIZE_PKG,CATEGORIES,PKGPATH "
	"FROM REMOTE_PKG "
    "ORDER BY FULLPKGNAME DESC;";

const char NOKEEP_LOCAL_PKGS[] =
    "SELECT FULLPKGNAME,PKGNAME,PKGPATH "
	"FROM LOCAL_PKG WHERE PKG_KEEP IS NULL;";

const char KEEP_LOCAL_PKGS[] =
    "SELECT FULLPKGNAME,PKGNAME,PKGPATH "
	"FROM LOCAL_PKG WHERE PKG_KEEP IS NOT NULL;";

const char PKG_URL[] =
    "SELECT REPOSITORY FROM REMOTE_PKG WHERE FULLPKGNAME = \'%s\';";

const char DELETE_EMPTY_ROWS[] =
    "DELETE FROM REMOTE_PKG WHERE PKGNAME IS NULL;";

const char UPDATE_PKGDB_MTIME[] =
    "REPLACE INTO PKGDB (PKGDB_MTIME) VALUES (%lld);";

const char EXISTS_REPO[] =
    "SELECT COUNT(*) FROM REPOS WHERE REPO_URL = \'%s\';";

const char INSERT_REPO[] =
    "INSERT INTO REPOS (REPO_URL, REPO_MTIME) VALUES (\'%s\', 0);";

const char UPDATE_REPO_MTIME[] =
    "UPDATE REPOS SET REPO_MTIME = %lld WHERE REPO_URL = \'%s\';";

const char INSERT_SINGLE_VALUE[] =
	"INSERT INTO %s (PKG_ID, %s_PKGNAME) VALUES (%d,\"%s\");";

const char INSERT_DEPENDS_VALUES[] = 
	"INSERT INTO %s (PKG_ID, %s_PKGNAME, %s_DEWEY) VALUES (%d,\"%s\",\"%s\");";

const char UNIQUE_PKG[] = 
	"SELECT FULLPKGNAME, PKGVERS FROM %s WHERE PKGNAME = '%s';";

const char UNIQUE_EXACT_PKG[] = 
	"SELECT FULLPKGNAME, PKGVERS FROM %s WHERE FULLPKGNAME GLOB '%s*';";

const char EXPORT_KEEP_LIST[] =
	"SELECT PKGPATH FROM LOCAL_PKG WHERE PKG_KEEP IS NOT NULL "
	"ORDER BY PKG_ID DESC;";

const char GET_PKGNAME_BY_PKGPATH[] = 
	"SELECT PKGNAME FROM REMOTE_PKG WHERE PKGPATH = '%s';";

const char GET_ORPHAN_PACKAGES[] = 
	"SELECT FULLPKGNAME FROM LOCAL_PKG WHERE PKG_KEEP IS NULL AND "
	"PKGNAME NOT IN (SELECT LOCAL_DEPS_PKGNAME FROM LOCAL_DEPS);";

const char COMPAT_CHECK[] =
	"SELECT FULLPKGNAME FROM REMOTE_PKG LIMIT 1;";

const char SHOW_ALL_CATEGORIES[] =
	"SELECT DISTINCT CATEGORIES FROM REMOTE_PKG WHERE "
	"CATEGORIES NOT LIKE '%% %%' ORDER BY CATEGORIES DESC;";

const char LOCAL_PKG_COUNT[] = 
    "SELECT COUNT(PKG_ID) FROM LOCAL_PKG;";

const char LOCAL_PKG_SIZE[] = 
    "SELECT SUM(SIZE_PKG) FROM LOCAL_PKG;";

const char REMOTE_PKG_COUNT[] = 
    "SELECT COUNT(PKG_ID) FROM REMOTE_PKG;";

const char REMOTE_PKG_SIZE[] = 
    "SELECT SUM(FILE_SIZE) FROM REMOTE_PKG;";