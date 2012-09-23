/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <ftw.h>
#include <fcntl.h>

#include <xbps_api.h>
#include "queue.h"

#define _PROGNAME	"xbps-create"

struct xentry {
	TAILQ_ENTRY(xentry) entries;
	char *file, *type, *target, *hash;
};

static TAILQ_HEAD(xentry_head, xentry) xentry_list =
    TAILQ_HEAD_INITIALIZER(xentry_list);

static uint64_t instsize;
static prop_dictionary_t pkg_propsd, pkg_filesd;
static const char *destdir;

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stderr,
	"usage: %s [options] [file1] [file2] ...\n\n"
	"  Options:\n"
	"    -A, --architecture   Package architecture (e.g: noarch, i686, etc).\n"
	"    -B, --built-with     Package builder string (e.g: xbps-src-30).\n"
	"    -C, --conflicts      Conflicts (blank separated list,\n"
	"                         e.g: 'foo>=2.0 blah<=2.0').\n"
	"    -D, --dependencies   Dependencies (blank separated list,\n"
	"                         e.g: 'foo>=1.0_1 blah<2.1').\n"
	"    -d, --destdir        Package destdir.\n"
	"    -F, --config-files   Configuration files (blank separated list,\n"
	"                         e.g '/etc/foo.conf /etc/foo-blah.conf').\n"
	"    -h, --homepage       Homepage.\n"
	"    -l, --license        License.\n"
	"    -M, --mutable-files  Mutable files list (blank separated list,\n"
	"                         e.g: '/usr/lib/foo /usr/bin/blah').\n"
	"    -m, --maintainer     Maintainer.\n"
	"    -n, --pkgver         Package name/version tuple (e.g `foo-1.0_1').\n"
	"    -P, --provides       Provides (blank separated list,\n"
	"                         e.g: 'foo-9999 blah-1.0').\n"
	"    -p, --preserve       Enable package preserve boolean.\n"
	"    -q, --quiet          Work silently.\n"
	"    -R, --replaces       Replaces (blank separated list,\n"
	"                         e.g: 'foo>=1.0 blah<2.0').\n"
	"    -S, --long-desc      Long description (80 cols per line).\n"
	"    -s, --desc           Short description (max 80 characters).\n"
	"    -V, --version        Prints the xbps release version\n\n"
	"  Example:\n"
	"    $ %s -A noarch -n foo-1.0_1 -s \"foo pkg\" -d dir\n",
	_PROGNAME, _PROGNAME);
	exit(EXIT_FAILURE);
}

static void __attribute__((noreturn))
die(const char *fmt, ...)
{
	va_list ap;
	int save_errno = errno;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ERROR: ", _PROGNAME);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, " %s\n", save_errno ? strerror(save_errno) : "");
	va_end(ap);
	exit(EXIT_FAILURE);
}

static void
process_array(const char *key, const char *val)
{
	prop_array_t array;
	char *args, *p = NULL, *saveptr;

	assert(key);

	if (val == NULL)
		return;

	array = prop_array_create();
	assert(array);

	if (strchr(val, ' ') == NULL) {
		prop_array_add_cstring_nocopy(array, val);
		goto out;
	}

        args = strdup(val);
	assert(args);

	for ((p = strtok_r(args, " ", &saveptr)); p;
	     (p = strtok_r(NULL, " ", &saveptr))) {
		if (p == NULL)
			continue;
		prop_array_add_cstring(array, p);
	}
	free(args);
out:
	prop_dictionary_set(pkg_propsd, key, array);
	prop_object_release(array);
}

static bool
entry_is_conf_file(const char *file)
{
	prop_array_t a;
	const char *curfile;
	size_t i;

	assert(file);

	a = prop_dictionary_get(pkg_propsd, "conf_files");
	if (a == NULL || prop_array_count(a) == 0)
		return false;

	for (i = 0; i < prop_array_count(a); i++) {
		prop_array_get_cstring_nocopy(a, i, &curfile);
		if (strcmp(file, curfile) == 0)
			return true;
	}
	return false;
}

static int
ftw_cb(const char *fpath, const struct stat *sb, int type, struct FTW *ftwbuf)
{
	struct xentry *xe = NULL;
	const char *filep = NULL;
	char buf[PATH_MAX];

	(void)ftwbuf;

	/* Ignore metadata files generated by xbps-src and destdir */
	if ((strcmp(fpath, ".") == 0) ||
	    (strcmp(fpath, "./props.plist") == 0) ||
	    (strcmp(fpath, "./files.plist") == 0) ||
	    (strcmp(fpath, "./flist") == 0) ||
	    (strcmp(fpath, "./rdeps") == 0))
		return 0;

	/* sanitized file path */
	filep = strchr(fpath, '.') + 1;
	xe = calloc(1, sizeof(*xe));
	assert(xe);
	xe->file = strdup(fpath);
	assert(xe->file);

	if ((strcmp(fpath, "./INSTALL") == 0) ||
	    (strcmp(fpath, "./REMOVE") == 0)) {
		/* metadata file */
		xe->type = strdup("metadata");
		assert(xe->type);
		goto out;
	}

	if (type == FTW_SL) {
		/*
		 * Symlinks.
		 *
		 * Find out target file.
		 */
		xe->type = strdup("links");
		assert(xe->type);
		memset(&buf, 0, sizeof(buf));
		if (realpath(fpath, buf) == NULL)
			die("failed to process symlink `%s':", filep);

		filep = buf + strlen(destdir);
		xe->target = strdup(filep);
		assert(xe->target);
	} else if (type == FTW_F) {
		/*
		 * Regular files.
		 * Find out if it's a configuration file or not
		 * and calculate sha256 hash.
		 */
		if (entry_is_conf_file(filep))
			xe->type = strdup("conf_files");
		else
			xe->type = strdup("files");

		assert(xe->type);
		if ((xe->hash = xbps_file_hash(fpath)) == NULL)
			die("failed to process hash for %s:", fpath);

		instsize += sb->st_size;

	} else if (type == FTW_D) {
		/* directory */
		xe->type = strdup("dirs");
		assert(xe->type);
	}

out:
	TAILQ_INSERT_TAIL(&xentry_list, xe, entries);
	return 0;
}

static void
process_xentry(const char *key, const char *mutable_files)
{
	prop_array_t a;
	prop_dictionary_t d;
	struct xentry *xe;
	char *p, *saveptr, *args, *tok;
	bool found = false, mutable_found = false;

	a = prop_array_create();
	assert(a);

	TAILQ_FOREACH_REVERSE(xe, &xentry_list, xentry_head, entries) {
		if (strcmp(xe->type, key))
			continue;

		found = true;
		d = prop_dictionary_create();
		assert(d);
		/* sanitize file path */
		p = strchr(xe->file, '.') + 1;
		/*
		 * Find out if this file is mutable.
		 */
		if (mutable_files) {
			if ((strchr(mutable_files, ' ') == NULL) &&
			    (strcmp(mutable_files, p) == 0))
				prop_dictionary_set_bool(d, "mutable", true);
			else {
				args = strdup(mutable_files);
				assert(args);
				for ((tok = strtok_r(args, " ", &saveptr)); tok;
				    (tok = strtok_r(NULL, " ", &saveptr))) {
					if (strcmp(tok, p) == 0) {
						mutable_found = true;
						break;
					}
				}
				free(args);
				if (mutable_found) {
					prop_dictionary_set_bool(d, "mutable",
					    true);
					mutable_found = false;
				}
			}
		}
		prop_dictionary_set_cstring(d, "file", p);
		if (xe->target)
			prop_dictionary_set_cstring(d, "target", xe->target);
		else if (xe->hash)
			prop_dictionary_set_cstring(d, "sha256", xe->hash);
		prop_array_add(a, d);
		prop_object_release(d);
	}
	if (found)
		prop_dictionary_set(pkg_filesd, key, a);

	prop_object_release(a);
}

static void
process_destdir(const char *mutable_files)
{
	if (nftw(".", ftw_cb, 20, FTW_PHYS|FTW_MOUNT) != 0)
		die("failed to process destdir files (nftw):");

	/* Process regular files */
	process_xentry("files", mutable_files);

	/* Process configuration files */
	process_xentry("conf_files", NULL);

	/* Process symlinks */
	process_xentry("links", NULL);

	/* Process directories */
	process_xentry("dirs", NULL);
}

static void
process_entry_file(struct archive *ar, struct xentry *xe, const char *filematch)
{
	struct archive *ard;
	struct archive_entry *entry;
	struct stat st;
	char *buf, *p;
	int fd;
	size_t len;

	assert(ar);
	assert(xe);

	if (filematch && strcmp(xe->file, filematch))
		return;

	ard = archive_read_disk_new();
	assert(ard);
	archive_read_disk_set_standard_lookup(ard);
	archive_read_disk_set_symlink_physical(ard);

	entry = archive_entry_new();
	assert(entry);
	archive_entry_set_pathname(entry, xe->file);

	p = xbps_xasprintf("%s/%s", destdir, xe->file);
	assert(p);

	if ((fd = open(p, O_RDONLY)) == -1)
		die("failed to add entry (open) %s to archive:", xe->file);

	if (fstat(fd, &st) == -1)
		die("failed to add entry (fstat) %s to archive:", xe->file);

	if (st.st_size >= SSIZE_MAX)
		die("failed to add entry (SSIZE_MAX) %s to archive:", xe->file);

	if ((archive_read_disk_entry_from_file(ard, entry, fd, NULL)) != 0)
		die("failed to add entry %s to archive:", xe->file);

	archive_write_header(ar, entry);
	buf = malloc(st.st_size+1);
	assert(buf);
	len = read(fd, buf, st.st_size);
	archive_write_data(ar, buf, len);
	free(buf);

	free(p);
	close(fd);
	archive_entry_free(entry);
	archive_read_close(ard);
	archive_read_free(ard);
}

static void
process_entry_memory(struct archive *ar, const void *src, const char *file)
{
	struct archive_entry *entry;
	time_t tm;

	assert(ar);
	assert(src);
	assert(file);

	tm = time(NULL);

	entry = archive_entry_new();
	assert(entry);
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, 0644);
	archive_entry_set_uname(entry, "root");
	archive_entry_set_gname(entry, "root");
	archive_entry_set_pathname(entry, file);
	archive_entry_set_size(entry, strlen(src));
	archive_entry_set_atime(entry, tm, 0);
	archive_entry_set_mtime(entry, tm, 0);
	archive_entry_set_ctime(entry, tm, 0);
	archive_write_header(ar, entry);
	archive_write_data(ar, src, strlen(src));
	archive_entry_free(entry);
}

static void
destroy_xentry(struct xentry *xe)
{
	assert(xe);

	free(xe->file);
	free(xe->type);
	if (xe->target)
		free(xe->target);
	if (xe->hash)
		free(xe->hash);
	free(xe);
}

static void
process_archive(struct archive *ar, const char *pkgver, bool quiet)
{
	struct xentry *xe;
	char *xml;

	/* Add INSTALL/REMOVE metadata scripts first */
	TAILQ_FOREACH(xe, &xentry_list, entries) {
		process_entry_file(ar, xe, "./INSTALL");
		process_entry_file(ar, xe, "./REMOVE");
	}
	/*
	 * Add the installed-size object.
	 */
	prop_dictionary_set_uint64(pkg_propsd, "installed_size", instsize);

	/* Add props.plist metadata file */
	xml = prop_dictionary_externalize(pkg_propsd);
	assert(xml);
	process_entry_memory(ar, xml, "./props.plist");
	free(xml);

	/* Add files.plist metadata file */
	xml = prop_dictionary_externalize(pkg_filesd);
	assert(xml);
	process_entry_memory(ar, xml, "./files.plist");
	free(xml);

	/* Add all package data files and release resources */
	while ((xe = TAILQ_FIRST(&xentry_list)) != NULL) {
		TAILQ_REMOVE(&xentry_list, xe, entries);
		if ((strcmp(xe->type, "metadata") == 0) ||
		    (strcmp(xe->type, "dirs") == 0)) {
			destroy_xentry(xe);
			continue;
		}
		if (!quiet) {
			printf("%s: adding `%s' ...\n", pkgver, xe->file);
			fflush(stdout);
		}
		process_entry_file(ar, xe, NULL);
		destroy_xentry(xe);
	}
}

static void
set_build_date(void)
{
	char outstr[64];
	time_t t;
	struct tm *tmp;

	t = time(NULL);
	tmp = localtime(&t);
	assert(tmp);

	if (strftime(outstr, sizeof(outstr)-1, "%F %R %Z", tmp) == 0)
		die("failed to set build-date object (strftime):");

	if (!prop_dictionary_set_cstring(pkg_propsd, "build-date", outstr))
		die("failed to add build-date object:");
}

int
main(int argc, char **argv)
{
	struct option longopts[] = {
		{ "architecture", required_argument, NULL, 'A' },
		{ "built-with", required_argument, NULL, 'B' },
		{ "conflicts", required_argument, NULL, 'C' },
		{ "dependencies", required_argument, NULL, 'D' },
		{ "destdir", required_argument, NULL, 'd' },
		{ "config-files", required_argument, NULL, 'F' },
		{ "homepage", required_argument, NULL, 'h' },
		{ "license", required_argument, NULL, 'l' },
		{ "mutable-files", required_argument, NULL, 'M' },
		{ "maintainer", required_argument, NULL, 'm' },
		{ "pkgver", required_argument, NULL, 'n' },
		{ "provides", required_argument, NULL, 'P' },
		{ "preserve", no_argument, NULL, 'p' },
		{ "quiet", no_argument, NULL, 'q' },
		{ "replaces", required_argument, NULL, 'R' },
		{ "long-desc", required_argument, NULL, 'S' },
		{ "desc", required_argument, NULL, 's' },
		{ "version", no_argument, NULL, 'V' },
		{ 0, 0, 0, 0 }
	};
	struct archive *ar;
	struct stat st;
	const char *conflicts, *deps, *homepage, *license, *maint, *bwith;
	const char *provides, *pkgver, *replaces, *desc, *ldesc;
	const char *arch, *config_files, *mutable_files, *version;
	char *pkgname, *binpkg, *tname, *p, cwd[PATH_MAX-1];
	bool quiet = false, preserve = false;
	int c, pkg_fd;
	mode_t myumask;

	arch = conflicts = deps = homepage = license = maint = NULL;
	provides = pkgver = replaces = desc = ldesc = bwith = NULL;
	config_files = mutable_files = NULL;

	while ((c = getopt_long(argc, argv,
		"A:B:C:D:d:F:h:l:M:m:n:P:pqR:S:s:V", longopts, &c)) != -1) {
		if (optarg && strcmp(optarg, "") == 0)
			optarg = NULL;

		switch (c) {
		case 'A':
			arch = optarg;
			break;
		case 'B':
			bwith = optarg;
			break;
		case 'C':
			conflicts = optarg;
			break;
		case 'D':
			deps = optarg;
			break;
		case 'd':
			destdir = optarg;
			break;
		case 'F':
			config_files = optarg;
			break;
		case 'h':
			homepage = optarg;
			break;
		case 'l':
			license = optarg;
			break;
		case 'M':
			mutable_files = optarg;
			break;
		case 'm':
			maint = optarg;
			break;
		case 'n':
			pkgver = optarg;
			break;
		case 'P':
			provides = optarg;
			break;
		case 'p':
			preserve = true;
			break;
		case 'q':
			quiet = true;
			break;
		case 'R':
			replaces = optarg;
			break;
		case 'S':
			ldesc = optarg;
			break;
		case 's':
			desc = optarg;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (destdir == NULL)
		die("destdir not set!");
	else if (pkgver == NULL)
		die("pkgver not set!");
	else if (desc == NULL)
		die("short description not set!");
	else if (arch == NULL)
		die("architecture not set!");
	/*
	 * Sanity check for required options.
	 */
	pkgname = xbps_pkg_name(pkgver);
	if (pkgname == NULL)
		die("invalid pkgver! got `%s' expected `foo-1.0_1'", pkgver);
	version = xbps_pkg_version(pkgver);
	if (version == NULL)
		die("invalid pkgver! got `%s' expected `foo-1.0_1'", pkgver);

	if (stat(destdir, &st) == -1)
		die("failed to stat() destdir `%s':", destdir);
	if (!S_ISDIR(st.st_mode))
		die("destdir `%s' is not a directory!", destdir);
	/*
	 * Process XBPS_PKGPROPS metadata file.
	 */
	pkg_propsd = prop_dictionary_create();
	assert(pkg_propsd);

	/* Required properties */
	prop_dictionary_set_cstring_nocopy(pkg_propsd, "architecture", arch);
	prop_dictionary_set_cstring_nocopy(pkg_propsd, "pkgname", pkgname);
	prop_dictionary_set_cstring_nocopy(pkg_propsd, "version", version);
	prop_dictionary_set_cstring_nocopy(pkg_propsd, "pkgver", pkgver);
	prop_dictionary_set_cstring_nocopy(pkg_propsd, "short_desc", desc);
	set_build_date();

	/* Optional properties */
	if (homepage)
		prop_dictionary_set_cstring_nocopy(pkg_propsd,
				"homepage", homepage);
	if (license)
		prop_dictionary_set_cstring_nocopy(pkg_propsd,
				"license", license);
	if (maint)
		prop_dictionary_set_cstring_nocopy(pkg_propsd,
				"maintainer", maint);
	if (ldesc)
		prop_dictionary_set_cstring_nocopy(pkg_propsd,
				"long_desc", ldesc);
	if (bwith)
		prop_dictionary_set_cstring_nocopy(pkg_propsd,
				"packaged-with", bwith);
	if (preserve)
		prop_dictionary_set_bool(pkg_propsd, "preserve", true);

	/* Optional arrays */
	process_array("run_depends", deps);
	process_array("conf_files", config_files);
	process_array("conflicts", conflicts);
	process_array("provides", provides);
	process_array("replaces", replaces);

	/* save cwd */
	memset(&cwd, 0, sizeof(cwd));
	p = getcwd(cwd, sizeof(cwd));
	assert(p);

	if (chdir(destdir) == -1)
		die("cannot chdir() to destdir %s:", destdir);
	/*
	 * Process XBPS_PKGFILES metadata file.
	 */
	pkg_filesd = prop_dictionary_create();
	assert(pkg_filesd);
	process_destdir(mutable_files);

	/* Back to original cwd after file tree walk processing */
	if (chdir(cwd) == -1)
		die("cannot chdir() to cwd %s:", cwd);

	/*
	 * Create a temp file to store archive data.
	 */
	tname = xbps_xasprintf(".xbps-pkg-XXXXXX");
	assert(tname);
	pkg_fd = mkstemp(tname);
	assert(pkg_fd != -1);
	/*
	 * Process the binary package's archive (ustar compressed with xz).
	 */
	ar = archive_write_new();
	assert(ar);
	archive_write_add_filter_xz(ar);
	archive_write_set_format_ustar(ar);
	archive_write_set_options(ar, "compression-level=9");
	if (archive_write_open_fd(ar, pkg_fd) != 0)
		die("Failed to open %s fd for writing:", tname);

	process_archive(ar, pkgver, quiet);
	archive_write_free(ar);
	prop_object_release(pkg_propsd);
	prop_object_release(pkg_filesd);
	/*
	 * Archive was created successfully; flush data to storage,
	 * set permissions and rename to dest file; from the caller's
	 * perspective it's atomic.
	 */
	binpkg = xbps_xasprintf("%s.%s.xbps", pkgver, arch);
	assert(binpkg);

	(void)fsync(pkg_fd);
	myumask = umask(0);
	(void)umask(myumask);

	if (fchmod(pkg_fd, 0666 & ~myumask) == -1)
		die("cannot fchmod() %s:", tname);

	if (rename(tname, binpkg) == -1)
		die("cannot rename %s to %s:", tname, binpkg);

	/* Success, release resources */
	if (!quiet)
		printf("%s: binary package created successfully (%s)\n",
		    pkgver, binpkg);

	free(binpkg);
	free(pkgname);
	close(pkg_fd);

	exit(EXIT_SUCCESS);
}
