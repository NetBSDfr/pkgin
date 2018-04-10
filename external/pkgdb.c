/* NetBSD: pkgdb.c,v 1.39 2010/04/20 21:22:38 joerg Exp */

/*-
 * Copyright (c) 1999-2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Hubert Feyrer <hubert@feyrer.de>.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This is a much simplified version of pkgdb.c that provides what is required
 * by the other files we have pulled from pkg_install.  It is also modified to
 * set pkgdb_dir explicitly to what we have parsed from pkg_admin(1).
 */

#include "lib.h"

static char *pkgdb_dir = NULL;
static int pkgdb_dir_prio = 0;

const char *
pkgdb_get_dir(void)
{
	return pkgdb_dir;
}

void
pkgdb_set_dir(const char *dir, int prio)
{
	if (prio < pkgdb_dir_prio)
		return;

	pkgdb_dir_prio = prio;

	if (dir == pkgdb_dir)
		return;

	pkgdb_dir = xstrdup(dir);
}

char *
pkgdb_pkg_file(const char *pkg, const char *file)
{
	return xasprintf("%s/%s/%s", pkgdb_get_dir(), pkg, file);
}
