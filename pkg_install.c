/*
 * Copyright (c) 2018 The NetBSD Foundation, Inc.
 * All rights reserved.
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

/*
 * Routines for configuring and using pkg_install utilities.
 */

#include "pkgin.h"

char *pkg_add;
char *pkg_admin;
char *pkg_delete;
char *pkg_info;

/*
 * Configure the location to pkg_install used in this instance, either via the
 * PKG_INSTALL_DIR environment variable or the default compiled-in location.
 */
void
setup_pkg_install(void)
{
	FILE *fp;
	char *line, *p;
	size_t len;
	ssize_t llen;

	p = getenv("PKG_INSTALL_DIR");
	pkg_add = xasprintf("%s/pkg_add", p ? p : PKG_INSTALL_DIR);
	pkg_admin = xasprintf("%s/pkg_admin", p ? p : PKG_INSTALL_DIR);
	pkg_info = xasprintf("%s/pkg_info", p ? p : PKG_INSTALL_DIR);
	pkg_delete = xasprintf("%s/pkg_delete", p ? p : PKG_INSTALL_DIR);

	/* Sanity check */
	if (access(pkg_admin, X_OK) != 0)
		err(EXIT_FAILURE, "Cannot execute %s", pkg_admin);

	/* Ensure pkg_install only looks at our specified paths */
	unsetenv("PKG_PATH");

	/* Get PKG_DBDIR from pkg_admin */
	p = xasprintf("%s config-var PKG_DBDIR", pkg_admin);

	if ((fp = popen(p, "r")) == NULL)
		err(EXIT_FAILURE, "Cannot execute '%s'", p);

	line = NULL; len = 0;
	while ((llen = getline(&line, &len, fp)) > 0) {
		if (line[llen - 1] == '\n')
			line[llen - 1] = '\0';
		pkgdb_set_dir(line, 1);
	}
	pclose(fp);

	free(line);
	free(p);

	if (pkgdb_get_dir() == NULL)
		errx(EXIT_FAILURE, "Could not determine PKG_DBDIR");
}
