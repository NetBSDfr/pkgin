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

/* main.c */
#define MSG_MISSING_PKGNAME "missing package name"
#define MSG_MISSING_FILENAME "missing file name"
#define MSG_FULLDEPTREE "full dependency tree for %s\n"
#define MSG_REVDEPTREE "local reverse dependency tree for %s\n"
#define MSG_PKG_ARGS_INST "specify at least one package to install"
#define MSG_PKG_ARGS_RM "specify at least one package to remove"
#define MSG_PKG_ARGS_KEEP "specify at least one package to keep"
#define MSG_PKG_ARGS_UNKEEP "specify at least one package to unkeep"
#define MSG_MISSING_SRCH "missing search string"
#define MSG_MISSING_CATEGORY "missing category"

#define MSG_CHROOT_FAILED "Unable to chroot"
#define MSG_CHDIR_FAILED "Unable to chroot"

#define MSG_MISSING_PKG_REPOS \
	PKGIN_CONF"/"REPOS_FILE" has no repositories or does not exist.\nNo PKG_REPOS variable to fallback to."
#define MSG_CANT_OPEN_WRITE "Couldn't open %s for writing.\n"
#define MSG_PKG_INSTALL_NOT_PRESENT "pkg_install package is not installed."
#define MSG_DONT_HAVE_RIGHTS "You don't have enough rights for this operation."

/* actions.c */
#define MSG_REMOVING "removing %s...\n"
#define MSG_DOWNLOAD_PKGS "downloading packages...\n"
#define MSG_PKG_NO_REPO "%s has no associated repository"
#define MSG_SYMLINK_FAILED "failed to create symlink %s"
#define MSG_SYMLINKING_PKG "symlinking %s...\n"
#define MSG_ERR_OPEN "error opening %s"
#define MSG_INSTALL_PKG "installing packages...\n"
#define MSG_INSTALLING "installing %s...\n"
#define MSG_REQT_NOT_PRESENT \
	"%s, needed by %s is not present in this system.\n"
#define MSG_REQT_NOT_PRESENT_DEPS \
	"warning: %s is not present in this system nor package's dependencies\n"
#define MSG_CONFLICT_PKG \
	"%s (to be installed) conflicts with installed package %s.\n"
#define MSG_NOTHING_TO_DO "nothing to do.\n"
#define MSG_REQT_MISSING "the following packages have unmet requirements: %s\n\n"
#define MSG_NO_CACHE_SPACE "%s does not have enough space for download, (%s required but only %s are available)\n"
#define MSG_NO_INSTALL_SPACE "%s does not have enough space for installation (%s required but only %s are available)\n"
#define MSG_PKGS_TO_UPGRADE "%d packages to be upgraded:\n%s\n"
#define MSG_PKGS_TO_REMOVE "%d packages to be removed: \n%s\n"
#define MSG_NOTHING_TO_UPGRADE "nothing to upgrade.\n"
#define MSG_PKGS_TO_DOWNLOAD "%d packages to be downloaded (%s to download):\n%s\n"
#define MSG_PKGS_TO_INSTALL \
	"%d packages to be installed (%s to download, %s to install):\n%s\n"
#define MSG_RM_UPGRADE_PKGS "removing packages to be upgraded...\n"
#define MSG_NOTHING_TO_INSTALL "nothing to install.\n"
#define MSG_EMPTY_LOCAL_PKGLIST "empty local package list."
#define MSG_PKG_NOT_INSTALLED "no such installed package %s\n"
#define MSG_PKGS_TO_DELETE "%d packages to delete: \n%s\n"
#define MSG_NO_PKGS_TO_DELETE "no packages to delete\n"
#define MSG_EMPTY_KEEP_LIST "empty non-autoremovable package list"
#define MSG_EMPTY_NOKEEP_LIST "empty autoremovable package list"
#define MSG_EMPTY_AVAIL_PKGLIST "empty available packages list"
#define MSG_UPGRADE_NEED_REMOVE "this upgrade needs package removal\n"
#define MSG_PKG_INSTALL_LOGGING_TO "pkg_install error log can be found in %s\n"
#define MSG_EMPTY_FILE_SIZE "warning: package %s has an empty FILE_SIZE on repository.\n"
#define MSG_NOT_REMOVING "not removing %s...\n"
#define MSG_UPGRADE_PKG_INSTALL "about to upgrade %s, "
#define MSG_ERR_INSTALLING_PKG "/!\\ there was an error while installing %s, please check %s\n"
#define MSG_ERR_REMOVING_PKG "/!\\ there was an error while removing %s, please check %s\n"
#define MSG_WARNS_ERRS "pkg_install warnings: %d, errors: %d\n"
#define MSG_USER_ABORT "user aborted.\n"

/* depends.c */
#define MSG_DIRECT_DEPS_FOR "direct dependencies for %s\n"

/* download.c */
#define MSG_DOWNLOADING "downloading %s:   0%%"
#define MSG_DOWNLOADING_PCT "\rdownloading %s: %8s %3d%%"
#define MSG_DOWNLOAD_START "downloading %s"
#define MSG_DOWNLOAD_END " done.\n"

/* autoremove.c */
#define MSG_AUTOREMOVE_WARNING "\
in order to remove packages from the autoremove list, flag those with the `keep' action.\n"
#define MSG_ALL_KEEP_PKGS "all packages are marked as \"keepable\"\n."
#define MSG_AUTOREMOVE_PKGS "%d packages to be autoremoved: %s\n"
#define MSG_MARK_PKG_KEEP "%s is marked as non-autoremovable\n"
#define MSG_MARK_PKG_NOKEEP "%s is marked as autoremovable\n"
#define MSG_MARKING_PKG_KEEP "marking %s as non auto-removable\n"
#define MSG_UNMARKING_PKG_KEEP "marking %s as auto-removable\n"
#define MSG_NO_ORPHAN_DEPS "no orphan dependencies found.\n"

/* summary.c */
#define MSG_READING_LOCAL_SUMMARY "reading local summary...\n"
#define MSG_CLEANING_DB_FROM_REPO "cleaning database from %s entries...\n"
#define MSG_PROCESSING_LOCAL_SUMMARY "processing local summary...\n"
#define MSG_DB_IS_UP_TO_DATE "database for %s is up-to-date\n"
#define MSG_PROCESSING_REMOTE_SUMMARY "processing remote summary (%s)...\n"
#define MSG_COULDNT_FETCH "Could not fetch %s\n"
#define MSG_ARCH_DONT_MATCH "\r\n/!\\ Warning /!\\ %s doesn't match your current architecture (%s)\nYou probably want to modify "PKGIN_CONF"/"REPOS_FILE".\nStill want to "
#define MSG_COULD_NOT_GET_PKGNAME "Could not get package name from dependency: %s\n"

/* impact.c */
#define MSG_GREATER_VERSION \
	"warning: installed package %s has a greater version than %s (to be installed)\n"
#define MSG_MORE_THAN_ONE_VER \
	"there's more than one version available for this package.\n\
please re-run %s with a package name matching one of the following:\n"
#ifndef DEBUG
#define MSG_CALCULATING_DEPS "\rcalculating dependencies..."
#else
#define MSG_CALCULATING_DEPS "calculating dependencies for %s...\n"
#endif
#define MSG_PKG_NOT_AVAIL "%s is not available on the repository\n"
#define MSG_PKG_IS_PREFERRED "\rnot choosing %s, %s is preferred\n"
#define MSG_PKG_NOT_INSTALLABLE "\r\n%s is not installable\n"
#define MSG_BROKEN_DEP "%s has no dependency in pkg_summary(5), while it's a reverse dependency for %s (missing package in repository ?). Default behaviour is to remove %s. "

/* pkglist.c */
#define MSG_IS_INSTALLED_CODE "\n=: package is installed and up-to-date\n<: package is installed but newer version is available\n>: installed package has a greater version than available package\n"
#define MSG_NO_SEARCH_RESULTS "No results found for %s\n"
#define MSG_EMPTY_LIST "Requested list is empty.\n"
#define MSG_NO_CATEGORIES "No categories found.\n"

/* pkgindb.c */
#define MSG_LOCAL_STAT_TITLE "Local package database:\n"
#define MSG_LOCAL_PACKAGES "\tInstalled packages: %s\n"
#define MSG_LOCAL_PKG_SIZE "\tDisk space occupied: %s\n\n"
#define MSG_REMOTE_STAT_TITLE "Remote package database(s):\n"
#define MSG_REMOTE_NB_REPOS "\tNumber of repositories: %d\n"
#define MSG_REMOTE_PACKAGES "\tPackages available: %s\n"
#define MSG_REMOTE_PKG_SIZE "\tTotal size of packages: %s\n"

/* fsops.c */
#define MSG_TRANS_FAILED "Failed to translate %s in repository config file"
#define MSG_INVALID_REPOS "Invalid repository: %s"

/* selection.c */
#define MSG_EMPTY_IMPORT_LIST "Empty import list."

/* pkg_check.c */
#define MSG_NO_PROV_REQ "Nothing %s by %s.\n"
#define MSG_FILES_PROV_REQ "Files %s by %s:\n"
