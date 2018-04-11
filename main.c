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

#include "pkgin.h"
#include "cmd.h"
#include "external/lib.h"

static void	usage(int);
static int	find_cmd(const char *);
static void	missing_param(int, int, const char *);
static void	ginto(void);

uint8_t		yesflag = 0, noflag = 0, force_update = 0, force_reinstall = 0;
uint8_t		verbosity = 0, package_version = 0, parsable = 0, pflag = 0;
char		lslimit = '\0';
FILE  		*tracefp = NULL;

int
main(int argc, char *argv[])
{
	uint8_t		need_upgrade, need_refresh;
	uint8_t		do_inst = DO_INST; /* by default, do install packages */
	int 		ch, i, rc = EXIT_SUCCESS;
	const char	*chrootpath = NULL;

	setprogname("pkgin");

	/* Default to not doing \r printouts if we don't send to a tty */
	parsable = !isatty(fileno(stdout));

	while ((ch = getopt(argc, argv, "dhyfFPvVl:nc:t:p")) != -1) {
		switch (ch) {
		case 'f':
			force_update = 1;
			break;
		case 'F':
			force_reinstall = 1;
			break;
		case 'y':
			yesflag = 1;
			noflag = 0;
			break;
		case 'n':
			yesflag = 0;
			noflag = 1;
			break;
		case 'v':
			printf("pkgin %s (using %s)\n",
			    PKGIN_VERSION, pdb_version());
			exit(EXIT_SUCCESS);
			/* NOTREACHED */
		case 'h':
			usage(EXIT_SUCCESS);
			/* NOTREACHED */
		case 'l':
			lslimit = optarg[0];
			break;
		case 'd':
			do_inst = DONT_INST; /* download only */
			break;
		case 'c':
			chrootpath = optarg;
			break;
		case 'V':
			verbosity = 1;
			break;
		case 'P':
			package_version = 1;
			break;
		case 't':
			if ((tracefp = fopen(optarg, "w")) == NULL)
				err(EXIT_FAILURE, MSG_CANT_OPEN_WRITE, optarg);
			break;
		case 'p':
			parsable = 1;
			pflag = 1;
			break;
		default:
			usage(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/* check we were given a valid command */
	if (argc < 1 || (ch = find_cmd(argv[0])) == -1) {
		usage(EXIT_FAILURE);
		/* NOTREACHED */
	}

	/* enter chroot if -c specified */
	if (chrootpath != NULL) {
		if (chroot(chrootpath) == -1)
			errx(-1, MSG_CHROOT_FAILED);

		if (chdir("/") == -1)
			errx(-1, MSG_CHDIR_FAILED);
	}

	/* Configure pkg_install */
	setup_pkg_install();

	/* Configure pkgin database directory */
	setup_pkgin_dbdir();

	pkgindb_init();

	/* check if current database fits our needs */
	need_upgrade = upgrade_database();

	/* update local db if pkgdb mtime has changed */
	(void)update_db(LOCAL_SUMMARY, NULL, 1);

	/* split PKG_REPOS env variable and record them */
	split_repos();

	/* check repository consistency between repo list and recorded repos */
	need_refresh = chk_repo_list();

	/*
	 * upgrade remote database if pkgin version changed and not compatible,
	 * or if empty database, or if repository list changed.
	 */
	if (need_upgrade || need_refresh)
		(void)update_db(REMOTE_SUMMARY, NULL, 0);

	/* load preferred file */
	load_preferred();

	/* we need packages lists for almost everything */
	if (ch != PKG_UPDT_CMD) /* already loaded by update_db() */
		init_global_pkglists();

	switch (ch) {
	case PKG_UPDT_CMD: /* update packages db */
		if (need_upgrade || need_refresh) /* no need to do it twice */
			break;
		if (update_db(REMOTE_SUMMARY, NULL, 1) == EXIT_FAILURE)
			errx(EXIT_FAILURE, MSG_DONT_HAVE_RIGHTS);
		break;
	case PKG_SHDDP_CMD: /* show direct depends */
		missing_param(argc, 2, MSG_MISSING_PKGNAME);
		rc = show_direct_depends(argv[1]);
		break;
	case PKG_SHFDP_CMD: /* show full dependency tree */
		missing_param(argc, 2, MSG_MISSING_PKGNAME);
		rc = show_full_dep_tree(argv[1],
			DIRECT_DEPS, MSG_FULLDEPTREE);
		break;
	case PKG_SHRDP_CMD: /* show full reverse dependency tree */
		missing_param(argc, 2, MSG_MISSING_PKGNAME);
		rc = show_full_dep_tree(argv[1],
			LOCAL_REVERSE_DEPS,
			MSG_REVDEPTREE);
		break;
	case PKG_LLIST_CMD:
		/* list local packages */
		list_pkgs(LOCAL_PKGS_QUERY_DESC, PKG_LLIST_CMD);
		break;
	case PKG_RLIST_CMD:
		/* list available packages */
		list_pkgs(REMOTE_PKGS_QUERY_DESC, PKG_RLIST_CMD);
		break;
	case PKG_INST_CMD: /* install a package and its dependencies */
		missing_param(argc, 2, MSG_PKG_ARGS_INST);
		rc = pkgin_install(&argv[1], do_inst);
		break;
	case PKG_UPGRD_CMD: /* upgrade keep-packages */
		rc = pkgin_upgrade(UPGRADE_KEEP);
		break;
	case PKG_FUPGRD_CMD: /* upgrade everything installed */
		rc = pkgin_upgrade(UPGRADE_ALL);
		break;
	case PKG_REMV_CMD: /* remove packages and reverse dependencies */
		missing_param(argc, 2, MSG_PKG_ARGS_RM);
		rc = pkgin_remove(&argv[1]);
		break;
	case PKG_AUTORM_CMD: /* autoremove orphan packages */
		pkgin_autoremove();
		break;
	case PKG_KEEP_CMD: /* mark a package as "keep" (not automatic) */
		missing_param(argc, 2, MSG_PKG_ARGS_KEEP);
		pkg_keep(KEEP, &argv[1]);
		break;
	case PKG_UNKEEP_CMD: /* mark a package as "unkeep" (automatic) */
		missing_param(argc, 2, MSG_PKG_ARGS_UNKEEP);
		pkg_keep(UNKEEP, &argv[1]);
		break;
	case PKG_SHKP_CMD: /* show keep packages */
		show_pkg_keep();
		break;
	case PKG_SHNOKP_CMD: /* show keep packages */
		show_pkg_nokeep();
		break;
	case PKG_SRCH_CMD: /* search for package */
		missing_param(argc, 2, MSG_MISSING_SRCH);
		rc = search_pkg(argv[1]);
		break;
	case PKG_CLEAN_CMD: /* clean pkgin's packages cache */
		clean_cache();
		break;
	case PKG_EXPORT_CMD: /* export PKGPATH for keep packages */
		export_keep();
		break;
	case PKG_IMPORT_CMD: /* import for keep packages and install them */
		missing_param(argc, 2, MSG_MISSING_FILENAME);
		for (i=1; i<argc; i++)
			import_keep(do_inst, argv[i]);
		break;
	case PKG_SHPROV_CMD: /* show what a package provides */
		missing_param(argc, 2, MSG_MISSING_PKGNAME);
		show_prov_req(GET_PROVIDES_QUERY, argv[1]);
		break;
	case PKG_SHREQ_CMD: /* show what a package requires */
		missing_param(argc, 2, MSG_MISSING_PKGNAME);
		show_prov_req(GET_REQUIRES_QUERY, argv[1]);
		break;
	case PKG_SHPKGCONT_CMD: /* show remote package content */
		missing_param(argc, 2, MSG_MISSING_PKGNAME);
		show_pkg_info('L', argv[1]); /* pkg_info flag */
		break;
	case PKG_SHPKGDESC_CMD: /* show remote package DESCR */
		missing_param(argc, 2, MSG_MISSING_PKGNAME);
		show_pkg_info('d', argv[1]); /* pkg_info flag */
		break;
	case PKG_SHPKGBDEFS_CMD: /* show remote package build definitions */
		missing_param(argc, 2, MSG_MISSING_PKGNAME);
		show_pkg_info('B', argv[1]); /* pkg_info flag */
		break;
	case PKG_SHCAT_CMD: /* show packages belonging to a category */
		missing_param(argc, 2, MSG_MISSING_CATEGORY);
		show_category(argv[1]);
		break;
	case PKG_SHPCAT_CMD: /* show package's category */
		missing_param(argc, 2, MSG_MISSING_PKGNAME);
		show_pkg_category(argv[1]);
		break;
	case PKG_SHALLCAT_CMD: /* show all categories */
		show_all_categories();
		break;
	case PKG_GINTO_CMD: /* Miod's request */
		ginto();
		break;
        case PKG_STATS_CMD: /* show package statistics */
                pkgindb_stats();
                break;
	default:
		usage(EXIT_FAILURE);
		/* NOTREACHED */
	}

	free_global_pkglists();

	pkgindb_close();

	if (tracefp != NULL)
		fclose(tracefp);

	XFREE(env_repos);
	XFREE(pkg_repos);
	free_preferred();

	return rc;
}

static void
missing_param(int argc, int nargs, const char *msg)
{

	if (argc < nargs)
		errx(EXIT_FAILURE, "%s", msg);
}

/* find command index */
static int
find_cmd(const char *arg)
{
	int i;

	for (i = 0; cmd[i].name != NULL; i++) {
		if (strncmp(arg, cmd[i].name, strlen(cmd[i].name)) == 0 ||
			strncmp(arg, cmd[i].shortcut, strlen(cmd[i].name)) == 0)
			return cmd[i].cmdtype;
	}

	return -1;
}

static void
usage(int status)
{
	int i;

	fprintf((status) ? stderr : stdout,
	    "Usage: pkgin [-cdfFhlnPtvVy] command [package ...]\n\n"
	    "Commands and shortcuts:\n");

	for (i = 0; cmd[i].name != NULL; i++) {
		if (cmd[i].cmdtype != PKG_GINTO_CMD)
			fprintf((status) ? stderr : stdout,
			    "%-19s (%-4s) - %s\n",
			    cmd[i].name, cmd[i].shortcut, cmd[i].descr);
	}

	exit(status);
}

static void
ginto()
{
	printf("* 2 oz gin\n* 5 oz tonic water\n* 1 lime wedge\n");
}
