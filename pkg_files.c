/* $Id:$ */

/*
 * Copyright (c) 2009, 2010, 2011, 2012 The NetBSD Foundation, Inc.
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

#include <regex.h>
#include <zlib.h>
#include <bzlib.h>
#include "pkgin.h"

#define PKG_FILES_GZ 0
#define PKG_FILES_BZ2 1

static const char *const files_exts[] = { "bz2", "gz", NULL };
static const int files_kinds[] = { PKG_FILES_BZ2, PKG_FILES_GZ };

static void
pkg_files_dir(const char *repo, char *dir, int dir_size)
{
	char	*esc, repo_dir[BUFSIZ];

	snprintf(repo_dir, BUFSIZ, "%s", repo);

        while ((esc = strpbrk(repo_dir, ":/")) != NULL) {
		*esc = '_';
	}

	create_dir(PKGIN_FILES_CACHE);
	snprintf(dir, dir_size, "%s/%s", PKGIN_FILES_CACHE, repo_dir);
        create_dir(dir);
} 

/**
 * Fetch remote pkg_files to local cache
 */
void
fetch_pkg_files(const char *cur_repo)
{
	/* from pkg_install/files/admin/audit.c */
	FILE		*fp = NULL;
	struct stat	st;
	Dlfile		*file = NULL;
	char		*decompressed_input;
	size_t		decompressed_len;
	time_t		files_mtime;
	int		i;
	char		**out, buf[BUFSIZ], repo_dir[BUFSIZ], buf_fs[BUFSIZ];

	pkg_files_dir(cur_repo, repo_dir, BUFSIZ);

	for (i = 0; files_exts[i] != NULL; i++) { /* try all extensions */
		snprintf(buf, BUFSIZ, "%s/%s.%s", cur_repo, PKG_FILES, files_exts[i]);
		snprintf(buf_fs, BUFSIZ, "%s/%s.%s", repo_dir, PKG_FILES, files_exts[i]);

		if (stat(buf_fs, &st) == 0)
			files_mtime = st.st_mtime;
		else
			files_mtime = 0; /* 0 files_mtime == force reload */


		if ((file = download_file(buf, &files_mtime)) != NULL)
			break; /* pkg_files found and not up-to-date */

		if (files_mtime < 0) { /* pkg_files found, but up-to-date */
			printf(MSG_PKG_FILES_IS_UP_TO_DATE, cur_repo);
			return;
		}
	}

	if (file == NULL)
		errx(EXIT_FAILURE, MSG_COULDNT_FETCH, buf);

	umask(DEF_UMASK);
	if ((fp = fopen(buf_fs, "w")) == NULL)
		err(EXIT_FAILURE, MSG_ERR_OPEN, buf_fs);

	fwrite(file->buf, file->size, 1, fp);
	fclose(fp);

	XFREE(file->buf);
	XFREE(file);
}

void *
open_pkg_files_gz(const char *path)
{
	void	*files;

	if ((files = (void *) gzopen(path, "r")) == NULL) {
		errx(1, "gzopen: %s: %s", path, strerror(errno));
	}

	return files;
}

void *
open_pkg_files_bz2(const char *path)
{
	void	*files;

	if ((files = (void *) BZ2_bzopen(path, "r")) == NULL) {
		errx(1, "bzopen: %s: %s", path, strerror(errno));
	}

	return files;
}

void *
open_pkg_files(const char *path, int kind)
{
	void	*files = NULL;

	switch (kind) {
		case PKG_FILES_GZ:
			files = open_pkg_files_gz(path);
		break;
		case PKG_FILES_BZ2:
			files = open_pkg_files_bz2(path);
		break;
		default:
			errx(1, "unknown pkg_files kind");
		break;
	}

	return files;
}

int
read_pkg_files_gz(void *fd, char *buf, int size)
{
	gzFile		*files = (gzFile *) fd;
	int		err;
	const char	*err_str;
	int		bytes_read;

	if ((bytes_read = gzread(files, buf, size)) < size) {
		if (gzeof(files)) {
			bytes_read = 0;
		} else {
			err_str = gzerror(files, &err);

			if (err) {
				errx(1, "gzread error: %s", err_str);
			}
		}
	}

	return bytes_read;
}

int
read_pkg_files_bz2(void *fd, char *buf, int size)
{
	BZFILE		*files = (BZFILE *) fd;
	int		err;
	const char	*err_str;
	int		bytes_read;

	if ((bytes_read = BZ2_bzread(files, buf, size)) < size) {
		err_str = BZ2_bzerror(files, &err);

		if (err == BZ_OK) {
			bytes_read = 0;
		} else {
			errx(1, "bzread error: %s", err_str);
		}
	}

	return bytes_read;
}

int
read_pkg_files(void *files, int kind, char *buf, int buf_size)
{
	int	rc = 0;

	switch (kind) {
		case PKG_FILES_GZ:
			rc = read_pkg_files_gz(files, buf, buf_size);
		break;
		case PKG_FILES_BZ2:
			rc = read_pkg_files_bz2(files, buf, buf_size);
		break;
		default:
			errx(1, "unknown pkg_files kind");
		break;
	}

	return rc;
}

void
close_pkg_files(void *files, int kind)
{
	switch (kind) {
		case PKG_FILES_GZ:
			gzclose((gzFile *) files);
		break;
		case PKG_FILES_BZ2:
			BZ2_bzclose((BZFILE *) files);
		break;
		default:
			errx(1, "unknown pkg_files kind");
		break;
	}
}

void
search_pkg_file_lines(regex_t *re, char *buf, int buf_size)
{
	int	i, sz;
	char	*sep = ": ";
	char	*line, *pfile;

	line = buf;
	sz = 0;

	for (i = 0; i < buf_size; i++) {
		if (buf[i] != '\n') {
			sz++;
			continue;
		}

		buf[i] = '\0';

		if ((pfile = strnstr(line, sep, sz)) == NULL) {
			/* failure here is not expected */
			return;
		}

		pfile += 2;

		if (regexec(re, pfile, 0, NULL, 0) == 0) {
			printf("%s\n", line);
		}

		line = &buf[i + 1];
	}
}

int
search_pkg_file_in_repo(const char *repo, const char *pattern)
{
	regex_t		re;
	struct stat	st;
	int		i, kind, rc, bytes_read, buf_size, left_overs;
	char		eb[64], repo_dir[BUFSIZ], buf[BUFSIZ], *repo_file, *bytes;
        int		z_buf_size = BUFSIZ * 128;
	char		*end;
	void		*files;

	pkg_files_dir(repo, repo_dir, BUFSIZ);
	repo_file = NULL;
	kind = -1;

	for (i = 0; files_exts[i] != NULL; i++) { /* try all extensions */
		snprintf(buf, BUFSIZ, "%s/%s.%s", repo_dir, PKG_FILES, files_exts[i]);

		if (stat(buf, &st) == 0) {
			repo_file = buf;
			kind = files_kinds[i];
		}
	}

	if (repo_file == NULL) {
		return 0;
	}

	if ((rc = regcomp(&re, pattern,
		REG_EXTENDED|REG_NOSUB)) != 0) {
		regerror(rc, &re, eb, sizeof(eb));
		errx(1, "regcomp: %s: %s", pattern, eb);
	}

	files = open_pkg_files(buf, kind);

	XMALLOC(bytes, z_buf_size + 1);
        buf_size = 0;
	left_overs = 0;

	while ((bytes_read = read_pkg_files(
			files, kind,
			bytes + left_overs, z_buf_size - left_overs)) != 0) {
		bytes[bytes_read + left_overs] = '\0';

		if ((end = strrchr(bytes, '\n')) == NULL) {
			/* failure not expected here */
			break;
		}

		buf_size = end - bytes + 1;
		left_overs = bytes_read - buf_size;

		search_pkg_file_lines(&re, bytes, buf_size);

		if (left_overs > 0) {
			memmove(bytes, end + 1, left_overs);
		}
	}

	close_pkg_files(files, kind);

	XFREE(bytes);

	return EXIT_SUCCESS;
}

int
search_pkg_file(const char *pattern)
{
	char	**prepos;

	/* loop through PKG_REPOS */
	for (prepos = pkg_repos; *prepos != NULL; prepos++) {
		search_pkg_file_in_repo(*prepos, pattern);
	}

	return EXIT_SUCCESS;
}

int
update_pkg_files(void)
{
	char	**prepos;

        if (!have_enough_rights())
                return EXIT_FAILURE;

	/* loop through PKG_REPOS */
	for (prepos = pkg_repos; *prepos != NULL; prepos++) {
		fetch_pkg_files(*prepos);
	}

        return EXIT_SUCCESS;
}
