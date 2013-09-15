/*-
 * Copyright (c) 2012-2013 Juan Romero Pardines.
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
#include <sys/mman.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <ftw.h>
#include <fcntl.h>
#include <libgen.h>
#include <locale.h>

#include <xbps.h>
#include "queue.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

#define _PROGNAME	"xbps-create"

/* libarchive 2.x compat */
#if ARCHIVE_VERSION_NUMBER >= 3000000
# define archive_write_finish(x) 	archive_write_free(x)
#endif

struct xentry {
	TAILQ_ENTRY(xentry) entries;
	char *file, *type, *target, *hash;
};

static TAILQ_HEAD(xentry_head, xentry) xentry_list =
    TAILQ_HEAD_INITIALIZER(xentry_list);

static uint64_t instsize;
static xbps_dictionary_t pkg_propsd, pkg_filesd;
static const char *destdir;

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stdout,
	"Usage: %s [OPTIONS] destdir\n\n"
	"OPTIONS\n"
	" -A --architecture   Package architecture (e.g: noarch, i686, etc).\n"
	" -B --built-with     Package builder string (e.g: xbps-src-30).\n"
	" -C --conflicts      Conflicts (blank separated list,\n"
	"                     e.g: 'foo>=2.0 blah<=2.0').\n"
	" -D --dependencies   Dependencies (blank separated list,\n"
	"                     e.g: 'foo>=1.0_1 blah<2.1').\n"
	" -F --config-files   Configuration files (blank separated list,\n"
	"                     e.g '/etc/foo.conf /etc/foo-blah.conf').\n"
	" -H --homepage       Homepage.\n"
	" -h --help           Show help.\n"
	" -l --license        License.\n"
	" -M --mutable-files  Mutable files list (blank separated list,\n"
	"                     e.g: '/usr/lib/foo /usr/bin/blah').\n"
	" -m --maintainer     Maintainer.\n"
	" -n --pkgver         Package name/version tuple (e.g `foo-1.0_1').\n"
	" -P --provides       Provides (blank separated list,\n"
	"                     e.g: 'foo-9999 blah-1.0').\n"
	" -p --preserve       Enable package preserve boolean.\n"
	" -q --quiet          Work silently.\n"
	" -R --replaces       Replaces (blank separated list,\n"
	"                     e.g: 'foo>=1.0 blah<2.0').\n"
	" -S --long-desc      Long description (80 cols per line).\n"
	" -s --desc           Short description (max 80 characters).\n"
	" -V --version        Prints XBPS release version.\n"
	" --build-options     A string with the used build options.\n"
	" --shlib-provides    List of provided shared libraries (blank separated list,\n"
	"                     e.g 'libfoo.so.1 libblah.so.2').\n"
	" --shlib-requires    List of required shared libraries (blank separated list,\n"
	"                     e.g 'libfoo.so.1 libblah.so.2').\n\n"
	"NOTE:\n"
	" At least three flags are required: architecture, pkgver and desc.\n\n"
	"EXAMPLE:\n"
	" $ %s -A noarch -n foo-1.0_1 -s \"foo pkg\" destdir\n",
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
	xbps_array_t array;
	char *args, *p = NULL, *saveptr;

	assert(key);

	if (val == NULL)
		return;

	array = xbps_array_create();
	assert(array);

	if (strchr(val, ' ') == NULL) {
		xbps_array_add_cstring_nocopy(array, val);
		goto out;
	}

        args = strdup(val);
	assert(args);

	for ((p = strtok_r(args, " ", &saveptr)); p;
	     (p = strtok_r(NULL, " ", &saveptr))) {
		if (p == NULL)
			continue;
		xbps_array_add_cstring(array, p);
	}
	free(args);
out:
	xbps_dictionary_set(pkg_propsd, key, array);
	xbps_object_release(array);
}

static bool
entry_is_conf_file(const char *file)
{
	xbps_array_t a;
	const char *curfile;

	assert(file);

	a = xbps_dictionary_get(pkg_propsd, "conf_files");
	if (a == NULL || xbps_array_count(a) == 0)
		return false;

	for (unsigned int i = 0; i < xbps_array_count(a); i++) {
		xbps_array_get_cstring_nocopy(a, i, &curfile);
		if (strcmp(file, curfile) == 0)
			return true;
	}
	return false;
}

static int
ftw_cb(const char *fpath, const struct stat *sb, int type, struct FTW *ftwbuf _unused)
{
	struct xentry *xe = NULL;
	const char *filep = NULL;
	char *buf, *p, *p2, *dname;
	ssize_t r;

	/* Ignore metadata files generated by xbps-src and destdir */
	if ((strcmp(fpath, ".") == 0) ||
	    (strcmp(fpath, "./props.plist") == 0) ||
	    (strcmp(fpath, "./files.plist") == 0) ||
	    (strcmp(fpath, "./flist") == 0) ||
	    (strcmp(fpath, "./rdeps") == 0) ||
	    (strcmp(fpath, "./shlib-provides") == 0) ||
	    (strcmp(fpath, "./shlib-requires") == 0))
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
		buf = malloc(sb->st_size+1);
		assert(buf);
		r = readlink(fpath, buf, sb->st_size+1);
		if (r < 0 || r > sb->st_size)
			die("failed to process symlink %s:", fpath);

		buf[sb->st_size] = '\0';
		/*
		 * Check if symlink is absolute or relative; on the former
		 * make it absolute for the target object.
		 */
		if (strncmp(buf, "../", 3) == 0) {
			p = realpath(fpath, NULL);
			if (p == NULL) {
				/*
				 * This symlink points to an unexistent file,
				 * which might be provided in another package.
				 * So let's use the same target.
				 */
				xe->target = strdup(buf);
			} else {
				/*
				 * Sanitize destdir just in case.
				 */
				if ((p2 = realpath(destdir, NULL)) == NULL)
					die("failed to sanitize destdir %s: %s", destdir, strerror(errno));

				xe->target = strdup(p+strlen(p2));
				free(p2);
				free(p);
			}
		} else if (strchr(buf, '/') == NULL) {
			p = strdup(filep);
			assert(p);
			dname = dirname(p);
			assert(dname);
			xe->target = xbps_xasprintf("%s/%s", dname, buf);
			free(p);
		} else {
			xe->target = strdup(buf);
		}
		assert(xe->target);
		free(buf);
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

		if (sb->st_nlink <= 1)
			instsize += sb->st_size;

	} else if (type == FTW_D || type == FTW_DP) {
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
	xbps_array_t a;
	xbps_dictionary_t d;
	struct xentry *xe;
	char *p, *saveptr, *args, *tok;
	bool found = false, mutable_found = false;

	a = xbps_array_create();
	assert(a);

	TAILQ_FOREACH_REVERSE(xe, &xentry_list, xentry_head, entries) {
		if (strcmp(xe->type, key))
			continue;

		found = true;
		d = xbps_dictionary_create();
		assert(d);
		/* sanitize file path */
		p = strchr(xe->file, '.') + 1;
		/*
		 * Find out if this file is mutable.
		 */
		if (mutable_files) {
			if ((strchr(mutable_files, ' ') == NULL) &&
			    (strcmp(mutable_files, p) == 0))
				xbps_dictionary_set_bool(d, "mutable", true);
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
					xbps_dictionary_set_bool(d, "mutable",
					    true);
					mutable_found = false;
				}
			}
		}
		xbps_dictionary_set_cstring(d, "file", p);
		if (xe->target)
			xbps_dictionary_set_cstring(d, "target", xe->target);
		else if (xe->hash)
			xbps_dictionary_set_cstring(d, "sha256", xe->hash);
		xbps_array_add(a, d);
		xbps_object_release(d);
	}
	if (found)
		xbps_dictionary_set(pkg_filesd, key, a);

	xbps_object_release(a);
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
write_entry(struct archive *ar, struct archive_entry *entry)
{
	char buf[16384];
	const char *name;
	int fd = -1;
	off_t len;
	ssize_t buf_len;

	if (archive_entry_pathname(entry) == NULL)
		return;

	if (archive_write_header(ar, entry)) {
		die("cannot write %s to archive: %s",
		    archive_entry_pathname(entry),
		    archive_error_string(ar));
	}

	/* Only regular files can have data. */
	if (archive_entry_filetype(entry) != AE_IFREG ||
	    archive_entry_size(entry) == 0) {
		archive_entry_free(entry);
		return;
	}

	name = archive_entry_sourcepath(entry);
	fd = open(name, O_RDONLY);
	assert(fd != -1);

	len = archive_entry_size(entry);
	while (len > 0) {
		buf_len = (len > (off_t)sizeof(buf)) ?
			(ssize_t)sizeof(buf) : (ssize_t)len;

		if ((buf_len = read(fd, buf, buf_len)) == 0)
			break;
		else if (buf_len < 0)
			die("cannot read from %s", name);

		archive_write_data(ar, buf, (size_t)buf_len);
		len -= buf_len;
	}
	close(fd);

	archive_entry_free(entry);
}


static void
process_entry_file(struct archive *ar,
		   struct archive_entry_linkresolver *resolver,
		   struct xentry *xe, const char *filematch)
{
	struct archive_entry *entry, *sparse_entry;
	struct stat st;
	char buf[16384], *p;
	ssize_t len;

	assert(ar);
	assert(xe);

	if (filematch && strcmp(xe->file, filematch))
		return;

	p = xbps_xasprintf("%s/%s", destdir, xe->file);
	if (lstat(p, &st) == -1)
		die("failed to add entry (fstat) %s to archive:", xe->file);

	entry = archive_entry_new();
	assert(entry);
	archive_entry_set_pathname(entry, xe->file);
	if (st.st_uid == geteuid())
		st.st_uid = 0;
	if (st.st_gid == getegid())
		st.st_gid = 0;

	archive_entry_copy_stat(entry, &st);
	archive_entry_copy_sourcepath(entry, p);
	if (st.st_uid == geteuid())
		archive_entry_set_uname(entry, "root");
	if (st.st_gid == getegid())
		archive_entry_set_gname(entry, "root");

	if (S_ISLNK(st.st_mode)) {
		len = readlink(p, buf, sizeof(buf));
		if (len < 0 || len > st.st_size)
			die("failed to add entry %s (readlink) to archive:",
			    xe->file);
		buf[len] = '\0';
		archive_entry_set_symlink(entry, buf);
	}
	free(p);

	archive_entry_linkify(resolver, &entry, &sparse_entry);

	if (entry != NULL)
		write_entry(ar, entry);
	if (sparse_entry != NULL)
		write_entry(ar, sparse_entry);
}

static void
process_archive(struct archive *ar,
		struct archive_entry_linkresolver *resolver,
		const char *pkgver, bool quiet)
{
	struct xentry *xe;
	char *xml;

	/* Add INSTALL/REMOVE metadata scripts first */
	TAILQ_FOREACH(xe, &xentry_list, entries) {
		process_entry_file(ar, resolver, xe, "./INSTALL");
		process_entry_file(ar, resolver, xe, "./REMOVE");
	}
	/*
	 * Add the installed-size object.
	 */
	xbps_dictionary_set_uint64(pkg_propsd, "installed_size", instsize);

	/* Add props.plist metadata file */
	xml = xbps_dictionary_externalize(pkg_propsd);
	assert(xml);
	xbps_archive_append_buf(ar, xml, strlen(xml), "./props.plist",
	    0644, "root", "root");
	free(xml);

	/* Add files.plist metadata file */
	xml = xbps_dictionary_externalize(pkg_filesd);
	assert(xml);
	xbps_archive_append_buf(ar, xml, strlen(xml), "./files.plist",
	    0644, "root", "root");
	free(xml);

	/* Add all package data files and release resources */
	while ((xe = TAILQ_FIRST(&xentry_list)) != NULL) {
		TAILQ_REMOVE(&xentry_list, xe, entries);
		if ((strcmp(xe->type, "metadata") == 0) ||
		    (strcmp(xe->type, "dirs") == 0))
			continue;

		if (!quiet) {
			printf("%s: adding `%s' ...\n", pkgver, xe->file);
			fflush(stdout);
		}
		process_entry_file(ar, resolver, xe, NULL);
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

	if (!xbps_dictionary_set_cstring(pkg_propsd, "build-date", outstr))
		die("failed to add build-date object:");
}

int
main(int argc, char **argv)
{
	const char *shortopts = "A:B:C:D:F:G:H:hl:M:m:n:P:pqR:S:s:V";
	const struct option longopts[] = {
		{ "architecture", required_argument, NULL, 'A' },
		{ "built-with", required_argument, NULL, 'B' },
		{ "source-revisions", required_argument, NULL, 'G' },
		{ "conflicts", required_argument, NULL, 'C' },
		{ "dependencies", required_argument, NULL, 'D' },
		{ "config-files", required_argument, NULL, 'F' },
		{ "homepage", required_argument, NULL, 'H' },
		{ "help", no_argument, NULL, 'h' },
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
		{ "shlib-provides", required_argument, NULL, '0' },
		{ "shlib-requires", required_argument, NULL, '1' },
		{ "build-options", required_argument, NULL, '2' },
		{ NULL, 0, NULL, 0 }
	};
	struct archive *ar;
	struct archive_entry *entry, *sparse_entry;
	struct archive_entry_linkresolver *resolver;
	struct stat st;
	const char *conflicts, *deps, *homepage, *license, *maint, *bwith;
	const char *provides, *pkgver, *replaces, *desc, *ldesc;
	const char *arch, *config_files, *mutable_files, *version;
	const char *buildopts, *shlib_provides, *shlib_requires;
	const char *srcrevs = NULL;
	char *pkgname, *binpkg, *tname, *p, cwd[PATH_MAX-1];
	bool quiet = false, preserve = false;
	int c, pkg_fd;
	mode_t myumask;

	arch = conflicts = deps = homepage = license = maint = NULL;
	provides = pkgver = replaces = desc = ldesc = bwith = buildopts = NULL;
	config_files = mutable_files = shlib_provides = shlib_requires = NULL;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
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
		case 'F':
			config_files = optarg;
			break;
		case 'G':
			srcrevs = optarg;
			break;
		case 'h':
			usage();
			break;
		case 'H':
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
		case '0':
			shlib_provides = optarg;
			break;
		case '1':
			shlib_requires = optarg;
			break;
		case '2':
			buildopts = optarg;
			break;
		case '?':
		default:
			usage();
		}
	}
	if (argc == optind)
		usage();

	destdir = argv[optind];

	setlocale(LC_ALL, "");

	if (pkgver == NULL)
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
	pkg_propsd = xbps_dictionary_create();
	assert(pkg_propsd);

	/* Required properties */
	xbps_dictionary_set_cstring_nocopy(pkg_propsd, "architecture", arch);
	xbps_dictionary_set_cstring_nocopy(pkg_propsd, "pkgname", pkgname);
	xbps_dictionary_set_cstring_nocopy(pkg_propsd, "version", version);
	xbps_dictionary_set_cstring_nocopy(pkg_propsd, "pkgver", pkgver);
	xbps_dictionary_set_cstring_nocopy(pkg_propsd, "short_desc", desc);
	set_build_date();

	/* Optional properties */
	if (homepage)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"homepage", homepage);
	if (license)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"license", license);
	if (maint)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"maintainer", maint);
	if (ldesc)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"long_desc", ldesc);
	if (bwith)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"packaged-with", bwith);
	if (srcrevs)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"source-revisions", srcrevs);
	if (preserve)
		xbps_dictionary_set_bool(pkg_propsd, "preserve", true);
	if (buildopts)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"build-options", buildopts);

	/* Optional arrays */
	process_array("run_depends", deps);
	process_array("conf_files", config_files);
	process_array("conflicts", conflicts);
	process_array("provides", provides);
	process_array("replaces", replaces);
	process_array("shlib-provides", shlib_provides);
	process_array("shlib-requires", shlib_requires);

	/* save cwd */
	memset(&cwd, 0, sizeof(cwd));
	p = getcwd(cwd, sizeof(cwd));
	assert(p);

	if (chdir(destdir) == -1)
		die("cannot chdir() to destdir %s:", destdir);
	/*
	 * Process XBPS_PKGFILES metadata file.
	 */
	pkg_filesd = xbps_dictionary_create();
	assert(pkg_filesd);
	process_destdir(mutable_files);

	/* Back to original cwd after file tree walk processing */
	if (chdir(p) == -1)
		die("cannot chdir() to cwd %s:", cwd);

	/*
	 * Create a temp file to store archive data.
	 */
	tname = xbps_xasprintf(".xbps-pkg-XXXXXX");
	pkg_fd = mkstemp(tname);
	assert(pkg_fd != -1);
	/*
	 * Process the binary package's archive (ustar compressed with xz).
	 */
	ar = archive_write_new();
	assert(ar);
	archive_write_add_filter_xz(ar);
	archive_write_set_format_pax_restricted(ar);
	if ((resolver = archive_entry_linkresolver_new()) == NULL)
		die("cannot create link resolver");
	archive_entry_linkresolver_set_strategy(resolver,
	    archive_format(ar));

	archive_write_set_options(ar, "compression-level=9");
	if (archive_write_open_fd(ar, pkg_fd) != 0)
		die("Failed to open %s fd for writing:", tname);

	process_archive(ar, resolver, pkgver, quiet);
	/* Process hardlinks */
	entry = NULL;
	archive_entry_linkify(resolver, &entry, &sparse_entry);
	while (entry != NULL) {
		write_entry(ar, entry);
		entry = NULL;
		archive_entry_linkify(resolver, &entry, &sparse_entry);
	}
	archive_entry_linkresolver_free(resolver);
	/* close and free archive */
	archive_write_finish(ar);

	/*
	 * Archive was created successfully; flush data to storage,
	 * set permissions and rename to dest file; from the caller's
	 * perspective it's atomic.
	 */
	binpkg = xbps_xasprintf("%s.%s.xbps", pkgver, arch);

	(void)fsync(pkg_fd);
	myumask = umask(0);
	(void)umask(myumask);

	if (fchmod(pkg_fd, 0666 & ~myumask) == -1)
		die("cannot fchmod() %s:", tname);

	close(pkg_fd);

	if (rename(tname, binpkg) == -1)
		die("cannot rename %s to %s:", tname, binpkg);

	/* Success, release resources */
	if (!quiet)
		printf("%s: binary package created successfully (%s)\n",
		    pkgver, binpkg);

	exit(EXIT_SUCCESS);
}
