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

/*
 * SQLite callback for LOCAL_CONFLICTS etc, record a pattern and optional
 * PKGBASE to a Plistarray.
 *
 * argv0: pattern
 * argv1: pkgbase, may be NULL if it cannot be determined from pattern
 */
static int
record_pattern_to_array(void *param, int argc, char **argv, char **colname)
{
	Plistarray *depends = (Plistarray *)param;
	Pkglist *d;
	int slot;

	if (argv == NULL)
		return PDB_ERR;

	d = malloc_pkglist();
	d->patterns = xmalloc(2 * sizeof(char *));
	d->patterns[0] = xstrdup(argv[0]);
	d->patterns[1] = NULL;
	d->patcount = 1;

	/*
	 * XXX: default slot if no pkgbase available, should we allocate one
	 * outside of the normal range for these?
	 */
	slot = 0;

	if (argv[1]) {
		d->name = xstrdup(argv[1]);
		slot = pkg_hash_entry(d->name, depends->size);
	}

	SLIST_INSERT_HEAD(&depends->head[slot], d, next);

	return PDB_OK;
}

void
get_conflicts(Plistarray *conflicts)
{
	pkgindb_doquery(LOCAL_CONFLICTS, record_pattern_to_array, conflicts);
}

/* find required files (REQUIRES) from PROVIDES or filename */
int
pkg_met_reqs(Plisthead *impacthead)
{
	int		met_reqs = 1;
	Pkglist		*pimpact, *requires;
	Plistnumbered	*requireshead = NULL;
	struct stat	sb;
#ifdef CHECK_PROVIDES
	int		foundreq;
	Pkglist		*impactprov = NULL, *provides = NULL;
	Plistnumbered	*l_provideshead = NULL, *r_provideshead = NULL;

	l_provideshead = rec_pkglist(LOCAL_PROVIDES);
#endif

	/* first, parse impact list */
	SLIST_FOREACH(pimpact, impacthead, next) {
		/* Only check incoming packages */
		if (!action_is_install(pimpact->action))
			continue;

		/* retreive requires list for package */
		if ((requireshead = rec_pkglist(REMOTE_REQUIRES,
					pimpact->rpkg->full)) == NULL)
			/* empty requires list (very unlikely) */
			continue;

		/* parse requires list */
		SLIST_FOREACH(requires, requireshead->P_Plisthead, next) {

#ifdef CHECK_PROVIDES
			foundreq = 0;
#endif

			/* for performance sake, first check basesys */
			if ((strncmp(requires->full, PREFIX,
				    strlen(PREFIX) - 1)) != 0) {
				if (stat(requires->full, &sb) < 0) {
					printf(MSG_REQT_NOT_PRESENT,
						requires->full, pimpact->rpkg->full);

					/*
					 * mark as DONOTHING,
					 * requirement missing
					 */
					pimpact->action = ACTION_UNMET_REQ;

					met_reqs = 0;
				}
				/* was a basysfile, no need to check PROVIDES */
				continue;
			}
			/*
			 * FIXME: the code below actually works, but there's no
			 * point losing performances when some REQUIRES do not
			 * match PROVIDES in pkg_summary(5). This is a known
			 * issue and will hopefuly be fixed.
			 */
#ifndef CHECK_PROVIDES
			continue;
#else
			/* search what local packages provide */
			SLIST_FOREACH(provides, l_provideshead->P_Plisthead,
					next) {
				if (strncmp(provides->full,
						requires->full,
						strlen(requires->full)) == 0) {

					foundreq = 1;

					/* found, no need to go further*/
					break;
				} /* match */
			} /* SLIST_FOREACH LOCAL_PROVIDES */

			/*
			 * REQUIRES was not found on local packages,
			 * try impact list
			 */
			if (!foundreq) {
				/* re-parse impact list to retreive PROVIDES */
				SLIST_FOREACH(impactprov, impacthead, next) {
					if ((r_provideshead =
						rec_pkglist(REMOTE_PROVIDES,
						impactprov->full)) == NULL)
						continue;

					/*
					 * then parse provides list for
					 * every package
					 */
					SLIST_FOREACH(provides,
					r_provideshead->P_Plisthead, next) {
						if (strncmp(provides->full,
						requires->full,
						strlen(requires->full)) == 0) {

							foundreq = 1;

							/*
							 * found, no need to
							 * go further return
							 * to impactprov list
							 */
							break;
						} /* match */
					}
					free_pkglist(&r_provideshead->P_Plisthead);
					free(r_provideshead);

					if (foundreq)
					/* exit impactprov list loop */
						break;

				} /* SLIST_NEXT impactprov */

			} /* if (!foundreq) LOCAL_PROVIDES -> impact list */

			/* FIXME: BIG FAT DISCLAIMER
			 * as of 04/2009, some packages described in pkg_summary
			 * have unmet REQUIRES. This is a known bug that makes
			 * the PROVIDES untrustable and some packages
			 * uninstallable. foundreq is forced to 1 for now for
			 * every REQUIRES matching PREFIX.
			 */
			if (!foundreq) {
				printf(MSG_REQT_NOT_PRESENT_DEPS,
					requires->full);

				foundreq = 1;
			}
#endif
		} /* SLIST_FOREACH requires */
		free_pkglist(&requireshead->P_Plisthead);
		free(requireshead);
	} /* 1st impact SLIST_FOREACH */

#ifdef CHECK_PROVIDES
	free_pkglist(&l_provideshead->P_Plisthead);
	free(l_provideshead);
#endif

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
