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
show_pkg_info(char flag, char *pkgname)
{
	int		i;
	char	cmd[BUFSIZ], *fullpkgname, **prepos, **out_cmd = NULL;

	if ((fullpkgname = unique_pkg(pkgname, REMOTE_PKG)) == NULL)
		errx(EXIT_FAILURE, MSG_PKG_NOT_AVAIL, pkgname);	

	/* loop through PKG_REPOS */
	for (prepos = pkg_repos; *prepos != NULL; prepos++) {
		snprintf(cmd, BUFSIZ, "%s -%c %s/%s%s",
		    pkg_info, flag, *prepos, fullpkgname, PKG_EXT);

		if ((out_cmd = exec_list(cmd, NULL)) == NULL)
			continue;

		for (i = 0; out_cmd[i] != NULL; i++)	
			printf("%s\n", out_cmd[i]);

		free_list(out_cmd);
	}

	XFREE(fullpkgname);

	return;
}
