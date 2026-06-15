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

#include "tools.h"

/*
 * Remove trailing \n or \r\n, returning length of resulting string.
 */
size_t
trimcr(char *str)
{
	size_t len;

	if (str == NULL)
		return (0);

	len = strlen(str);

	if (len > 0 && str[len - 1] == '\n')
		str[--len] = '\0';

	if (len > 0 && str[len - 1] == '\r')
		str[--len] = '\0';

	return (len);
}

/*
 * Format a size in human-readable form into buf, which must be at least
 * H_BUF bytes.
 */
void
humanize_size(char *buf, int64_t size)
{
	(void)humanize_number(buf, H_BUF, size, "", HN_AUTOSCALE,
	    HN_B | HN_NOSPACE | HN_DECIMAL);
}

void
free_list(char **list)
{
	int i;

	if (list != NULL) {
		for (i = 0; list[i] != NULL; i++)
			XFREE(list[i]);
		XFREE(list);
	}
}

/* Return architecture name or NULL in case of failure */
char *
getosarch(void)
{
	return xstrdup(MACHINE_ARCH);
}

/* Return release numbers or NULL in case of failure */
char *
getosrelease(void)
{
	struct utsname	un;
	char			*ret, *p;

	memset(&un, 0, sizeof(un));
	if (uname(&un) < 0)
		return NULL;

	ret = xstrdup(un.release);

	for (p = ret; isdigit((int)*p) || *p == '.'; p++);
	*p = '\0';

	return ret;
}

/*
 * Find all coincidences found in big string that match oldsub
 * substring and replace them with newsub.
 * Returns an allocated buffer that needs to be freed later.
 */
char *
strreplace(char *str, const char *from, const char *to)
{
	size_t	fromlen, tolen;
	char	buf[MAXLEN], *dst, *m;

	fromlen = strlen(from);
	tolen = strlen(to);
	dst = buf;

	while ((m = strstr(str, from)) != NULL) {
		memcpy(dst, str, m - str);
		dst += m - str;
		memcpy(dst, to, tolen);
		dst += tolen;
		str = m + fromlen;
	}
	strcpy(dst, str);

	return xstrdup(buf);
}

int
check_yesno(uint8_t default_answer)
{
	int c, r;

	if (yesflag)
		return ANSW_YES;
	if (noflag)
		return ANSW_NO;

	printf(default_answer == ANSW_YES ? MSG_PROCEED_YES : MSG_PROCEED_NO);
	fflush(stdout);

	c = tolower(getchar());

	if (c == '\n')
		r = default_answer;
	else if (c == 'y')
		r = ANSW_YES;
	else
		r = ANSW_NO;

	/* avoid residual char */
	if (c != '\n')
		while ((c = getchar()) != '\n' && c != EOF)
			continue;

	return r;
}
