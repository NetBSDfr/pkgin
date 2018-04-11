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

#include "pkgin.h"

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
		/* retreive requires list for package */
		if ((requireshead = rec_pkglist(GET_REQUIRES_QUERY,
					pimpact->full)) == NULL)
			/* empty requires list (very unlikely) */
			continue;

		/* parse requires list */
		SLIST_FOREACH(requires, requireshead->P_Plisthead, next) {

#ifdef CHECK_PROVIDES
			foundreq = 0;
#endif

			/* for performance sake, first check basesys */
			if ((strncmp(requires->full, LOCALBASE,
				    strlen(LOCALBASE) - 1)) != 0) {
				if (stat(requires->full, &sb) < 0) {
					printf(MSG_REQT_NOT_PRESENT,
						requires->full, pimpact->full);

					/*
					 * mark as DONOTHING,
					 * requirement missing
					 */
					pimpact->action = UNMET_REQ;

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
						rec_pkglist(GET_PROVIDES_QUERY,
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
					free_pkglist(&r_provideshead->P_Plisthead, LIST);
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
			 * every REQUIRES matching LOCALBASE, which is hardcoded
			 * to "/usr/pkg"
			 */
			if (!foundreq) {
				printf(MSG_REQT_NOT_PRESENT_DEPS,
					requires->full);

				foundreq = 1;
			}
#endif
		} /* SLIST_FOREACH requires */
		free_pkglist(&requireshead->P_Plisthead, LIST);
		free(requireshead);
	} /* 1st impact SLIST_FOREACH */

#ifdef CHECK_PROVIDES
	free_pkglist(&l_provideshead->P_Plisthead, LIST);
	free(l_provideshead);
#endif

	return met_reqs;
}

/* check for conflicts and if needed files are present */
int
pkg_has_conflicts(Pkglist *pimpact)
{
	int			has_conflicts = 0;
	char		*conflict_pkg, query[BUFSIZ];
	Pkglist		*conflicts; /* SLIST conflicts pointer */
	Plistnumbered	*conflictshead;

	/* conflicts list */
	if ((conflictshead = rec_pkglist(LOCAL_CONFLICTS)) == NULL)
		return 0;

	/* check conflicts */
	SLIST_FOREACH(conflicts, conflictshead->P_Plisthead, next) {
		if (pkg_match(conflicts->full, pimpact->full)) {

			/* got a conflict, retrieve conflicting local package */
			snprintf(query, BUFSIZ,
				GET_CONFLICT_QUERY, conflicts->full);

			conflict_pkg = xmalloc(BUFSIZ * sizeof(char));
			if (pkgindb_doquery(query,
					pdb_get_value, conflict_pkg) == PDB_OK)

				printf(MSG_CONFLICT_PKG,
					pimpact->full, conflict_pkg);

			XFREE(conflict_pkg);

			has_conflicts = 1;
		} /* match conflict */
	} /* SLIST_FOREACH conflicts */

	free_pkglist(&conflictshead->P_Plisthead, LIST);
	free(conflictshead);

	return has_conflicts;
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

	say = ( query == GET_PROVIDES_QUERY ) ? out[0] : out[1];

	if ((plisthead = rec_pkglist(query, fullpkgname)) == NULL) {
		printf(MSG_NO_PROV_REQ, say, fullpkgname);
		exit(EXIT_SUCCESS);
	}

	printf(MSG_FILES_PROV_REQ, say, fullpkgname);
	SLIST_FOREACH(plist, plisthead->P_Plisthead, next)
		printf("\t%s\n", plist->full);

	XFREE(fullpkgname);
}
