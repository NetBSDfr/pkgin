/* $Id: tools.c,v 1.5 2012/08/04 14:23:46 imilh Exp $ */

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

#include "tools.h"

int
charcount(char *str, char c)
{
	char	*p;
	int		count = 0;

	if (str == NULL)
		return 0;

	for (p = str; *p != '\0'; p++)
		if (*p == c)
			count++;

	return count;
}

__inline int
trimcr(char *str)
{
	uint16_t len;

	if (str == NULL)
		return (-1);

	len = strlen(str);

	while (len--)
		if ((str[len] == '\r') || (str[len] == '\n'))
			str[len] = '\0';

	return (0);
}

char **
splitstr(char *str, const char *sep)
{
	int i, size;
	char *p, *tmp, **split;

	for (i = 0, size = 0; str[i] != '\0'; i++)
		if (str[i] == *sep)
			size++;

	/* size = number of separators + 1 member + NULL */
	size += 2;
	split = xmalloc(size * sizeof(char *));

	i = 0;
	for (p = str; p != NULL;)
		while ((tmp = strsep(&p, sep)) != NULL) {
			if (*tmp != '\0') {
				while (*tmp == ' ' || *tmp == '\t')
					tmp++;
				split[i] = xstrdup(tmp);
				i++;
			}
		}

	split[i] = NULL;

	return(split);
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

void
trunc_str(char *str, char limit, int direction)
{
	char *p;

	switch(direction) {
	case STR_FORWARD:
		if ((p = strchr(str, limit)) != NULL)
			*p = '\0';
		break;
	case STR_BACKWARD:
		if ((p = strrchr(str, limit)) != NULL)
			*p = '\0';
		break;
	}
}

char *
safe_snprintf(int size, const char *fmt, ...)
{
	char *p;
	va_list ap;

	p = xmalloc(size * sizeof(char));
	va_start(ap, fmt);
	(void) vsnprintf(p, size, fmt, ap);
	va_end(ap);

	return p;
}

int
safe_strcmp(const char *s1, const char *s2)
{
	if (s1 == NULL)
		return -1;
	if (s2 == NULL)
		return 1;
	return strcmp(s1, s2);
}

__inline int
max(int a, int b)
{
        return (a > b ? a : b);
}
__inline int
min(int a, int b)
{
        return (a < b ? a : b);
}

int
listlen(const char **list)
{
	int i;

	for (i = 0; list[i] != NULL; i++);
	return(i);
}

/* execute a command and receive result on a char ** */
char **
exec_list(const char *cmd, const char *match)
{
	FILE *fp;
	int size;
	char **res, *rawlist, buf[MAXLEN];

	if ((fp = popen(cmd, "r")) == NULL)
		return(NULL);

	rawlist = NULL;
	size = 0;

	while (fgets(buf, MAXLEN, fp) != NULL) {
		if (match == NULL || strstr(buf, match) != NULL) {
			size += (strlen(buf) + 1) * sizeof(char);

			rawlist = xrealloc(rawlist,  size);
			strlcat(rawlist, buf, size);
		}
	}
	pclose(fp);

	if (rawlist == NULL)
		return(NULL);

	res = splitstr(rawlist, "\n");
	XFREE(rawlist);

	return(res);
}

uint8_t
is_listed(const char **list, const char *item)
{
	if (list != NULL)
		for (; *list != NULL; list++)
			if (strcmp(item, *list) == 0)
				return(TRUE);

	return(FALSE);
}

void
do_log(const char *path, const char *fmt, ...)
{
	FILE *fp;
	char buffer[MAXLEN];

	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, MAXLEN, fmt, args);

	fp = fopen(path, "a");
	fputs(buffer, fp);
	fclose(fp);

	va_end(args);
}

/* Return architecture name or NULL in case of failure */
char *
getosarch(void)
{
	char			*ret;
	struct utsname	un;

	memset(&un, 0, sizeof(un));
	if (uname(&un) < 0)
		return NULL;

	ret = xstrdup(un.machine);

	return ret;
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

#ifdef _MINIX
	asprintf(&ret, "%s.%s", un.release, un.version);
#else
	ret = xstrdup(un.release);
#endif

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
	int		fromlen, tolen, i;
	char	*p, *ret, buf[MAXLEN];

	memset(buf, 0, MAXLEN);

	fromlen = strlen(from);
	tolen = strlen(to);

	for (i = 0, p = str; *p != '\0';) {
		if (strncmp(p, from, fromlen) == 0) {
			strncat(buf, to, tolen);
			p += fromlen;
			i += tolen;
		} else {
			buf[i] = *p;
			p++;
			i++;
		}
	}
	buf[i] = '\0';

	ret = xstrdup(buf);
	return(ret);
}

int
check_yesno(uint8_t default_answer)
{
	const struct Answer	{
		const uint8_t	numval;
		const char		charval;
	} answer[] = { { ANSW_NO, 'n' }, { ANSW_YES, 'y' } };

	uint8_t	r, reverse_answer;
	int		c;

	if (yesflag)
		return ANSW_YES;
	else if (noflag)
		return ANSW_NO;

	/* reverse answer is default's answer opposite (you don't say!) */
	reverse_answer = (default_answer == ANSW_YES) ? ANSW_NO : ANSW_YES;

	if (default_answer == answer[ANSW_YES].numval)
		printf(MSG_PROCEED_YES);
	else
		printf(MSG_PROCEED_NO);

	c = tolower(getchar());
	
	/* default answer */
	if (c == answer[default_answer].charval || c == '\n')
		r = answer[default_answer].numval;
	/* reverse answer */
	else if (c == answer[reverse_answer].charval)
		r = answer[reverse_answer].numval;
	/* bad key was given, default to No */
	else
		r = ANSW_NO;
	
	/* avoid residual char */
	if (c != '\n')
		while((c = getchar()) != '\n' && c != EOF)
			continue;

	return r;
}
