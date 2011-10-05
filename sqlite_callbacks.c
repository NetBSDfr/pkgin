/* $Id: sqlite_callbacks.c,v 1.6 2011/10/05 21:32:37 imilh Exp $ */

/*
 * Copyright (c) 2009, 2010, 2011 The NetBSD Foundation, Inc.
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

#include "pkgin.h"

/* SQLite results columns */
#define FULLPKGNAME	argv[0]
#define PKGNAME		argv[1]
#define PKGVERS		argv[2]
#define COMMENT		argv[3]
#define FILE_SIZE	argv[4]
#define SIZE_PKG	argv[5]
/** 
 * SQLite callback, record package list
 */
int
pdb_rec_list(void *param, int argc, char **argv, char **colname)
{
	Pkglist	   	*plist;
	Plisthead 	*plisthead = (Plisthead *)param;

	if (argv == NULL)
		return PDB_ERR;

	/* FULLPKGNAME was empty, probably a package installed
	 * from pkgsrc or wip that does not exist in
	 * pkg_summary(5), return
	 */
	if (FULLPKGNAME == NULL)
		return PDB_ERR;

	plist = malloc_pkglist(LIST);
	XSTRDUP(plist->full, FULLPKGNAME);

	/* it's convenient to have package name without version (autoremove.c) */
	if (argc > 1)
		XSTRDUP(plist->name, PKGNAME);

	plist->size_pkg = 0;
	plist->file_size = 0;

	/* classic pkglist, has COMMENT and SIZEs */
	if (argc > 2) {
		if (COMMENT == NULL) {
			/* COMMENT or SIZEs were empty
			 * not a valid pkg_summary(5) entry, return
			 */
			XFREE(plist->full);
			XFREE(plist);
			return PDB_OK;
		}

		XSTRDUP(plist->version, PKGVERS);
		XSTRDUP(plist->comment, COMMENT);
		if (FILE_SIZE != NULL)
			plist->file_size = strtol(FILE_SIZE, (char **)NULL, 10);
		if (SIZE_PKG != NULL)
			plist->size_pkg = strtol(SIZE_PKG, (char **)NULL, 10);

	} else
		/* conflicts or requires list, only pkgname needed */
		plist->comment = NULL;

	SLIST_INSERT_HEAD(plisthead, plist, next);

	return PDB_OK;
}

/* dewey for direct deps, full pkgname for reverse deps*/
#define DEPS_FULLPKG		argv[0]
/* pkgname for direct deps and reverse deps */
#define DEPS_PKGNAME		argv[1]
#define PKG_KEEP			argv[2]
/**
 * sqlite callback
 * DIRECT_DEPS or REVERSE_DEPS result, feeds a Pkglist SLIST
 * Plisthead is the head of Pkglist
 */
int
pdb_rec_depends(void *param, int argc, char **argv, char **colname)
{
	Pkglist		*deptree, *pdp, *pkg_map;
	Plisthead	*pdphead = (Plisthead *)param, *plisthead;

	if (argv == NULL)
		return PDB_ERR;

	/* check if dependency is already recorded, do not insert on list  */
	SLIST_FOREACH(pdp, pdphead, next)
		if (strcmp(DEPS_PKGNAME, pdp->name) == 0) {
			TRACE(" < dependency %s already recorded\n", pdp->name);
			/* proceed to next result */
			return PDB_OK;
		}

	deptree = malloc_pkglist(DEPTREE);
	XSTRDUP(deptree->depend, DEPS_FULLPKG);

	/* check wether we're getting local or remote dependencies */
	if (strncmp(colname[0], "LOCAL_", 6) == 0)
		plisthead = &l_plisthead;
	else
		plisthead = &r_plisthead;

	/* map corresponding pkgname */
	if ((pkg_map = map_pkg_to_dep(plisthead, deptree->depend)) != NULL)
		XSTRDUP(deptree->name, pkg_map->name);
	else
		/* some dependencies just don't match anything */
		XSTRDUP(deptree->name, DEPS_PKGNAME);

	deptree->computed = 0;
	deptree->level = 0;
	/* used in LOCAL_REVERSE_DEPS / autoremove.c */
	if (argc > 2 && PKG_KEEP != NULL)
		deptree->keep = 1;
	else
		deptree->keep = 0;

	SLIST_INSERT_HEAD(pdphead, deptree, next);

	return PDB_OK;
}
