/* NetBSD: opattern.c,v 1.6 2012/01/28 12:33:05 joerg Exp */

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Miscellaneous string utilities.
 *
 */

#include "lib.h"
#include "dewey.h"

/*
 * Perform alternate match on "pkg" against "pattern",
 * calling pkg_match (recursively) to resolve any other patterns.
 * Return 1 on match, 0 otherwise
 */
static int
alternate_match(const char *pattern, const char *pkg)
{
	char   *sep;
	char    buf[MaxPathSize];
	char   *last;
	char   *alt;
	char   *cp;
	int     cnt;
	int     found;

	if ((sep = strchr(pattern, '{')) == (char *) NULL) {
		errx(EXIT_FAILURE, "alternate_match(): '{' expected in `%s'", pattern);
	}
	(void) strncpy(buf, pattern, (size_t) (sep - pattern));
	alt = &buf[sep - pattern];
	last = (char *) NULL;
	for (cnt = 0, cp = sep; *cp && last == (char *) NULL; cp++) {
		if (*cp == '{') {
			cnt++;
		} else if (*cp == '}' && --cnt == 0 && last == (char *) NULL) {
			last = cp + 1;
		}
	}
	if (cnt != 0) {
		errx(EXIT_FAILURE, "Malformed alternate `%s'", pattern);
	}
	for (found = 0, cp = sep + 1; *sep != '}'; cp = sep + 1) {
		for (cnt = 0, sep = cp; cnt > 0 || (cnt == 0 && *sep != '}' && *sep != ','); sep++) {
			if (*sep == '{') {
				cnt++;
			} else if (*sep == '}') {
				cnt--;
			}
		}
		(void) snprintf(alt, sizeof(buf) - (alt - buf), "%.*s%s", (int) (sep - cp), cp, last);
		if (pkg_match(buf, pkg) == 1) {
			found = 1;
		}
	}
	return found;
}

/*
 * Perform glob match on "pkg" against "pattern".
 * Return 1 on match, 0 otherwise
 */
static int
glob_match(const char *pattern, const char *pkg)
{
	return fnmatch(pattern, pkg, FNM_PERIOD) == 0;
}

/*
 * Perform simple match on "pkg" against "pattern". 
 * Return 1 on match, 0 otherwise
 */
static int
simple_match(const char *pattern, const char *pkg)
{
	return strcmp(pattern, pkg) == 0;
}

/*
 * Performs a fast check if pattern can ever match pkg.
 * Returns 1 if a match is possible and 0 otherwise.
 */
int
quick_pkg_match(const char *pattern, const char *pkg)
{
#define simple(x) (isalnum((unsigned char)(x)) || (x) == '-')
	/*
	 * In pkgin we often match over the entire remote repository, and so it
	 * is optimal to extend this faster check to more characters and reduce
	 * the number of candidate matches, as later pkg_match checks are much
	 * more expensive.
	 *
	 * 8 loops are currrently chosen as that is the sweet spot for a 2022Q4
	 * repository with around 24,000 packages, limiting the maximum number
	 * of possible matches to around 120:
	 *
	 * $ for i in {2..10}; do
	 * >   printf "%3s:" ${i}
	 * >   pkgin avail | cut -c1-${i} | sort | uniq -c | sort -n | tail -1
	 * > done
	 *
	 *   2:   7144 py
	 *   3:   6415 py3
	 *   4:   3199 py31
	 *   5:   1608 py38-
	 *   6:   1607 py310-
	 *   7:    804 ruby27-
	 *   8:    119 p5-Test-
	 *   9:    104 py39-tryt
	 *  10:    104 py39-tryto
	 */
	for (int i = 0; i < 8; i++) {
		if (!simple(pattern[i]))
			return 1;
		if (pattern[i] != pkg[i])
			return 0;
	}
	return 1;
#undef simple
}

/*
 * Match pkg against pattern, return 1 if matching, 0 else
 */
int
pkg_match(const char *pattern, const char *pkg)
{
	if (!quick_pkg_match(pattern, pkg))
		return 0;

	if (strchr(pattern, '{') != (char *) NULL) {
		/* emulate csh-type alternates */
		return alternate_match(pattern, pkg);
	}
	if (strpbrk(pattern, "<>") != (char *) NULL) {
		int ret;

		/* perform relational dewey match on version number */
		ret = dewey_match(pattern, pkg);
		if (ret < 0)
			errx(EXIT_FAILURE, "dewey_match returned error");
		return ret;
	}
	if (strpbrk(pattern, "*?[]") != (char *) NULL) {
		/* glob match */
		if (glob_match(pattern, pkg))
			return 1;
	}

	/* no alternate, dewey or glob match -> simple compare */
	if (simple_match(pattern, pkg))
		return 1;

	/* globbing patterns and simple matches may be specified with or
	 * without the version number, so check for both cases. */

	{
		char *pattern_ver;
		int retval;

		pattern_ver = xasprintf("%s-[0-9]*", pattern);
		retval = glob_match(pattern_ver, pkg);
		free(pattern_ver);
		return retval;
	}
}

int
pkg_order(const char *pattern, const char *first_pkg, const char *second_pkg)
{
	const char *first_version;
	const char *second_version;

	if (first_pkg == NULL && second_pkg == NULL)
		return 0;

	if (first_pkg == NULL)
		return pkg_match(pattern, second_pkg) ? 2 : 0;
	if (second_pkg == NULL)
		return pkg_match(pattern, first_pkg) ? 1 : 0;

	first_version = strrchr(first_pkg, '-');
	second_version = strrchr(second_pkg, '-');

	if (first_version == NULL || !pkg_match(pattern, first_pkg))
		return pkg_match(pattern, second_pkg) ? 2 : 0;

	if (second_version == NULL || !pkg_match(pattern, second_pkg))
		return pkg_match(pattern, first_pkg) ? 1 : 0;

	if (dewey_cmp(first_version + 1, DEWEY_GT, second_version + 1))
		return 1;
	else if (dewey_cmp(first_version + 1, DEWEY_LT, second_version + 1))
		return 2;
	else if (strcmp(first_pkg, second_pkg) < 0)
		return 1;
	else
		return 2;
}
