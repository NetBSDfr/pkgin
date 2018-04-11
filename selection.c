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

void
export_keep()
{
	Plistnumbered	*plisthead;
	Pkglist		*plist;

	if ((plisthead = rec_pkglist(EXPORT_KEEP_LIST)) == NULL)
		errx(EXIT_FAILURE, MSG_EMPTY_LOCAL_PKGLIST);
	/* yes we could output directly from the sql reading, but we would lose
	 * some genericity.
	 */
	SLIST_FOREACH(plist, plisthead->P_Plisthead, next)
		printf("%s\n", plist->full);

	free_pkglist(&plisthead->P_Plisthead, LIST);
	free(plisthead);
}

void
import_keep(uint8_t do_inst, const char *import_file)
{
	int		list_size = 0;
	char	**pkglist = NULL, *match;
	char	input[BUFSIZ], fullpkgname[BUFSIZ], query[BUFSIZ];
	FILE	*fp;

	if ((fp = fopen(import_file, "r")) == NULL)
		err(EXIT_FAILURE, MSG_ERR_OPEN, import_file);

	while (fgets(input, BUFSIZ, fp) != NULL) {
		if (!isalnum((int)input[0]))
			continue;

		trimcr(input);
		if (strchr(input, '/') != NULL) {
			snprintf(query, BUFSIZ, GET_PKGNAME_BY_PKGPATH, input);

			if ((pkgindb_doquery(query,
					pdb_get_value, fullpkgname)) == PDB_OK)
				match = xstrdup(fullpkgname);
			else
				match = NULL;
		} else
			match = unique_pkg(input, REMOTE_PKG);

		if (match == NULL) {
			fprintf(stderr, MSG_PKG_NOT_AVAIL, input);
			continue;
		}
		/* 1st element + NULL */
		pkglist = xrealloc(pkglist, (list_size + 2) * sizeof(char *));
		pkglist[list_size] = match;
		pkglist[++list_size] = NULL;
	}
	fclose(fp);

	if (pkglist == NULL)
		errx(EXIT_FAILURE, MSG_EMPTY_IMPORT_LIST);

	pkgin_install(pkglist, do_inst);

	free_list(pkglist);
}
