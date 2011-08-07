/* $Id: download.c,v 1.4 2011/08/07 16:28:14 imilh Exp $ */

/*
 * Copyright (c) 2009, 2010 The NetBSD Foundation, Inc.
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

/* Nasty workaround for buggy libfetch */
int fetchTimeout = 2;

Dlfile *
download_file(fetchIO *f, char *url, time_t *db_mtime)
{
	/* from pkg_install/files/admin/audit.c */
	Dlfile			*file;
	char			*p;
	char			sz[8];
	size_t			buf_len, buf_fetched;
	ssize_t			cur_fetched;
	time_t			begin_dl, now;
	struct url_stat	st;
	int				retry = 3;

	if ((fetchStatURL(url, &st, "") < 0) || st.size == -1) {
		if (db_mtime != NULL)
			*db_mtime = 0;

		return NULL;
	}

	if (db_mtime != NULL) {
		if (st.mtime <= *db_mtime) {
			/* local db is up-to-date */
			*db_mtime = -1; /* used to identify return type */

			return NULL;
		}

		*db_mtime = st.mtime;
	}

	while (f == NULL) {
		f = fetchXGetURL(url, &st, "");
		if (f == NULL) {
			fprintf(stderr, "%s: fetch failure (%s)%s\n",
				getprogname(), fetchLastErrString,
				retry == 1 ? "" : ", retrying");
			sleep(1);
			--retry;
		}
		if (retry == 0)
			errx(EXIT_FAILURE, "could not fetch url: %s: %s",
				url, fetchLastErrString);
	}


	if ((p = strrchr(url, '/')) != NULL)
		p++;
	else
		p = (char *)url; /* should not happen */

#ifndef _MINIX /* XXX: SSIZE_MAX fails under MINIX */
	/* st.size is an off_t, it will be > SSIZE_MAX on 32 bits systems */
	if (sizeof(st.size) == sizeof(SSIZE_MAX) && st.size > SSIZE_MAX - 1)
		err(EXIT_FAILURE, "file is too large");
#endif

	buf_len = st.size;
	XMALLOC(file, sizeof(Dlfile));
	XMALLOC(file->buf, buf_len + 1);

	printf(MSG_DOWNLOADING, p);
	fflush(stdout);

	buf_fetched = 0;
	begin_dl = time(NULL);

	while (buf_fetched < buf_len) {
		cur_fetched = fetchIO_read(f, file->buf + buf_fetched,
			buf_len - buf_fetched);
		if (cur_fetched == 0)
			errx(EXIT_FAILURE, "truncated file");
		else if (cur_fetched == -1)
			errx(EXIT_FAILURE, "failure during fetch of file: %s",
				fetchLastErrString);

		buf_fetched += cur_fetched;
		now = time(NULL);

		if ((now - begin_dl) > 0)
			humanize_number(sz, 8, (int64_t)(buf_fetched / (now - begin_dl)),
				"bps", HN_AUTOSCALE, HN_B | HN_DECIMAL | HN_NOSPACE);
		else
			humanize_number(sz, 8, (int64_t)buf_fetched,
				"bps", HN_AUTOSCALE, HN_B | HN_DECIMAL | HN_NOSPACE);

		printf(MSG_DOWNLOADING_PCT, p, sz,
			(int)(((float)buf_fetched / (float)buf_len) * 100));

		fflush(stdout);
	}


	file->buf[buf_len] = '\0';
	file->size = buf_len;

	if (file->buf[0] == '\0')
		errx(EXIT_FAILURE, "empty download, exiting.\n");

	printf("\n");

	fetchIO_close(f);

	return file;
}
