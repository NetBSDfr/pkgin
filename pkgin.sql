CREATE TABLE [PKGDB] (
	"PKGDB_MTIME" INTEGER,
	"PKGDB_NTIME" INTEGER
);

CREATE TABLE [REPOS] (
	"REPO_URL" TEXT UNIQUE,
	"REPO_MTIME" INTEGER
);

CREATE TABLE [REMOTE_PKG] (
	"PKG_ID" INTEGER PRIMARY KEY,
	"FULLPKGNAME" TEXT UNIQUE,
	"PKGNAME" TEXT,
	"PKGVERS" TEXT,
	"BUILD_DATE" TEXT,
	"COMMENT" TEXT,
	"LICENSE" TEXT NULL,
	"PKGTOOLS_VERSION" TEXT,
	"HOMEPAGE" TEXT NULL,
	"OS_VERSION" TEXT,
	"PKGPATH" TEXT,
	"PKG_OPTIONS" TEXT NULL,
	"CATEGORIES" TEXT,
	"SIZE_PKG" TEXT,
	"FILE_SIZE" TEXT,
	"OPSYS" TEXT,
	"REPOSITORY" TEXT
);

CREATE TABLE [LOCAL_PKG] (
	"PKG_ID" INTEGER PRIMARY KEY,
	"FULLPKGNAME" TEXT UNIQUE,
	"PKGNAME" TEXT,
	"PKGVERS" TEXT,
	"BUILD_DATE" TEXT,
	"COMMENT" TEXT,
	"LICENSE" TEXT NULL,
	"PKGTOOLS_VERSION" TEXT,
	"HOMEPAGE" TEXT NULL,
	"OS_VERSION" TEXT,
	"PKGPATH" TEXT,
	"PKG_OPTIONS" TEXT NULL,
	"CATEGORIES" TEXT,
	"SIZE_PKG" TEXT,
	"FILE_SIZE" TEXT,
	"OPSYS" TEXT,
	"PKG_KEEP" INTEGER NULL
);

/*
 * CONFLICTS
 */
CREATE TABLE local_conflicts (
	pkg_id		INTEGER,
	pattern		TEXT NOT NULL,
	pkgbase		TEXT
);
CREATE INDEX idx_local_conflicts_pkg_id ON local_conflicts (
	pkg_id		ASC
);
CREATE INDEX idx_local_conflicts_pattern ON local_conflicts (
	pattern		ASC
);
CREATE TABLE remote_conflicts (
	pkg_id		INTEGER,
	pattern		TEXT NOT NULL,
	pkgbase		TEXT
);
CREATE INDEX idx_remote_conflicts_pkg_id ON remote_conflicts (
	pkg_id		ASC
);
CREATE INDEX idx_remote_conflicts_pattern ON remote_conflicts (
	pattern		ASC
);

/*
 * DEPENDS
 */
CREATE TABLE local_depends (
	pkg_id		INTEGER,
	pattern		TEXT NOT NULL,
	pkgbase		TEXT
);
CREATE INDEX idx_local_depends_pkg_id ON local_depends (
	pkg_id		ASC
);
CREATE INDEX idx_local_depends_pattern ON local_depends (
	pattern		ASC
);
CREATE TABLE remote_depends (
	pkg_id		INTEGER,
	pattern		TEXT NOT NULL,
	pkgbase		TEXT
);
CREATE INDEX idx_remote_depends_pkg_id ON remote_depends (
	pkg_id		ASC
);
CREATE INDEX idx_remote_depends_pattern ON remote_depends (
	pattern		ASC
);

/*
 * PROVIDES
 */
CREATE TABLE local_provides (
	pkg_id		INTEGER,
	filename	TEXT
);
CREATE INDEX idx_local_provides_pkg_id ON local_provides (
	pkg_id		ASC
);
CREATE INDEX idx_local_provides_filename ON local_provides (
	filename	ASC
);
CREATE TABLE remote_provides (
	pkg_id		INTEGER,
	filename	TEXT
);
CREATE INDEX idx_remote_provides_pkg_id ON remote_provides (
	pkg_id		ASC
);
CREATE INDEX idx_remote_provides_filename ON remote_provides (
	filename	ASC
);

/*
 * REQUIRES
 */
CREATE TABLE local_requires (
	pkg_id		INTEGER,
	filename	TEXT
);
CREATE INDEX idx_local_requires_pkg_id ON local_requires (
	pkg_id		ASC
);
CREATE INDEX idx_local_requires_filename ON local_requires (
	filename	ASC
);
CREATE TABLE remote_requires (
	pkg_id		INTEGER,
	filename	TEXT
);
CREATE INDEX idx_remote_requires_pkg_id ON remote_requires (
	pkg_id		ASC
);
CREATE INDEX idx_remote_requires_filename ON remote_requires (
	filename	ASC
);

/*
 * SUPERSEDES
 */
CREATE TABLE remote_supersedes (
	pkg_id		INTEGER,
	pattern		TEXT NOT NULL,
	pkgbase		TEXT
);
CREATE INDEX idx_remote_supersedes_pkg_id ON remote_supersedes (
	pkg_id		ASC
);
CREATE INDEX idx_remote_supersedes_pattern ON remote_supersedes (
	pattern		ASC
);

/*
 * +REQUIRED_BY
 */
CREATE TABLE local_required_by (
	pkgname		TEXT,
	required_by	TEXT
);
CREATE INDEX idx_local_required_by_pkgname ON local_required_by (
	pkgname		ASC
);
CREATE INDEX idx_local_required_by_required_by ON local_required_by (
	required_by	ASC
);

CREATE INDEX [idx_remote_pkg_category] ON [REMOTE_PKG] (
	[CATEGORIES] ASC
);
CREATE INDEX [idx_remote_pkg_comment] ON [REMOTE_PKG] (
	[COMMENT] ASC
);
CREATE INDEX [idx_remote_pkg_name] ON [REMOTE_PKG] (
	[PKGNAME] ASC
);
CREATE INDEX [idx_local_pkg_category] ON [LOCAL_PKG] (
	[CATEGORIES] ASC
);
CREATE INDEX [idx_local_pkg_comment] ON [LOCAL_PKG] (
	[COMMENT] ASC
);
CREATE INDEX [idx_local_pkg_name] ON [LOCAL_PKG] (
	[PKGNAME] ASC
);
