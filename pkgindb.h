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

#ifndef _DRYDB_H
#define _DRYDB_H

#include <stdint.h>
#include "pkgindb_create.h"

extern const char DROP_LOCAL_TABLES[];
extern const char DROP_REMOTE_TABLES[];
extern const char DELETE_LOCAL[];
extern const char DELETE_REMOTE[];
extern const char DELETE_REMOTE_PKG_REPO[];
extern const char DIRECT_DEPS[];
extern const char LOCAL_DIRECT_DEPS[];
extern const char EXACT_DIRECT_DEPS[];
extern const char LOCAL_REVERSE_DEPS[];
extern const char REMOTE_REVERSE_DEPS[];
extern const char LOCAL_CONFLICTS[];
extern const char GET_CONFLICT_QUERY[];
extern const char GET_REQUIRES_QUERY[];
extern const char GET_PROVIDES_QUERY[];
extern const char LOCAL_PROVIDES[];
extern const char KEEP_PKG[];
extern const char UNKEEP_PKG[];
extern const char INSERT_REMOTE_PKG[];
extern const char LOCAL_PKGS_QUERY_ASC[];
extern const char REMOTE_PKGS_QUERY_ASC[];
extern const char LOCAL_PKGS_QUERY_DESC[];
extern const char REMOTE_PKGS_QUERY_DESC[];
extern const char NOKEEP_LOCAL_PKGS[];
extern const char KEEP_LOCAL_PKGS[];
extern const char PKG_URL[];
extern const char DELETE_EMPTY_ROWS[];
extern const char UPDATE_PKGDB_MTIME[];
extern const char EXISTS_REPO[];
extern const char INSERT_REPO[];
extern const char SELECT_REPO_URLS[];
extern const char DELETE_REPO_URL[];
extern const char UPDATE_REPO_MTIME[];
extern const char INSERT_SINGLE_VALUE[];
extern const char INSERT_DEPENDS_VALUES[];
extern const char UNIQUE_EXACT_PKG[];
extern const char UNIQUE_PKG[];
extern const char EXPORT_KEEP_LIST[];
extern const char GET_PKGNAME_BY_PKGPATH[];
extern const char GET_ORPHAN_PACKAGES[];
extern const char COMPAT_CHECK[];
extern const char SHOW_ALL_CATEGORIES[];

#define LOCAL_PKG "LOCAL_PKG"
#define REMOTE_PKG "REMOTE_PKG"

const char	*pdb_version(void);
uint8_t		upgrade_database(void);
void		pkgindb_init(void);
void		pkgindb_close(void);
int		pkgindb_dovaquery(const char *, ...);
int		pkgindb_doquery(const char *,
		    int (*)(void *, int, char *[], char *[]), void *);
int		pdb_get_value(void *, int, char **, char **);
int		pkg_db_mtime(void);
void		repo_record(char **);
time_t		pkg_sum_mtime(char *);
void		pkgindb_reset(void);
void            pkgindb_stats(void);

#define PDB_OK 0
#define PDB_ERR -1

#endif
