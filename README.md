pkgin(1) -- A tool to manage pkgsrc binary packages.
====================================================

## SYNOPSIS

`pkgin` [`-dfFhipPvVyn`] [`-l` _limit_chars_] [`-c` _chroot_path_] [`-t` _log_file_] _command_ [package ...]

## DESCRIPTION

The pkgin command is aimed at being an `apt` / `yum` like tool for managing pkgsrc(7) binary packages. It relies on pkg_summary(5) for installation, removal and upgrade of packages and associated dependencies, using a remote repository.

## OPTIONS

The following command line arguments are supported:

  * `-c` chroot_path:
    Enable chrooting pkgin in the given repository

  * `-d`:
    Download only

  * `-f`:
    Force database update

  * `-F`:
    Force package reinstall

  * `-h`:
    Displays help for the command

  * `-i`:
    Allow insecure downloads: bypass HTTPS certificate validation,
    allow HTTPS to redirect to HTTP/FTP

  * `-l` _limit_chars_:
    Only include the packages with the specified [STATUS FLAGS][]

  * `-n`:
    Assumes "no" as default answer and print results of actions to be taken line per line

  * `-p`:
    Displays results in a parsable format

  * `-P`:
    Displays packages versions instead of globs (sd, sfd, srd)

  * `-t` log_file:
    Logs package browsing (dependencies and impact) to a given log file

  * `-v`:
    Displays pkgin version

  * `-V`:
    Be verbose when (un)installing

  * `-y`:
    Assumes "yes" as default answer, except for autoremove

The `pkgin` utility provides several commands:

  * `autoremove`:
    Automatically removes orphan dependencies. When used with the `-n` flag, it can be used to show packages that are possibly not necessary.

  * `avail`:
    Lists all packages available in the repository.

  * `clean`:
    Delete downloaded packages from the cache directory.

  * `export`:
    Export the list of non-autoremovable packages to stdout (one category/package by line)

  * `upgrade`:
    Upgrade all packages to the newest versions available in the repository.

  * `import` _file_:
    Import a list of packages to be installed from file (one category/package by line)

  * `install` _package|glob_ ...:
    Performs installation or upgrade of package. If more than one packages are specified on the command-line, all will be installed (or upgraded). Instead of a package name, a glob can be specified in order to install specific versions.

    Example:

    pkgin in 'mysql-server&gt;5.1&lt;5.6'  

  * `keep` _package_ ...:
    Marks package as "non auto-removable". A `keep`-able package is equivalent to a non-automatic package in pkgsrc(7) terminology.

  * `list`:
    Lists all packages installed locally on a system. If the l modifier is added to this command, show only packages matching the status flag.

  * `pkg-content` _package_:
    Show remote package content.

  * `pkg-descr` _package_:
    Show remote package long-description.

  * `pkg-build-defs` _package_:
    Show remote package build definitions.

  * `provides` _package_:
    Shows what a package provides to others

  * `remove` _package_ ...:
    Removes package as well as all packages depending on it. When more than one package are specified, they will all be uninstalled. By default, it will prompt you to confirm before package removals.

  * `requires` _package_:
    Shows what a package requires from others packages.

  * `search` _pattern_:
    Performs a regular expression search for a pattern in the repository.

  * `show-deps`:
    Displays all direct dependencies

  * `show-full-deps` _package_:
    Displays all direct dependencies recursively

  * `show-rev-deps` _package_:
    Displays all reverse direct dependencies for package. If more than one package is specified, pkgin will show recursively reverse direct dependencies for all packages on the command-line.

  * `show-category` _category_:
    Show packages belonging to category.

  * `show-pkg-category` _package_:
    Show package category.

  * `show-keep`:
    Display "non auto-removable" packages.

  * `show-no-keep`:
    Display "auto-removable" packages.

  * `unkeep` _package_ ...:
    Marks package as "auto-removable". If no other package depends on it, it will be removed when using the autoremove modifier. It is equivalent to an `automatic` package in pkgsrc(7) terminology.

  * `update`:
    Creates and populates the initial database of locally installed packages and available packages (from the remote pkg_summary(5) list). This is done automatically when pkgin is first used, when the system package database has been modified, or when pkgin is upgraded to a new database version.

## STATUS FLAGS

When using the `-l` flag along with the list command, the following status flag must be set:

  * `=`:
    The installed version of the package is current.

  * `<`:
    The installed version of the package is older than the current version.

  * `>`:
    The installed version of the package is newer than the current version.

## ENVIRONMENT

`PKG_REPOS`
    The `PKG_REPOS` environment variable can be pointed to a suitable repository or a list of space separated repositories in order to override _/usr/pkg/etc/pkgin/repositories.conf_

## FILES

  * _/usr/pkg/etc/pkgin/repositories.conf_:
    This file contains a list of repository URIs that pkgin will use. It may contain macros `$arch` to define the machine hardware platform and `$osrelease` to define the release version for the operating system (as reported by uname(3)).

  * _/usr/pkg/etc/pkgin/preferred.conf_:
    This file contains a list of preferences regarding packages to be installed or upgraded. Each line defines a package preference taking the form of a simple glob(3).

    Example:

    mysql-server&lt;5.6  
    php&gt;=5.4  
    autoconf=2.69.*


  * _/var/db/pkgin_:
    This directory contains component needed by `pkgin` at run time. This directory can be completely emptied if `pkgin`'s database gets corrupted, `pkgin` will rebuild its database based on `pkg_install`'s `PKG_DB` next time it is called.

  * _/var/db/pkgin/cache_:
    This directory contains the packages downloaded by `pkgin`. It is safe to empty it regularily using `pkgin clean` or simply `rm -rf /var/db/pkgin/cache`.

  * _/var/db/pkgin/pkgin.db_:
    _pkgin.db_ is the main `pkgin` `SQLite` database. This format has been chosen in order to parse, query, match and order packages using the `SQL` language thus making packages list manipulation a lot easier.

  * _/var/db/pkgin/pkg_install-err.log_:
    This file contains errors and warnings given by pkg_add(1) and pkg_delete(1), which are the tools called by `pkgin` to manipulate packages themselves.

  * _/var/db/pkgin/sql.log_:
    This file contains `SQL` errors that might have occurred on a `SQLite` query. Mainly for debugging purposes. 

## EXAMPLES

    Setup the initial database:

    # echo ftp://ftp.fr.netbsd.org/pub/pkgsrc/packages/NetBSD/i386/5.0/All > /usr/pkg/etc/pkgin/repositories.conf
    # pkgin update
    processing local summary...
    updating database: 100%
    downloading pkg_summary.bz2: 100%
    processing remote summary (ftp://ftp.fr.netbsd.org/pub/pkgsrc/packages/NetBSD/i386/5.0/All)...
    updating database: 100%

    Listing all packages available in the repository:

    # pkgin avail | more
    [...]
    autoconf-2.63        Generates automatic source code configuration scripts
    aumix-gtk-2.8nb3     Set mix levels (ncurses and GTK+ 2.0 interfaces)
    aumix-2.8nb7         Set mix levels (ncurses interface only)
    august-0.63b         Simple Tk-based HTML editor
    audacity-1.2.6nb3    Audio editor
    [...]

    Install packages and their dependencies:

    # pkgin install links eterm
    nothing to upgrade.
    11 packages to be installed: tiff-3.8.2nb4 png-1.2.35 libungif-4.1.4nb1 libltdl-1.5.26 jpeg-6bnb4 pcre-7.8 perl-5.10.0nb5 libast-0.6.1nb3 imlib2-1.4.2nb1 links-2.2nb1 eterm-0.9.4nb1 (25M to download, 64M to install)
    proceed ? [y/N]

    Remove packages and their reverse dependencies:

    # pkgin remove links eterm
    2 packages to delete: links-2.2nb1 eterm-0.9.4nb1
    proceed ? [y/N]

    Remove orphan dependencies:

    # pkgin autoremove
    in order to remove packages from the autoremove list, flag those with the -k modifier.
    9 packages to be autoremoved: libast-0.6.1nb3 pcre-7.8 imlib2-1.4.2nb1 tiff-3.8.2nb4 png-1.2.35 libungif-4.1.4nb1 libltdl-1.5.26 perl-5.10.0nb5 jpeg-6bnb4
    proceed ? [y/N]

## SEE ALSO

pkg_add(1), pkg_info(1), pkg_summary(5), pkgsrc(7)

## AUTHORS

  * Emile `iMil` Heitor:
    Initial work and ongoing development.
  * Jonathan Perkin:
    Primary maintainer 0.9.0 onwards.

## CONTRIBUTORS

  * Jeremy C. Reed:
    Testing and refinements.
  * Arnaud Ysmal:
    Tests and patches
  * Claude Charpentier:
    SQLite schema, and SQL queries debugging.
  * Guillaume Lasmayous:
    Man page
  * Antonio Huete Jimenez:
    DragonFly port
  * Min Sik Kim:
    Darwin port
  * Filip Hajny:
    SunOS port
  * Baptiste Daroussin:
    FreeBSD port and patches
  * Gautam B.T.:
    MINIX port
  * Thomas `wiz` Klausner:
    Testing and refinements.
  * Youri `yrmt` Mouton:
    OSX testing and patches

## BUGS

We're hunting them.
