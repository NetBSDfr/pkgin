/* $Id: tools.h,v 1.5 2012/08/04 14:23:46 imilh Exp $ */

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

#ifndef _TOOLS_H
#define _TOOLS_H

#if HAVE_CONFIG_H
#include "config.h"
#endif
#if HAVE_NBCOMPAT_H
#include <nbcompat.h>
#endif
#if HAVE_STDIO_H
#include <stdio.h>
#endif
#if HAVE_STDARG_H
#include <stdarg.h>
#endif
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_NBCOMPAT_STRING_H /* strsep() */
#include <nbcompat/string.h>
#elif HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_ERR_H
#include <err.h>
#endif
#if HAVE_CTYPE_H
#include <ctype.h>
#endif
#if HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#if defined(HAVE_BSD_LIBUTIL_H)
#include <bsd/libutil.h>
#elif defined(HAVE_LIBUTIL_H)
#include <libutil.h>
#elif defined(HAVE_UTIL_H)
#include <util.h>
#else
#include <nbcompat/util.h>
#endif

#include "external/lib.h"

#ifndef HN_AUTOSCALE
#include "external/humanize_number.h"
#endif

#ifdef LINE_MAX
#define MAXLEN LINE_MAX
#else
#define MAXLEN 2048
#endif
#define STR_FORWARD 0
#define STR_BACKWARD 1

#define R_READ(fd, buf, len)		\
	if (read(fd, buf, len) < 0) {	\
		warn("read()");		\
		pthread_exit(NULL);	\
	}

#define R_CLOSE(fd)			\
	do {				\
		close(fd);		\
		pthread_exit(NULL);	\
	} while (/* CONSTCOND */ 0)

#define DSTSRC_CHK(dst, src)			\
	if (dst == NULL) {			\
		warn("NULL destination");	\
		break;				\
	}					\
	if (src == NULL) {			\
		warn("NULL source");		\
		break;				\
	}


#define XSTRCPY(dst, src)		\
	do {				\
		DSTSRC_CHK(dst, src);	\
		strcpy(dst, src);	\
	} while (/* CONSTCOND */ 0)

#define XSTRCAT(dst, src)		\
	do {				\
		DSTSRC_CHK(dst, src);	\
		strcat(dst, src);	\
	} while (/* CONSTCOND */ 0)

#define XFREE(elm)		   	\
	do {				\
		if (elm != NULL) {	\
			free(elm);	\
			elm = NULL;	\
		}			\
	} while (/* CONSTCOND */ 0)

#define KVPRINTF(k, v)				\
	printf("key: %s, val: %s\n", k, v);	\

#ifndef SLIST_FOREACH_MUTABLE /* from DragonFlyBSD */
#define SLIST_FOREACH_MUTABLE(var, head, field, tvar)		\
	for ((var) = SLIST_FIRST((head));			\
	    (var) && ((tvar) = SLIST_NEXT((var), field), 1);	\
		 (var) = (tvar))
#endif

#define ANSW_NO 0
#define ANSW_YES 1
#define MSG_PROCEED_YES "proceed ? [Y/n] "
#define MSG_PROCEED_NO "proceed ? [y/N] "

/* those need to be initialized (main.c) */
extern uint8_t yesflag;
extern uint8_t noflag;

extern int charcount(char *, char);
extern int trimcr(char *);
extern char **splitstr(char *, const char *);
extern void free_list(char **);
extern int min(int, int);
extern int max(int, int);
extern int listlen(const char **);
extern char **exec_list(const char *, const char *);
extern uint8_t is_listed(const char **, const char *);
extern void do_log(const char *, const char *, ...);
extern void trunc_str(char *, char, int);
extern char *safe_snprintf(int, const char *, ...);
extern int safe_strcmp(const char *s1, const char *s2);
extern char *strreplace(char *, const char *, const char *);
extern char *getosarch(void);
extern char *getosrelease(void);
extern int check_yesno(uint8_t);

#endif
