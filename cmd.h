/* $Id: cmd.h,v 1.15 2012/10/02 10:20:24 imilh Exp $ */

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

static struct command {
	const char	*name;
	const char	*shortcut;
	const char	*descr;
	const int	cmdtype;
} cmd[] = {
	{ "list", "ls", "List installed packages.",
	  PKG_LLIST_CMD },
	{ "avail", "av", "List available packages.",
	  PKG_RLIST_CMD },
	{ "install", "in", "Perform packages installation or upgrade.",
	  PKG_INST_CMD },
	{ "update", "up" , "Create and populate the initial database.",
	  PKG_UPDT_CMD },
	{ "remove", "rm", "Remove packages and depending packages.",
	  PKG_REMV_CMD },
	{ "upgrade", "ug", "Upgrade main packages to their newer versions.",
	  PKG_UPGRD_CMD },
	{ "full-upgrade", "fug", "Upgrade all packages to their newer versions.",
	  PKG_FUPGRD_CMD },
	{ "show-deps", "sd", "Display direct dependencies.",
	  PKG_SHDDP_CMD },
	{ "show-full-deps", "sfd", "Display dependencies recursively.",
	  PKG_SHFDP_CMD },
	{ "show-rev-deps", "srd", "Display reverse dependencies recursively.",
	  PKG_SHRDP_CMD },
	{ "show-category", "sc", "Show packages belonging to category.",
	  PKG_SHCAT_CMD },
	{ "show-pkg-category", "spc", "Show package's category.",
	  PKG_SHPCAT_CMD },
	{ "show-all-categories", "sac", "Show all categories.",
	  PKG_SHALLCAT_CMD },
	{ "keep", "ke", "Mark package as \"non auto-removable\".",
	  PKG_KEEP_CMD },
	{ "unkeep", "uk", "Mark package as \"auto-removable\".",
	  PKG_UNKEEP_CMD },
	{ "show-keep", "sk", "Display \"non auto-removable\" packages.",
	  PKG_SHKP_CMD },
	{ "show-no-keep", "snk", "Display \"auto-removable\" packages.",
	  PKG_SHNOKP_CMD },
	{ "search", "se", "Search for a package.",
	  PKG_SRCH_CMD },
	{ "clean", "cl", "Clean packages cache.",
	  PKG_CLEAN_CMD },
	{ "autoremove", "ar", "Autoremove orphan dependencies.",
	  PKG_AUTORM_CMD },
	{ "export", "ex", "Export \"non auto-removable\" packages to stdout.",
	  PKG_EXPORT_CMD },
	{ "import", "im", "Import \"non auto-removable\" package list from file.",
	  PKG_IMPORT_CMD },
	{ "provides", "prov", "Show what files a package provides.",
	  PKG_SHPROV_CMD },
	{ "requires", "req", "Show what files a package requires.",
	  PKG_SHREQ_CMD },
	{ "pkg-content", "pc", "Show remote package's content.",
	  PKG_SHPKGCONT_CMD },
	{ "pkg-descr", "pd", "Show remote package's long-description.",
	  PKG_SHPKGDESC_CMD },
	{ "pkg-build-defs", "pbd", "Show remote package's build definitions.",
	  PKG_SHPKGBDEFS_CMD },
	{ "tonic", "to", "Gin Tonic recipe.",
	  PKG_GINTO_CMD },
        { "stats", "st", "Packages statistics.",
          PKG_STATS_CMD },
	{ NULL, NULL, NULL, 0 }
};
