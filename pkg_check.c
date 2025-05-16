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

void
get_conflicts(Plistarray *conflicts)
{
	pkgindb_doquery(LOCAL_CONFLICTS, record_pattern_to_array, conflicts);
}

/*
 * Check that REQUIRES is satisifed for incoming packages.  As the check
 * happens before install, we have to exclude any REQUIRES for libraries under
 * PREFIX as they may not exist yet.
 *
 * TODO: Record what will be installed from PROVIDES and enable check for files
 * under PREFIX.
 */
int
pkg_met_reqs(Plisthead *impacthead)
{
	struct stat	sb;
	Plistnumbered	*reqhead;
	Pkglist		*pkg, *req;
	size_t		len;
	int		met_reqs = 1;

	len = strlen(PREFIX) - 1;

	SLIST_FOREACH(pkg, impacthead, next) {
		if (!action_is_install(pkg->action))
			continue;

		reqhead = rec_pkglist(REMOTE_REQUIRES, pkg->rpkg->full);
		if (reqhead == NULL)
			continue;

		SLIST_FOREACH(req, reqhead->P_Plisthead, next) {
			if (strncmp(req->full, PREFIX, len) == 0)
				continue;
			if (stat(req->full, &sb) < 0) {
				printf(MSG_REQT_NOT_PRESENT, req->full,
				    pkg->rpkg->full);
				pkg->action = ACTION_UNMET_REQ;
				met_reqs = 0;
			}
		}

		free_pkglist(&reqhead->P_Plisthead);
		free(reqhead);
	}

	return met_reqs;
}

/*
 * A remote package has been identified as matching a local conflict.  Retrieve
 * the local package and print the conflict.
 */
static void
print_pkg_conflict(const char *pattern, const char *pkgname)
{
	char query[BUFSIZ];
	char *conflict;

	conflict = xmalloc(BUFSIZ * sizeof(char));

	sqlite3_snprintf(BUFSIZ, query, REMOTE_CONFLICTS, pattern);
	if (pkgindb_doquery(query, pdb_get_value, conflict) == PDB_OK)
		printf(MSG_CONFLICT_PKG, pkgname, conflict);

	XFREE(conflict);
}

/*
 * Check if an incoming remote package conflicts with any local packages.
 */
int
pkg_has_conflicts(Pkglist *pkg, Plistarray *conflicts)
{
	Pkglist *p;
	int i, slot;

	if (is_empty_plistarray(conflicts))
		return 0;

	slot = pkg_hash_entry(pkg->rpkg->name, conflicts->size);
	SLIST_FOREACH(p, &conflicts->head[slot], next) {
		for (i = 0; i < p->patcount; i++) {
			if (!pkg_match(p->patterns[i], pkg->rpkg->full))
				continue;

			/*
			 * Incoming remote package matches a local CONFLICTS
			 * entry, print the conflict message and return fail.
			 */
			print_pkg_conflict(p->patterns[i], pkg->rpkg->full);
			return 1;
		}
	}

	/*
	 * We also need to check slot 0 if not already checked as that's where
	 * any CONFLICTS that have a complicated pattern and no pkgbase are
	 * stored.
	 */
	if (slot == 0)
		return 0;

	SLIST_FOREACH(p, &conflicts->head[0], next) {
		for (i = 0; i < p->patcount; i++) {
			if (!pkg_match(p->patterns[i], pkg->rpkg->full))
				continue;
			print_pkg_conflict(p->patterns[i], pkg->rpkg->full);
			return 1;
		}
	}

	return 0;
}

void
show_prov_req(const char *query, const char *pkgname)
{
	const char	*out[] = { "provided", "required" };
	const char	*say;
	char		*fullpkgname;
	Plistnumbered	*plisthead;
	Pkglist		*plist;

	if ((fullpkgname = unique_pkg(pkgname, REMOTE_PKG)) == NULL)
		errx(EXIT_FAILURE, MSG_PKG_NOT_AVAIL, pkgname);

	say = (query == REMOTE_PROVIDES) ? out[0] : out[1];

	if ((plisthead = rec_pkglist(query, fullpkgname)) == NULL) {
		printf(MSG_NO_PROV_REQ, say, fullpkgname);
		exit(EXIT_SUCCESS);
	}

	printf(MSG_FILES_PROV_REQ, say, fullpkgname);
	SLIST_FOREACH(plist, plisthead->P_Plisthead, next)
		printf("\t%s\n", plist->full);

	free_pkglist(&plisthead->P_Plisthead);
	free(plisthead);
	XFREE(fullpkgname);
}
