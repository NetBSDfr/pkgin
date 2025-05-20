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
#include "external/progressmeter.h"

extern char fetchflags[3];

/*
 * Open a pkg_summary and if newer than local return an open libfetch
 * connection to it.
 */
Sumfile *
sum_open(char *str_url, time_t *db_mtime)
{
	Sumfile		*sum = NULL;
	fetchIO		*f = NULL;
	struct url	*url;
	struct url_stat	st;

	url = fetchParseURL(str_url);

	if (url == NULL || (f = fetchXGet(url, &st, fetchflags)) == NULL)
		goto nofetch;

	if (st.size == -1) { /* could not obtain file size */
		*db_mtime = 0; /* not -1, don't force update */
		goto nofetch;
	}

	if (st.mtime <= *db_mtime) {
		/*
		 * -1 used to identify return type,
		 * local summary up-to-date
		 */
		*db_mtime = -1;
		goto nofetch;
	}

	*db_mtime = st.mtime;

	/* st.size is an off_t, it will be > SSIZE_MAX on 32 bits systems */
	if (sizeof(st.size) == sizeof(SSIZE_MAX) && st.size > SSIZE_MAX - 1)
		err(EXIT_FAILURE, "file is too large");

	sum = xmalloc(sizeof(Sumfile));

	sum->fd = f;
	sum->url = url;
	sum->size = st.size;
	sum->pos = 0;
	goto out;
nofetch:
	if (url)
		fetchFreeURL(url);
	if (f)
		fetchIO_close(f);
out:
	return sum;
}

/*
 * archive_read_open open callback.  As we already have an open
 * libfetch handler all we need to do is print the download messages.
 */
int
sum_start(struct archive *a, void *data)
{
	Sumfile	*sum = data;
	char	*p;

	if ((p = strrchr(sum->url->doc, '/')) != NULL)
		p++;
	else
		p = (char *)sum->url->doc; /* should not happen */

	if (parsable)
		printf("downloading %s", p);
	else {
		printf("downloading %s:   0%%", p);
		fflush(stdout);
		start_progress_meter(p, sum->size, &sum->pos);
	}

	return ARCHIVE_OK;
}

/*
 * archive_read_open read callback.  Read the next chunk of data from libfetch
 * and update the read position for the progress meter.
 */
ssize_t
sum_read(struct archive *a, void *data, const void **buf)
{
	Sumfile	*sum = data;
	ssize_t	fetched;

	*buf = sum->buf;

	fetched = fetchIO_read(sum->fd, sum->buf, sizeof(sum->buf));

	if (fetched == -1)
		errx(EXIT_FAILURE, "failure during fetch of file: %s",
		    fetchLastErrString);

	sum->pos += fetched;

	return fetched;
}

/*
 * archive_read_open close callback.  Stop the progress meter and close the
 * libfetch handler.
 */
int
sum_close(struct archive *a, void *data)
{
	Sumfile	*sum = data;

	if (parsable)
		printf(" done.\n");
	else
		stop_progress_meter();

	fetchIO_close(sum->fd);
	fetchFreeURL(sum->url);
	XFREE(sum);

	return ARCHIVE_OK;
}

/*
 * Download a package to the local cache.
 */
off_t
download_pkg(char *pkg_url, FILE *fp, int cur, int total)
{
	struct url_stat st;
	size_t size, wrote;
	ssize_t fetched;
	off_t statsize = 0, written = 0;
	struct url *url;
	fetchIO *f = NULL;
	char buf[4096];
	char *pkg, *ptr, *msg = NULL;

	if ((url = fetchParseURL(pkg_url)) == NULL)
		errx(EXIT_FAILURE, "%s: parse failure", pkg_url);

	if ((f = fetchXGet(url, &st, fetchflags)) == NULL) {
		fprintf(stderr, "download error: %s %s\n", pkg_url,
		    fetchLastErrString);
		fetchFreeURL(url);
		return -1;
	}
	fetchFreeURL(url);

	if ((pkg = strrchr(pkg_url, '/')) != NULL)
		pkg++;
	else
		pkg = (char *)pkg_url; /* should not happen */

	if (parsable) {
		printf("[%d/%d] downloading %s", cur, total, pkg);
	} else {
		msg = xasprintf("[%d/%d] %s", cur, total, pkg);
		fflush(stdout);
		start_progress_meter(msg, st.size, &statsize);
	}

	while (written < st.size) {
		if ((fetched = fetchIO_read(f, buf, sizeof(buf))) == 0)
			break;
		if (fetched < 0 && errno == EINTR)
			continue;
		if (fetched < 0) {
			fprintf(stderr, "download error: %s",
			    fetchLastErrString);
			return -1;
		}

		statsize += fetched;
		size = (size_t)fetched;

		for (ptr = buf; size > 0; ptr += wrote, size -= wrote) {
			if ((wrote = fwrite(ptr, 1, size, fp)) < size) {
				if (ferror(fp) && errno == EINTR)
					clearerr(fp);
				else
					break;
			}
			written += (off_t)wrote;
		}
	}

	if (parsable)
		printf(" done.\n");
	else {
		stop_progress_meter();
		free(msg);
	}

	fetchIO_close(f);

	if (written != st.size) {
		fprintf(stderr, "download error: %s truncated\n", pkg_url);
		return -1;
	}

	return written;
}
