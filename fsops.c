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

#include <sys/statvfs.h>
#if _FILE_OFFSET_BITS == 32
#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64 /* needed for large filesystems on sunos */
#endif
#include "pkgin.h"
#include <dirent.h>

/* Variable options for the repositories file */
static const struct VarParam {
	const char *opt;
	char *(*func)(void);
} var[] = {
	{ "$arch", getosarch },
	{ "$osrelease", getosrelease },
	{ NULL, NULL }
};

int
fs_has_room(const char *dir, int64_t size)
{
	int64_t		freesize;
	struct statvfs	fsbuf;


	if (statvfs(dir, &fsbuf) < 0)
		err(EXIT_FAILURE, "Can't statvfs() `%s'", dir);

	freesize = (int64_t)fsbuf.f_bavail  * (int64_t)fsbuf.f_frsize;

	if (freesize > size)
		return 1;

	return 0;
}

uint64_t
fs_room(const char *dir)
{
	struct statvfs	fsbuf;

	if (statvfs(dir, &fsbuf) < 0)
		err(EXIT_FAILURE, "Can't statvfs() `%s'", dir);

	return (int64_t)fsbuf.f_bavail * (int64_t)fsbuf.f_frsize;
}

void
clean_cache()
{
	DIR		*dp;
	struct dirent	*ep;
	char		pkgpath[BUFSIZ];

	if ((dp = opendir(pkgin_cache)) == NULL)
		err(EXIT_FAILURE, "couldn't open %s", pkgin_cache);

	while ((ep = readdir(dp)) != NULL)
		if (ep->d_name[0] != '.') {
			snprintf(pkgpath, BUFSIZ, "%s/%s",
				pkgin_cache, ep->d_name);
			if (unlink(pkgpath) < 0)
				err(EXIT_FAILURE,
					"could not delete %s", pkgpath);
		}
	closedir(dp);
}

char *
read_repos()
{
	FILE	*fp;
	char	*tmp, *b, *repos = NULL, buf[BUFSIZ];
	int     curlen = 0, repolen = 0;
	const struct VarParam	*vp;

	if ((fp = fopen(PKGIN_CONF"/"REPOS_FILE, "r")) == NULL)
		return NULL;

	while (!feof(fp) && !ferror(fp)) {
		memset(buf, 0, BUFSIZ);

		if (fgets(buf, BUFSIZ, fp) == NULL)
			continue;

		if (strncmp(buf, "ftp://", 6) != 0 &&
			strncmp(buf, "http://", 7) != 0 &&
			strncmp(buf, "https://", 8) != 0 &&
			strncmp(buf, "file://", 7) != 0)
			continue;

		/*
		 * Try to replace all the ocurrences with the proper
		 * system values.
		 */
		for (vp = var; vp->func != NULL; vp++) {
			if (strstr(buf, vp->opt) != NULL) {
				tmp = vp->func();
				if (tmp == NULL) {
					warn(MSG_TRANS_FAILED, vp->opt);
					continue;
				}
				if ((b = strreplace(buf, vp->opt, tmp)) != NULL)
					strncpy(buf, b, BUFSIZ);
				else
					warn(MSG_INVALID_REPOS, buf);

				XFREE(tmp);
				XFREE(b);
			}
		}

		if (trimcr(buf) < 0) /* strip newline */
			continue;
	
		curlen = strlen(buf) + 2; /* ' ' + '\0' */
		repolen += curlen;

		repos = xrealloc(repos, repolen * sizeof(char));

		if (repolen > curlen) /* more than one repo */ {
			/* add a space character */
			curlen = strlen(repos);
			repos[curlen] = ' ';
			repos[curlen + 1] = '\0';
		} else
			/* 1st entry */
			memset(repos, 0, curlen);

		strcat(repos, buf);
	}

	fclose(fp);

	return repos;
}
