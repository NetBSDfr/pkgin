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

/*
 * This query checks the compatibility of the current database, and should be
 * one that either completes or fails due to an SQL error based on the most
 * recent schema change.  Returned rows are ignored, so choose a query that
 * runs quickly.
 */
const char CHECK_DB_LATEST[] =
	"SELECT pkgbase "
	"  FROM local_conflicts "
	" LIMIT 1;";

const char DELETE_LOCAL[] =
	"DELETE FROM LOCAL_PKG;"
	"DELETE FROM LOCAL_CONFLICTS;"
	"DELETE FROM LOCAL_DEPENDS;"
	"DELETE FROM LOCAL_PROVIDES;"
	"DELETE FROM LOCAL_REQUIRES;"
	"DELETE FROM LOCAL_REQUIRED_BY;";

const char DELETE_REMOTE[] =
	"DELETE FROM %s "
	" WHERE pkg_id IN "
	"    (SELECT pkg_id "
	"       FROM remote_pkg "
	"      WHERE repository GLOB %Q || '*' "
	"    );";

const char DELETE_REMOTE_PKG_REPO[] =
	"DELETE FROM REMOTE_PKG WHERE REPOSITORY = %Q;";

const char LOCAL_DIRECT_DEPENDS[] =
	"SELECT pattern, pkgbase "
	"  FROM local_depends, local_pkg "
	" WHERE fullpkgname = %Q "
	"   AND local_depends.pkg_id = local_pkg.pkg_id;";

const char REMOTE_DIRECT_DEPENDS[] =
	"SELECT pattern, pkgbase "
	"  FROM remote_depends, remote_pkg "
	" WHERE fullpkgname = %Q "
	"   AND remote_depends.pkg_id = remote_pkg.pkg_id;";

const char LOCAL_REVERSE_DEPENDS[] =
	"SELECT required_by, local_pkg.pkgname, local_pkg.pkg_keep "
	"  FROM local_pkg "
	"  LEFT JOIN local_required_by "
	"    ON local_pkg.fullpkgname = local_required_by.required_by "
	" WHERE local_required_by.pkgname = %Q;";

const char LOCAL_CONFLICTS[] =
	"SELECT DISTINCT pattern, pkgbase "
	"  FROM local_conflicts;";

const char LOCAL_PROVIDES[] =
	"SELECT filename "
	"  FROM local_provides;";

const char REMOTE_CONFLICTS[] =
	"SELECT local_pkg.fullpkgname "
	"  FROM local_conflicts, local_pkg "
	" WHERE local_conflicts.pattern = %Q "
	"   AND local_conflicts.pkg_id = local_pkg.pkg_id;";

const char REMOTE_PROVIDES[] =
	"SELECT filename "
	"  FROM remote_provides, remote_pkg "
	" WHERE fullpkgname = %Q "
	"   AND remote_provides.pkg_id = remote_pkg.pkg_id;";

const char REMOTE_REQUIRES[] =
	"SELECT filename "
	"  FROM remote_requires, remote_pkg "
	" WHERE fullpkgname = %Q "
	"   AND remote_requires.pkg_id = remote_pkg.pkg_id;";

const char REMOTE_SUPERSEDES[] =
	"SELECT pattern, pkgbase, pkgname "
	"  FROM remote_supersedes "
	"  LEFT JOIN remote_pkg "
	"    ON remote_supersedes.pkg_id = remote_pkg.pkg_id;";

const char KEEP_PKG[] =
	"UPDATE LOCAL_PKG SET PKG_KEEP = 1 WHERE PKGNAME = %Q;";
const char UNKEEP_PKG[] =
	"UPDATE LOCAL_PKG SET PKG_KEEP = NULL WHERE PKGNAME = %Q;";

/* for upgrades, prefer higher versions to be at the top of SLIST */
const char LOCAL_PKGS_QUERY_ASC[] =
	"SELECT FULLPKGNAME,PKGNAME,PKGVERS,BUILD_DATE,"
	"COMMENT,FILE_SIZE,SIZE_PKG,CATEGORIES,PKGPATH,PKG_KEEP "
	"FROM LOCAL_PKG "
	"ORDER BY FULLPKGNAME ASC;";

/* present packages by repository appearance to avoid conflicts between repos */
const char REMOTE_PKGS_QUERY_ASC[] =
	"SELECT FULLPKGNAME,PKGNAME,PKGVERS,BUILD_DATE,"
	"COMMENT,FILE_SIZE,SIZE_PKG,CATEGORIES,PKGPATH "
	"FROM REMOTE_PKG "
	"INNER JOIN REPOS WHERE REMOTE_PKG.REPOSITORY = REPOS.REPO_URL "
	"ORDER BY REPOS.ROWID, FULLPKGNAME ASC;";

/* for displays, prefer lower versions to be at the top of SLIST*/
const char LOCAL_PKGS_QUERY_DESC[] =
	"SELECT FULLPKGNAME,PKGNAME,PKGVERS,BUILD_DATE,"
	"COMMENT,FILE_SIZE,SIZE_PKG,CATEGORIES,PKGPATH "
	"FROM LOCAL_PKG "
	"ORDER BY FULLPKGNAME DESC;";

const char REMOTE_PKGS_QUERY_DESC[] =
	"SELECT FULLPKGNAME,PKGNAME,PKGVERS,BUILD_DATE,"
	"COMMENT,FILE_SIZE,SIZE_PKG,CATEGORIES,PKGPATH "
	"FROM REMOTE_PKG "
	"ORDER BY FULLPKGNAME DESC;";

const char NOKEEP_LOCAL_PKGS[] =
	"SELECT fullpkgname,pkgname,pkgpath,comment "
	"  FROM local_pkg "
	" WHERE pkg_keep IS NULL "
	" ORDER BY fullpkgname DESC;";

const char KEEP_LOCAL_PKGS[] =
	"SELECT fullpkgname,pkgname,pkgpath,comment "
	"  FROM local_pkg "
	" WHERE pkg_keep IS NOT NULL"
	" ORDER BY fullpkgname DESC;";

const char PKG_URL[] =
	"SELECT REPOSITORY FROM REMOTE_PKG WHERE FULLPKGNAME = %Q;";

const char DELETE_EMPTY_ROWS[] =
	"DELETE FROM REMOTE_PKG WHERE PKGNAME IS NULL;";

const char SELECT_REPO_URLS[] =
	"SELECT REPO_URL FROM REPOS;";

const char EXISTS_REPO[] =
	"SELECT COUNT(*) FROM REPOS WHERE REPO_URL = %Q;";

const char INSERT_REPO[] =
	"INSERT INTO REPOS (REPO_URL, REPO_MTIME) VALUES (%Q, 0);";

const char UPDATE_REPO_MTIME[] =
	"UPDATE REPOS SET REPO_MTIME = %lld WHERE REPO_URL = %Q;";

const char DELETE_REPO_URL[] =
	"DELETE FROM REPOS WHERE REPO_URL = %Q;";

const char INSERT_CONFLICTS[] =
	"INSERT INTO %s (pkg_id, pattern, pkgbase) VALUES (%d, %Q, %Q);";

const char INSERT_DEPENDS[] =
	"INSERT INTO %s (pkg_id, pattern, pkgbase) VALUES (%d, %Q, %Q);";

const char INSERT_PROVIDES[] =
	"INSERT INTO %s (PKG_ID, FILENAME) VALUES (%d, %Q);";

const char INSERT_REQUIRES[] =
	"INSERT INTO %s (PKG_ID, FILENAME) VALUES (%d, %Q);";

const char INSERT_SUPERSEDES[] =
	"INSERT INTO %s (pkg_id, pattern, pkgbase) VALUES (%d, %Q, %Q);";

const char INSERT_REQUIRED_BY[] =
	"INSERT INTO LOCAL_REQUIRED_BY (PKGNAME, REQUIRED_BY) VALUES (%Q, %Q);";

const char UNIQUE_PKG[] =
	"SELECT FULLPKGNAME, PKGVERS FROM %s WHERE PKGNAME = %Q;";

const char UNIQUE_EXACT_PKG[] =
	"SELECT FULLPKGNAME, PKGVERS FROM %s WHERE FULLPKGNAME GLOB %Q || '*';";

const char EXPORT_KEEP_LIST[] =
	"SELECT PKGPATH FROM LOCAL_PKG "
	"WHERE PKG_KEEP IS NOT NULL AND PKGPATH IS NOT NULL "
	"ORDER BY PKG_ID DESC;";

const char GET_PKGNAME_BY_PKGPATH[] =
	"SELECT FULLPKGNAME FROM REMOTE_PKG WHERE PKGPATH = %Q;";

const char SHOW_ALL_CATEGORIES[] =
	"SELECT DISTINCT CATEGORIES FROM REMOTE_PKG WHERE "
	"CATEGORIES NOT LIKE '%% %%' ORDER BY CATEGORIES DESC;";
