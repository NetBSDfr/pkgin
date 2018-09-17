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

static Preflisthead prefhead;

void
load_preferred(void)
{
	FILE		*fp;
	Preflist	*pref;
	size_t		len = 0;
	ssize_t		llen;
	char		*line = NULL, *p;
	const char	*cmp = "=<>";

	if ((fp = fopen(PKGIN_CONF"/"PREF_FILE, "r")) == NULL)
		return;

	SLIST_INIT(&prefhead);

	while ((llen = getline(&line, &len, fp)) > 0) {
		if (line[0] == '\n' || line[0] == '#')
			continue;

		if ((p = strpbrk(line, cmp)) == NULL)
			continue;

		trimcr(line);

		/*
		 * The preferred.conf syntax for equality uses "=" to separate
		 * the package name and version (e.g. "foo=1.*").  This needs
		 * to be converted to the "foo-1.*" form for pkg_match().
		 */
		if (*p == '=')
			*p = '-';

		pref = xmalloc(sizeof(Preflist));
		pref->glob = xstrdup(line);
		*p = '\0';
		pref->pkg = xstrdup(line);
		SLIST_INSERT_HEAD(&prefhead, pref, next);
	}

	fclose(fp);
}

void
free_preferred(void)
{
	Preflist *pref;

	while (!SLIST_EMPTY(&prefhead)) {
		pref = SLIST_FIRST(&prefhead);
		SLIST_REMOVE_HEAD(&prefhead, next);
		free(pref->pkg);
		free(pref->glob);
		free(pref);
	}
}

static char *
is_preferred(char *fullpkg)
{
	Preflist *pref;
	char pkg[BUFSIZ];

	XSTRCPY(pkg, fullpkg);
	trunc_str(pkg, '-', STR_BACKWARD);

	SLIST_FOREACH(pref, &prefhead, next) {
		if (strcmp(pref->pkg, pkg) == 0)
			return pref->glob;
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
