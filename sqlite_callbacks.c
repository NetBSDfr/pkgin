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

/** 
 * SQLite callback, record package list
 */
int
pdb_rec_list(void *param, int argc, char **argv, char **colname)
{
	Pkglist	   	*plist;
	Plistnumbered	*plisthead = (Plistnumbered *)param;
	int		i;

	if (argv == NULL)
		return PDB_ERR;

	/* FULLPKGNAME was empty, probably a package installed
	 * from pkgsrc or wip that does not exist in
	 * pkg_summary(5), return
	 */
	if (argv[0] == NULL)
		return PDB_ERR;

	plist = malloc_pkglist();

	/*
	 * rec_pkglist is used for convenience for REQUIRES / PROVIDES
	 * otherwise contains FULLPKGNAME
	 */
	plist->full = xstrdup(argv[0]);

	for (i = 1; i < argc; i++) {
		if (argv[i] == NULL)
			continue;

		if (strcmp(colname[i], "PKGNAME") == 0) {
			plist->name = xstrdup(argv[i]);
			continue;
		}
		if (strcmp(colname[i], "PKGVERS") == 0) {
			plist->version = xstrdup(argv[i]);
			continue;
		}
		if (strcmp(colname[i], "BUILD_DATE") == 0) {
			plist->build_date = xstrdup(argv[i]);
			continue;
		}
		if (strcmp(colname[i], "COMMENT") == 0) {
			plist->comment = xstrdup(argv[i]);
			continue;
		}
		if (strcmp(colname[i], "PKGPATH") == 0) {
			plist->pkgpath = xstrdup(argv[i]);
			continue;
		}
		if (strcmp(colname[i], "CATEGORIES") == 0) {
			plist->category = xstrdup(argv[i]);
			continue;
		}
		if (strcmp(colname[i], "FILE_SIZE") == 0) {
			plist->file_size = strtol(argv[i], (char **)NULL, 10);
			continue;
		}
		if (strcmp(colname[i], "SIZE_PKG") == 0) {
			plist->size_pkg = strtol(argv[i], (char **)NULL, 10);
			continue;
		}
	}

	SLIST_INSERT_HEAD(plisthead->P_Plisthead, plist, next);
	plisthead->P_count++;

	return PDB_OK;
}
