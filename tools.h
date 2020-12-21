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

#ifndef _TOOLS_H
#define _TOOLS_H

#include "config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/utsname.h>

#if defined(HAVE_BSD_LIBUTIL_H)
#include <bsd/libutil.h>
#elif defined(HAVE_LIBUTIL_H)
#include <libutil.h>
#elif defined(HAVE_UTIL_H)
#include <util.h>
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

#define ANSW_NO 0
#define ANSW_YES 1
#define MSG_PROCEED_YES "proceed ? [Y/n] "
#define MSG_PROCEED_NO "proceed ? [y/N] "

/* those need to be initialized (main.c) */
extern uint8_t yesflag;
extern uint8_t noflag;

extern int charcount(char *, char);
extern size_t trimcr(char *);
extern void free_list(char **);
extern void do_log(const char *, const char *, ...);
extern void trunc_str(char *, char, int);
extern char *strreplace(char *, const char *, const char *);
extern char *getosarch(void);
extern char *getosmachine(void);
extern char *getosname(void);
extern char *getosrelease(void);
extern int check_yesno(uint8_t);

#endif
