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

	if (str[len - 1] == '\n')
		str[--len] = '\0';

	if (str[len - 1] == '\r')
		str[--len] = '\0';

	return (len);
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

__attribute__((__format__ (__printf__, 2, 3)))
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
#if defined(HAVE_SYS_SYSCTL_H) && defined(HW_MACHINE_ARCH)
	char	machine_arch[32];
	int	mib[2];
	size_t	len;

	len = sizeof(machine_arch);
	mib[0] = CTL_HW;
	mib[1] = HW_MACHINE_ARCH;

	if (sysctl(mib, 2, machine_arch, &len, NULL, 0) != -1)
		return xstrdup(machine_arch);
#endif

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
	size_t	fromlen, tolen, i;
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
	fflush(stdout);

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
