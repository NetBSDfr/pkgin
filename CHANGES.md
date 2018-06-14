## Version 0.11.1 (2018-06-14)

 * Permit the user to install an older package version.
 * Ensure warning and error counters are reset for each phase.
 * Print total download size when using -d.

## Version 0.11.0 (2018-06-08)

 * Fix build on NetBSD/evbarm.
 * Skip download checks for packages that are being removed, prevents
   erroneous "empty FILE\_SIZE" failures.
 * Perform in-place upgrades using `pkg_add -DU` rather than removing
   and reinstalling.  Should be faster and less error-prone.
 * Output formatting improvements.  Installs are now split between
   refresh, upgrade, and install, with package lists formatted to be
   easier to read.

## Version 0.10.2 (2018-06-06)

 * Fix -f flag when used with install.
 * Handle empty `BUILD_DATE` correctly.

## Version 0.10.1 (2018-06-01)

 * Fix `BUILD_DATE` refresh support for preserved packages.
 * Improve provides/requires output to avoid confusion for new users.

## Version 0.10.0 (2018-05-17)

 * Add `BUILD_DATE` support, packages are now upgraded if the `BUILD_DATE`
   has changed, to ensure rebuilt dependencies are correctly handled.
 * Download handling fixes.
 * Count packages correctly.
 * Ensure the remote database is updated before performing upgrades.
 * Internal code cleanups, refactoring, WARNS fixes, etc.
 * Support nanosecond resolution on pkgdb mtime.
 * Sync progressmeter and pkg\_install with upstreams.
 * Improve SQL logging.
 * Fix -d when used with an upgrade action.
 * Fix exit status for various commands and usage.
 * Avoid segfault in show-category.
 * Improve pkgin database initialisation and upgrade checks.
 * Support libarchive 2.x and Minix 3.3.0
 * Support `PKG_INSTALL_DIR` and `PKGIN_DBDIR`, primarily to support the
   new pkgin test suite: https://github.com/joyent/pkgin-test-suite

## Version 0.9.4 (2016-02-08)

 * Check for summary updates before fetching packages.

## Version 0.9.3 (2015-08-18)

 * Ensure we aren't copying overlapping memory regions.
 * Free libarchive resources after use.
 * Various WARNS and build cleanups.

## Version 0.9.2 (2015-08-17)

 * Fix -Wempty-body build issue.

## Version 0.9.1 (2015-08-13)

 * Fix off-by-one NUL handling in pkg\_summary.

## Version 0.9.0 (2015-08-13)

 * Various performance improvements from Joyent, see blog post at
   <http://www.perkin.org.uk/posts/reducing-ram-usage-in-pkgin.html>
   for more details.
 * Clean database entries correctly when changing repositories.
 * Support pkg\_summary.xz if available.
 * Don't switch to parsable output when stdout isn't a tty, broke various
   automation tools: NetBSDfr/pkgin#46.
 * Bugfix: added missing dash before ffu

## Version 0.8.0 (2015-01-28)

 * Added preferred.conf support
 * man page now based on ronn <http://rtomayko.github.io/ronn/ronn.1.html>
 * When -n is provided to pkgin ar, rm, in, fug, packages list is shown
   as one package per line so it is easier to read (feature proposed
   by gdt@ in GitHub issue #41)
 * Fixed GitHub issue #33: clean database when repository removed
 * Fixed some resource leakage pointed out by coverity scan
 * +PRESERVE packages now correctly upgraded without pkg\_add -f
 * Dropped percentage printout if stdout is not a tty (patch by Havard
   Eidnes)
 * Changed pkg\_add -f flag with -D as suggested by Greg Troxel
 * Fixed an issue with meta-packages not updating pkgdb, check for
   `PKG_DBDIR` mtime instead
 * Avoid deepness > 1 when checking for dependency upgrades, should fix
 * PR #48561
 * Avoid `preserved' packages to be `autoremoved'
 * Fixed GH issue #1, `NULL PKG_PATH` (which is wrong btw) makes pkgin
   segfault, strcmp() secured with safe\_strcmp()
 * Added --with-pkginstall to configure

## Version 0.7.0 (2014-12-23)

 * Fixed GH issue #36, automatic flag no more set when a package is
   installed via pkg\_add(1)
 * Added -p (parsable) flag for 3rd party frontends
 * Added statistics (patch by Youri Mouton)
 * Package-names-containing-a-dot fix (patch by Yamamoto Takashi)

## Version 0.6.5 (2014-04-07)

 * Multi-repo debugging, fixed packages order

## Version 0.6.4 (2013-06-16)

 * Fixed many return codes
 * Large file systems fix by Filip Hajny
 * gcc 4.7 / unused-but-set-variable fix by Makoto Fujiwara
 * Migrated to github, updated website
 * Fixed computing of download size for local repositories (Reported by
   Ignatios Souvatzis in PR#47501)

## Version 0.6.3.1 (2012-12-14)

 * Do not exit with not enough space message when install size is negative
 * PR3555339@sf (stacktic)
 * Avoid negative speed when a download start (stacktic)
 * PR47196 fixed by stacktic
 * PR47060 fixed by stacktic

## Version 0.6.3 (2012-11-14)

 * Added show-no-keep by orgrim
 * Fixed PR47192 (by stacktic)

## Version 0.6.2.2 (2012-08-01)

 * Fixed bad size\_pkg
 * Fixed build on platforms lacking `__UNCONST`

## Version 0.6.2 (2012-07-30)

 * Modified default behaviour for Proceed [Y/n]
 * Rewrote narrow\_match() using version\_check() to avoid more
   dirty repositories mistakes.
 * Preparing pkginteractive integration.
 * SQLite is now part of NetBSD 6.0 base, remove it from pkgin's tree.
   Dependency will be added in the package.
 * Added SQLite build patch from sbd@
 * Added show-all-categories
 * unlink() before symlink()
 * Dynamically read `PKG_DBDIR` from pkg\_admin
 * Do not delete repository entries if the new one is unreachable.
 * When a package is marked as TOREMOVE and TOUPGRADE because of a
   dependency breakage during upgrade, just follow the usual upgrade
   process.
 * Handled "no value field" for buggy packages.
 * Hopefuly fixed the "dirty repository" bug thanks to bapt
 * Added show-{pkg-,}category as requested by Julian Fagir
 * Added pkg-build-defs as requested by gls@
 * Added pkg-content and pkg-descr
 * Introduced pkg\_infos.c to fetch remote informations
 * Removed `-D_FILE_OFFSET_BITS=64` from Makefile and added it in fsops.c

## Version 0.6.0 (2012-04-14)

 * case: cleanup in summary.c (stacktic)
 * Packages list queries ordered differently for display and internals

## Version 0.5.2.3 (2012-04-09)

 * Fixed "upgrade too many packages" bug (thanks stacktic)

## Version 0.5.2.2 (2011-10-30)

 * Handled misordered PKGNAME/CONFLICTS
 * Cleaned up `DELETE_REMOTE` query (thanks to anhj)

## Version 0.5.2 (2011-10-24)

 * Various minor fixes from NetBSDfr hackathon
 * Moved break\_depends() at the end of impact.c to avoid inconsistencies
 * Check for real filesystem size to be occuppied by upgrade
 * Check for user permissions before update\_db
 * update\_db() returns a status so we can warn simple users
 * introduced have\_enough\_rights(), fixed pkg\_keep() perms
 * Various return codes fixed to satisfy frontends
 * Don't download packages when repository is a file:// scheme
 * Reduce verbosity when marking non-autoremovable packages
 * Keep going if unmet requirements are encountered and warn about
   it before proceeding
 * Always move pkg\_install on top of the "to-upgrade" list
 * pkgin now depends on pkgsrc's pkg\_install


## Version 0.5.1 (2011-09-10)

 * Log timestamp
 * Better handling of pkg\_install error logs
 * rec\_pkglist() now takes a va\_list
 * Added "requires" and "provides" to have a closer look on packages
   requirements
 * pkg\_install's pkgdb not needed anymore for a fresh start (bsdx's idea)
 * Globs can now be passed as an argument for package installation, i.e.
   `pkgin in 'mysql-server<5.1'`
 * Export / import a list of packages as requested by wiz@
 * Operations tracing flag (-t)
 * Database silent migration
 * More flexible check\_yesno()
 * Made pkgin in / rm yes by default

## Version 0.5.0 (2011-08-21)

 * One struct to rule them all (Pkglist)
 * Got rid of many useless lists (speed x10, literally)
 * Introduced FULLPKGNAME as db member, speed ups searches
 * unique\_pkg(): no more "many versions of foo", pickup newer
 * Progress now shown with progressmeter from OpenSSH

## Version 0.4.2.2 (2011-08-09)

 * Cleaned up download.c
 * SQLite upgraded to 3.7.7.1

## Version 0.4.2.1 (2011-08-02)

 * Fixed the "too many connexions" problem with libfetch and FTP

## Version 0.4.2 (2011-03-06)

 * Moving to SF.net
 * Check for mark\_as\_automatic\_installed() return code
 * pkgin can now upgrade pkg\_install with user's approval
 * url\_stat's size is declared as off\_t, will be > `SSIZE_MAX` on
   32 bits systems. Added a fix for this.
 * Added -P, print package version instead of globs in sd, srd, sfd
 * Added warning for repositories with zero-length `FILE_SIZE`
 * Added -V (verbosity) flag
 * Moved integer flags to uint8\_t
 * Wiped out file.c, ftpio.c, path.c, str.c, pexec.c, pkg\_io.c,
 * pen.c, strsep.c, lpkg.c, iterate.c
 * Inform about logging
 * Replaced naive repository arch check by `MACHINE_ARCH` check
 * Bump to SQLite 3.7.5
 * Added statvfs64 support
 * Redirect stderr to logfile while pkg\_{add,delete}
 * Feature request: -F / force-reinstall
 * Feature request: PR 43049
 * MINIX patches from Gautam are now upstream
 * Re-added download-only
 * Makefile.in / configure.ac cleanup
 * Integrated some of bapt's patches (chroot, bandwidth calculation)

## Version 0.4.0 (2011-01-22)

 * SQLite "Amalgamation" version is now part of the tree,
 * No more databases/sqlite3 dependency needed

## Version 0.3.3.4 (2011-01-20)

 * Fixed a non-critical bug: some packages were marked
 * for upgrade more than once.

## Version 0.3.3.3 (2011-01-19)

 * Double dewey match fixed, i.e. foo>=1.2.3<3.0
 * Duplicate entries for remove and upgrade fixed
 * "Missing package in repository" case handled
 * Many cleanups from stacktic
     * Replaced strstr's with str{n}cmp when possible
     * #ifdef'ed PROVIDES
     * Queries are now const chars
 * Solaris 10 support
 * Mac OS X support
 * Many cleanups from stacktic
 * Many fixes by stacktic :
     * Added -n (no-flag)
     * Various memleaks fixed
     * pkgname comparison fix
     * Cleaned up trailing spaces
     * Got rid of recursion !
 * Added -l status flags
 * autoconf support
 * percentage redraw fix (jmcneill)
 * variables cleanups (PKG\_SYSCONFDIR, VARBASE) (sketch)
 * auto-lookup for SUMEXTS, removed options.mk (sketch)
 * opensolaris fixes (sketch)
 * repositories.conf variables substitution (tuxillo)
 * impact mutex (Johannes Hofmann)
 * SunOS 5.8 support (Mikhail T.)

## Version 0.2.5 (2009-06-08)

 * Added the long awaited repositories file
 * ${PREFIX}/etc/pkgin/repositories.conf
 * Glue between keep-state and pkgdb "automatic" flag.
 * pkgin now handles removal of packages when an upgrade would
 * break dependencies, i.e. upgrading php from version 4 to 5
 * will break php4-modules dependencies. Modules will be removed
 * before php-4 is upgraded.
 * Database modification. See the MIGRATION file for details on
   how to safely rebuild your database.
 * mtime check over pkg\_summary files on repositories, don't
   update pkgin database if mtime have not changed.
 * Database modification, see 20090507 to rebuild it
 * pkgin now records local pkgdb mtime so it does not rebuild
   its database when pkgdb is not updated
 * Parameters have changed.
 * Instead of getopt()-style flags, pkgin is now using commands.
   Only -y and -h have been kept.
 * pkg\_dry is now known as pkgin
 * Added -c flag, clean cache: delete all downloaded packages from
 * `/var/db/pkg_dry/cache`
