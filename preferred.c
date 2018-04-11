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
 *
 */

#include "pkgin.h"

Preflist **preflist;

void
load_preferred()
{
	FILE		*fp;
	size_t		pkglen;
	int		size = 0;
	char		buf[BUFSIZ], *p;
	const char	*cmp = "=<>";

	preflist = NULL;

	if ((fp = fopen(PKGIN_CONF"/"PREF_FILE, "r")) == NULL)
		return;

	preflist = xmalloc(sizeof(Preflist *));

	while (!feof(fp)) {
		if (fgets(buf, BUFSIZ, fp) == NULL ||
				buf[0] == '\n' || buf[0] == '#')
			continue;

		trimcr(&buf[0]);

		preflist[size] = xmalloc(sizeof(Preflist));
		preflist[size]->glob = xstrdup(buf);
		preflist[size]->pkg = xstrdup(buf);
		preflist[size]->vers = NULL;
		if ((p = strpbrk(preflist[size]->pkg, cmp)) != NULL)
			trunc_str(preflist[size]->pkg, *p, STR_FORWARD);
		pkglen = strlen(preflist[size]->pkg);
		if (pkglen < strlen(preflist[size]->glob))
			preflist[size]->vers = preflist[size]->glob + pkglen;

		preflist = xrealloc(preflist, (++size + 1) * sizeof(Preflist *));
		preflist[size] = NULL;
	}

	fclose(fp);
}

void
free_preferred()
{
	Preflist **p = preflist;

	for (p = preflist; p != NULL && *p != NULL; p++) {
		XFREE((*p)->pkg);
		XFREE((*p)->glob);
		XFREE(*p);
	}
	XFREE(preflist);
}

char *
is_preferred(char *fullpkg)
{
	int	i;
	char	pkg[BUFSIZ];

	if (preflist == NULL)
		return NULL;

	XSTRCPY(pkg, fullpkg);
	trunc_str(pkg, '-', STR_BACKWARD);

	for (i = 0; preflist[i] != NULL; i++)
		if (strcmp(preflist[i]->pkg, pkg) == 0) {
			/* equality is handled by strcmp in pkg_match */
			if (	preflist[i]->vers != NULL &&
				preflist[i]->vers[0] == '=')
				preflist[i]->vers[0] = '-';

			return preflist[i]->glob;
		}

	return NULL;
}

uint8_t
chk_preferred(char *match)
{
	char *pref;

	if ((pref = is_preferred(match)) != NULL && !pkg_match(pref, match)) {
			/*
			 * package is listed in preferred.conf but the
			 * version doesn't match requirement
			 */
		printf(MSG_PKG_IS_PREFERRED, match, pref);
		return 1;
	}
	return 0;
}
