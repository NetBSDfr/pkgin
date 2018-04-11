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

static struct command {
	const char	*name;
	const char	*shortcut;
	const char	*descr;
	const int	cmdtype;
} cmd[] = {
	{ "list", "ls", "List installed local packages",
	  PKG_LLIST_CMD },
	{ "avail", "av", "List all available remote packages",
	  PKG_RLIST_CMD },
	{ "search", "se", "Search for a remote package",
	  PKG_SRCH_CMD },
	{ "install", "in", "Install or upgrade packages",
	  PKG_INST_CMD },
	{ "update", "up" , "Refresh local and remote package lists",
	  PKG_UPDT_CMD },
	{ "upgrade", "ug", "Upgrade only packages marked with the keep flag",
	  PKG_UPGRD_CMD },
	{ "full-upgrade", "fug", "Upgrade all packages",
	  PKG_FUPGRD_CMD },
	{ "remove", "rm", "Remove packages and any dependent packages",
	  PKG_REMV_CMD },
	{ "keep", "ke", "Mark packages that should be kept",
	  PKG_KEEP_CMD },
	{ "unkeep", "uk", "Mark packages that can be autoremoved",
	  PKG_UNKEEP_CMD },
	{ "export", "ex", "Display PKGPATH for all keep packages",
	  PKG_EXPORT_CMD },
	{ "import", "im", "Import keep package list from file",
	  PKG_IMPORT_CMD },
	{ "show-keep", "sk", "Display keep packages",
	  PKG_SHKP_CMD },
	{ "show-no-keep", "snk", "Display autoremovable packages",
	  PKG_SHNOKP_CMD },
	{ "autoremove", "ar", "Remove orphaned dependencies",
	  PKG_AUTORM_CMD },
	{ "clean", "cl", "Remove downloaded package files",
	  PKG_CLEAN_CMD },
	{ "show-deps", "sd", "List remote package direct dependencies",
	  PKG_SHDDP_CMD },
	{ "show-full-deps", "sfd", "List remote package full dependencies",
	  PKG_SHFDP_CMD },
	{ "show-rev-deps", "srd", "List local package reverse dependencies",
	  PKG_SHRDP_CMD },
	{ "provides", "prov", "Show which shared libraries a package provides",
	  PKG_SHPROV_CMD },
	{ "requires", "req", "Show which shared libraries a package requires",
	  PKG_SHREQ_CMD },
	{ "show-category", "sc", "List all packages belonging to a category",
	  PKG_SHCAT_CMD },
	{ "show-pkg-category", "spc", "Show categories a package belongs to",
	  PKG_SHPCAT_CMD },
	{ "show-all-categories", "sac", "List all known categories",
	  PKG_SHALLCAT_CMD },
	{ "pkg-content", "pc", "Show remote package content",
	  PKG_SHPKGCONT_CMD },
	{ "pkg-descr", "pd", "Show remote package long-description",
	  PKG_SHPKGDESC_CMD },
	{ "pkg-build-defs", "pbd", "Show remote package build definitions",
	  PKG_SHPKGBDEFS_CMD },
	{ "tonic", "to", "Gin Tonic recipe",
	  PKG_GINTO_CMD },
        { "stats", "st", "Show local and remote package statistics",
          PKG_STATS_CMD },
	{ NULL, NULL, NULL, 0 }
};
