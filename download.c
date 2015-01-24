/* $Id: download.c,v 1.15 2012/07/15 17:36:34 imilh Exp $ */

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
#include "progressmeter.h"

int	fetchTimeout = 15; /* wait 15 seconds before timeout */
size_t	fetch_buffer = 1024;

/* if db_mtime == NULL, we're downloading a package, pkg_summary otherwise */
Dlfile *
download_file(char *str_url, time_t *db_mtime)
{
	/* from pkg_install/files/admin/audit.c */
	Dlfile			*file;
	char			*p;
	size_t			buf_len, buf_fetched;
	ssize_t			cur_fetched;
	off_t			statsize;
	struct url_stat		st;
	struct url		*url;
	fetchIO			*f = NULL;

	url = fetchParseURL(str_url);

	if (url == NULL || (f = fetchXGet(url, &st, "")) == NULL)
		return NULL;

	if (st.size == -1) { /* could not obtain file size */
		if (db_mtime != NULL) /* we're downloading pkg_summary */
			*db_mtime = 0; /* not -1, don't force update */

		return NULL;
	}

	if (db_mtime != NULL) {
		if (st.mtime <= *db_mtime) {
			/*
			 * -1 used to identify return type,
			 * local summary up-to-date
			 */
			*db_mtime = -1;

			fetchIO_close(f);

			return NULL;
		}

		*db_mtime = st.mtime;
	}


	if ((p = strrchr(str_url, '/')) != NULL)
		p++;
	else
		p = (char *)str_url; /* should not happen */

#ifndef _MINIX /* XXX: SSIZE_MAX fails under MINIX */
	/* st.size is an off_t, it will be > SSIZE_MAX on 32 bits systems */
	if (sizeof(st.size) == sizeof(SSIZE_MAX) && st.size > SSIZE_MAX - 1)
		err(EXIT_FAILURE, "file is too large");
#endif

	buf_len = st.size;
	XMALLOC(file, sizeof(Dlfile));
	XMALLOC(file->buf, buf_len + 1);

	buf_fetched = 0;
	statsize = 0;

	if (!parsable) { /* human readable output */
		printf(MSG_DOWNLOADING, p);
		fflush(stdout);

		start_progress_meter(p, buf_len, &statsize);
	} else
		printf(MSG_DOWNLOAD_START);

	while (buf_fetched < buf_len) {
		cur_fetched = fetchIO_read(f, file->buf + buf_fetched,
						fetch_buffer);
		if (cur_fetched == 0)
			errx(EXIT_FAILURE, "truncated file");
		else if (cur_fetched == -1)
			errx(EXIT_FAILURE, "failure during fetch of file: %s",
				fetchLastErrString);

		buf_fetched += cur_fetched;
		statsize += cur_fetched;
	}

	if (!parsable)
		stop_progress_meter();
	else
		printf(MSG_DOWNLOAD_END);

	file->buf[buf_len] = '\0';
	file->size = buf_len;

	if (file->buf[0] == '\0')
		errx(EXIT_FAILURE, "empty download, exiting.\n");


	fetchIO_close(f);

	return file;
}
