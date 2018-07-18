/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
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
#include <dirent.h>

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
	uint64_t mtime;
	char *file, *type, *target, *hash;
	ino_t inode;
};

static TAILQ_HEAD(xentry_head, xentry) xentry_list =
    TAILQ_HEAD_INITIALIZER(xentry_list);

static uint64_t instsize;
static xbps_dictionary_t pkg_propsd, pkg_filesd, all_filesd;
static const char *destdir;

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stdout,
	"Usage: %s [OPTIONS] -A <arch> -n <pkgver> -s \"<desc>\" destdir\n\n"
	"OPTIONS\n"
	" -A --architecture   Package architecture (e.g: noarch, i686, etc).\n"
	" -B --built-with     Package builder string (e.g: xbps-src-30).\n"
	" -C --conflicts      Conflicts (blank separated list,\n"
	"                     e.g: 'foo>=2.0 blah<=2.0').\n"
	" -c --changelog      Changelog URL.\n"
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
	" -r --reverts        Reverts (blank separated list,\n"
	"                     e.g: '1.0_1 2.0_3').\n"
	" -S --long-desc      Long description (80 cols per line).\n"
	" -s --desc           Short description (max 80 characters).\n"
	" -t --tags           A list of tags/categories (blank separated list).\n"
	" -V --version        Prints XBPS release version.\n"
	" --alternatives      List of available alternatives this pkg provides.\n"
	"                     This expects a blank separated list of <name>:<symlink>:<target>, e.g\n"
	"                     'vi:/usr/bin/vi:/usr/bin/vim foo:/usr/bin/foo:/usr/bin/blah'.\n"
	" --build-options     A string with the used build options.\n"
	" --compression       Compression format: none, gzip, bzip2, xz (default).\n"
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
	xbps_array_t array = NULL;
	char *args, *p = NULL, *saveptr = NULL;

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
		xbps_array_add_cstring(array, p);
	}
	free(args);
out:
	xbps_dictionary_set(pkg_propsd, key, array);
	xbps_object_release(array);
}

static void
process_one_alternative(const char *altgrname, const char *val)
{
	xbps_dictionary_t d;
	xbps_array_t a;
	char *altfiles;
	bool alloc = false;

	if ((d = xbps_dictionary_get(pkg_propsd, "alternatives")) == NULL) {
		d = xbps_dictionary_create();
		assert(d);
		alloc = true;
	}
	if ((a = xbps_dictionary_get(d, altgrname)) == NULL) {
		a = xbps_array_create();
		assert(a);
	}
	altfiles = strchr(val, ':') + 1;
	assert(altfiles);

	xbps_array_add_cstring(a, altfiles);
	xbps_dictionary_set(d, altgrname, a);
	xbps_dictionary_set(pkg_propsd, "alternatives", d);

	if (alloc) {
		xbps_object_release(a);
		xbps_object_release(d);
	}
}


static void
process_dict_of_arrays(const char *key UNUSED, const char *val)
{
	char *altgrname, *args, *p, *saveptr;

	assert(key);

	if (val == NULL)
		return;

	args = strdup(val);
	assert(args);

	if (strchr(args, ' ') == NULL) {
		altgrname = strtok(args, ":");
		assert(altgrname);
		process_one_alternative(altgrname, val);
		goto out;
	}

	for ((p = strtok_r(args, " ", &saveptr)); p;
	     (p = strtok_r(NULL, " ", &saveptr))) {
		char *b;

		b = strdup(p);
		assert(b);
		altgrname = strtok(b, ":");
		assert(altgrname);
		process_one_alternative(altgrname, p);
		free(b);
	}
out:
	free(args);
}

static void
process_file(const char *file, const char *key)
{
	void *blob;
	FILE *f;
	struct stat st;
	size_t len;
	xbps_data_t data;

	if ((f = fopen(file, "r")) == NULL)
		return;

	if (fstat(fileno(f), &st) == -1) {
		fclose(f);
		die("lstat %s", file);
	}

	if (S_ISREG(st.st_mode) == 0) {
		fclose(f);
		return;
	}

	len = st.st_size;

	if ((blob = malloc(len)) == NULL) {
	        fclose(f);
		die("malloc %s", file);
	}

	if (fread(blob, len, 1, f) != len) {
		if (ferror(f)) {
			fclose(f);
			die("fread %s", file);
		}
	}
	fclose(f);

	if ((data = xbps_data_create_data(blob, len)) == NULL)
		die("xbps_data_create_data %s", file);

	free(blob);

	if (!xbps_dictionary_set(pkg_propsd, key, data))
		die("xbps_dictionary_set %s", key);

	xbps_object_release(data);
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
ftw_cb(const char *fpath, const struct stat *sb, const struct dirent *dir UNUSED)
{
	struct xentry *xe = NULL;
	xbps_dictionary_t fileinfo = NULL;
	const char *filep = NULL;
	char *buf, *p, *p2, *dname;
	ssize_t r;

	/* Ignore metadata files generated by xbps-src and destdir */
	if ((strcmp(fpath, ".") == 0) ||
	    (strcmp(fpath, "./INSTALL.msg") == 0) ||
	    (strcmp(fpath, "./REMOVE.msg") == 0) ||
	    (strcmp(fpath, "./props.plist") == 0) ||
	    (strcmp(fpath, "./files.plist") == 0) ||
	    (strcmp(fpath, "./flist") == 0) ||
	    (strcmp(fpath, "./rdeps") == 0) ||
	    (strcmp(fpath, "./shlib-provides") == 0) ||
	    (strcmp(fpath, "./shlib-requires") == 0))
		return 0;

	/* sanitized file path */
	filep = strchr(fpath, '.') + 1;
	fileinfo = xbps_dictionary_create();
	xe = calloc(1, sizeof(*xe));
	assert(xe);
	/* XXX: fileinfo contains the sanatized path, whereas xe contains the
	 * unsanatized path!
	 *
	 * when handing the files over, do not use the dictionary directly. Instead
	 * use the keysym, as this value has the unsanatized path.
	 */
	xbps_dictionary_set_cstring(fileinfo, "file", filep);
	xbps_dictionary_set(all_filesd, fpath, fileinfo);
	xe->file = strdup(fpath);
	assert(xe->file);

	if ((strcmp(fpath, "./INSTALL") == 0) ||
	    (strcmp(fpath, "./REMOVE") == 0)) {
		/* metadata file */
		xbps_dictionary_set_cstring_nocopy(fileinfo, "type", "metadata");
		xe->type = strdup("metadata");
		assert(xe->type);
		goto out;
	}

	if (S_ISLNK(sb->st_mode)) {
		/*
		 * Symlinks.
		 *
		 * Find out target file.
		 */
		xbps_dictionary_set_cstring_nocopy(fileinfo, "type", "links");
		xe->type = strdup("links");
		assert(xe->type);
		/* store modification time for regular files and links */
		xbps_dictionary_set_cstring_nocopy(fileinfo, "type", "links");
		xe->mtime = (uint64_t)sb->st_mtime;
		xbps_dictionary_set_uint64(fileinfo, "mtime", (uint64_t)sb->st_mtime);
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
		if (strstr(buf, "./")) {
			p = realpath(fpath, NULL);
			if (p == NULL) {
				/*
				 * This symlink points to an unexistent file,
				 * which might be provided in another package.
				 * So let's use the same target.
				 */
				xe->target = strdup(buf);
				xbps_dictionary_set_cstring(fileinfo, "target", buf);
			} else {
				/*
				 * Sanitize destdir just in case.
				 */
				if ((p2 = realpath(destdir, NULL)) == NULL)
					die("failed to sanitize destdir %s: %s", destdir, strerror(errno));

				xe->target = strdup(p+strlen(p2));
				xbps_dictionary_set_cstring(fileinfo, "target", p+strlen(p2));
				free(p2);
				free(p);
			}
		} else if (buf[0] != '/') {
			/* relative path */
			p = strdup(filep);
			assert(p);
			dname = dirname(p);
			assert(dname);
			xe->target = xbps_xasprintf("%s/%s", dname, buf);
			p2 = xbps_xasprintf("%s/%s", dname, buf);
			xbps_dictionary_set_cstring(fileinfo, "target", p2);
			free(p2);
			free(p);
		} else {
			xe->target = strdup(buf);
			xbps_dictionary_set_cstring(fileinfo, "target", buf);
		}
		assert(xe->target);
		assert(xbps_dictionary_get(fileinfo, "target"));
		free(buf);
	} else if (S_ISREG(sb->st_mode)) {
		struct xentry *xep;
		bool hlink = false;
		xbps_object_iterator_t iter;
		xbps_object_t obj;
		xbps_dictionary_t linkinfo;
		uint64_t inode = 0;
		/*
		 * Regular files. First find out if it's a hardlink:
		 * 	- st_nlink > 1
		 * and then search for a stored file matching its inode.
		 */
		TAILQ_FOREACH(xep, &xentry_list, entries) {
			if (sb->st_nlink > 1 && xep->inode == sb->st_ino) {
				/* matched */
				printf("%"PRIu64" %"PRIu64"\n", xep->inode, sb->st_ino);
				hlink = true;
				break;
			}
		}

		iter = xbps_dictionary_iterator(all_filesd);
		assert(iter);
		while ((obj = xbps_object_iterator_next(iter))) {
			if (sb->st_nlink <= 1)
				continue;
			linkinfo = xbps_dictionary_get_keysym(all_filesd, obj);
			xbps_dictionary_get_uint64(linkinfo, "inode", &inode);
			if (inode == sb->st_ino) {
				/* matched */
				printf("%"PRIu64" %"PRIu64"\n", inode, sb->st_ino);
				break;
			}
		}
		if (!hlink != (inode != sb->st_ino))
			die("Inconsistent results from xbps_dictionary_t and linked list!\n");

		if (inode != sb->st_ino)
			instsize += sb->st_size;
		xbps_object_iterator_release(iter);

		/*
		 * Find out if it's a configuration file or not
		 * and calculate sha256 hash.
		 */
		if (entry_is_conf_file(filep)) {
			xbps_dictionary_set_cstring_nocopy(fileinfo, "type", "conf_files");
			xe->type = strdup("conf_files");
		} else {
			xbps_dictionary_set_cstring_nocopy(fileinfo, "type", "files");
			xe->type = strdup("files");
		}

		assert(xe->type);
		if ((p = xbps_file_hash(fpath)) == NULL)
			die("failed to process hash for %s:", fpath);
		xbps_dictionary_set_cstring(fileinfo, "sha256", p);
		free(p);
		if ((xe->hash = xbps_file_hash(fpath)) == NULL)
			die("failed to process hash for %s:", fpath);

		xbps_dictionary_set_uint64(fileinfo, "inode", sb->st_ino);
		xe->inode = sb->st_ino;
		/* store modification time for regular files and links */
		xbps_dictionary_set_uint64(fileinfo, "mtime", sb->st_mtime);
		xe->mtime = (uint64_t)sb->st_mtime;

	} else if (S_ISDIR(sb->st_mode)) {
		/* directory */
		xbps_dictionary_set_cstring_nocopy(fileinfo, "type", "dirs");
		xe->type = strdup("dirs");
		assert(xe->type);
	}

out:
	xbps_object_release(fileinfo);
	TAILQ_INSERT_TAIL(&xentry_list, xe, entries);
	return 0;
}

static int
walk_dir(const char *path,
		int (*fn) (const char *, const struct stat *sb, const struct dirent *dir))
{
	int rv, i;
	struct dirent **list;
	char tmp_path[PATH_MAX] = { 0 };
	struct stat sb;

	rv = scandir(path, &list, NULL, alphasort);
	for (i = rv - 1; i >= 0; i--) {
		if (strcmp(list[i]->d_name, ".") == 0 || strcmp(list[i]->d_name, "..") == 0)
			continue;
		if (strlen(path) + strlen(list[i]->d_name) + 1 >= PATH_MAX - 1) {
			errno = ENAMETOOLONG;
			rv = -1;
			break;
		}
		strncpy(tmp_path, path, PATH_MAX - 1);
		strncat(tmp_path, "/", PATH_MAX - 1 - strlen(tmp_path));
		strncat(tmp_path, list[i]->d_name, PATH_MAX - 1 - strlen(tmp_path));
		if (lstat(tmp_path, &sb) < 0) {
			break;
		}

		if (S_ISDIR(sb.st_mode)) {
			if (walk_dir(tmp_path, fn) < 0) {
				rv = -1;
				break;
			}
		}

		rv = fn(tmp_path, &sb, list[i]);
		if (rv != 0) {
			break;
		}

	}
	free(list);
	return rv;
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
		if (xe->hash)
			xbps_dictionary_set_cstring(d, "sha256", xe->hash);
		if (xe->mtime)
			xbps_dictionary_set_uint64(d, "mtime", xe->mtime);

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
	if (walk_dir(".", ftw_cb) < 0)
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
	const char *name;
	int fd;
	char buf[65536];
	ssize_t len;

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

	if ((fd = open(name, O_RDONLY)) < 0)
		die("cannot open %s file", name);
	while ((len = read(fd, buf, sizeof(buf))) > 0)
		archive_write_data(ar, buf, len);
	(void)close(fd);

	if(len < 0)
		die("cannot open %s file", name);

	archive_entry_free(entry);
}


static void
process_entry_file(struct archive *ar,
		   struct archive_entry_linkresolver *resolver,
		   struct xentry *xe, const char *filematch)
{
	struct archive_entry *entry, *sparse_entry;
	struct stat st;
	char *buf = NULL, *p;
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
		buf = malloc(st.st_size+1);
		assert(buf);
		len = readlink(p, buf, st.st_size+1);
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
	if (buf)
		free(buf);
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
	if (!xbps_dictionary_set_uint64(pkg_propsd, "installed_size", instsize))
		die("%s: failed to set installed_size obj!");

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

int
main(int argc, char **argv)
{
	const char *shortopts = "A:B:C:c:D:F:G:H:hl:M:m:n:P:pqr:R:S:s:t:V";
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
		{ "reverts", required_argument, NULL, 'r' },
		{ "long-desc", required_argument, NULL, 'S' },
		{ "desc", required_argument, NULL, 's' },
		{ "tags", required_argument, NULL, 't' },
		{ "version", no_argument, NULL, 'V' },
		{ "shlib-provides", required_argument, NULL, '0' },
		{ "shlib-requires", required_argument, NULL, '1' },
		{ "build-options", required_argument, NULL, '2' },
		{ "compression", required_argument, NULL, '3' },
		{ "alternatives", required_argument, NULL, '4' },
		{ "changelog", required_argument, NULL, 'c'},
		{ NULL, 0, NULL, 0 }
	};
	struct archive *ar;
	struct archive_entry *entry, *sparse_entry;
	struct archive_entry_linkresolver *resolver;
	struct stat st;
	const char *conflicts, *deps, *homepage, *license, *maint, *bwith;
	const char *provides, *pkgver, *replaces, *reverts, *desc, *ldesc;
	const char *arch, *config_files, *mutable_files, *version, *changelog;
	const char *buildopts, *shlib_provides, *shlib_requires, *alternatives;
	const char *compression, *tags = NULL, *srcrevs = NULL;
	char *pkgname, *binpkg, *tname, *p, cwd[PATH_MAX-1];
	bool quiet = false, preserve = false;
	int c, pkg_fd;
	mode_t myumask;

	arch = conflicts = deps = homepage = license = maint = compression = NULL;
	provides = pkgver = replaces = reverts = desc = ldesc = bwith = NULL;
	buildopts = config_files = mutable_files = shlib_provides = NULL;
	alternatives = shlib_requires = changelog = NULL;

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
		case 'c':
			changelog = optarg;
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
		case 'r':
			reverts = optarg;
			break;
		case 'S':
			ldesc = optarg;
			break;
		case 's':
			desc = optarg;
			break;
		case 't':
			tags = optarg;
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
		case '3':
			compression = optarg;
			break;
		case '4':
			alternatives = optarg;
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
	if (tags)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"tags", tags);
	if (preserve)
		xbps_dictionary_set_bool(pkg_propsd, "preserve", true);
	if (buildopts)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"build-options", buildopts);
	if (changelog)
		xbps_dictionary_set_cstring_nocopy(pkg_propsd,
				"changelog", changelog);

	/* Optional arrays */
	process_array("run_depends", deps);
	process_array("conf_files", config_files);
	process_array("conflicts", conflicts);
	process_array("provides", provides);
	process_array("replaces", replaces);
	process_array("reverts", reverts);
	process_array("shlib-provides", shlib_provides);
	process_array("shlib-requires", shlib_requires);
	process_dict_of_arrays("alternatives", alternatives);

	/* save cwd */
	memset(&cwd, 0, sizeof(cwd));
	p = getcwd(cwd, sizeof(cwd));
	assert(p);

	if (chdir(destdir) == -1)
		die("cannot chdir() to destdir %s:", destdir);

	/* Optional INSTALL/REMOVE messages */
	process_file("INSTALL.msg", "install-msg");
	process_file("REMOVE.msg", "remove-msg");
	/*
	 * Process XBPS_PKGFILES metadata file.
	 */
	pkg_filesd = xbps_dictionary_create();
	assert(pkg_filesd);
	all_filesd = xbps_dictionary_create();
	assert(all_filesd);
	process_destdir(mutable_files);

	/* Back to original cwd after file tree walk processing */
	if (chdir(p) == -1)
		die("cannot chdir() to cwd %s:", cwd);

	/*
	 * Create a temp file to store archive data.
	 */
	tname = xbps_xasprintf(".xbps-pkg-XXXXXXXXX");
	myumask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
	pkg_fd = mkstemp(tname);
	assert(pkg_fd != -1);
	umask(myumask);
	/*
	 * Process the binary package's archive (ustar compressed with xz).
	 */
	ar = archive_write_new();
	assert(ar);
	/*
	 * Set compression format, xz if unset.
	 */
	if (compression == NULL || strcmp(compression, "xz") == 0) {
		archive_write_add_filter_xz(ar);
	} else if (strcmp(compression, "gzip") == 0) {
		archive_write_add_filter_gzip(ar);
		archive_write_set_options(ar, "compression-level=9");
	} else if (strcmp(compression, "bzip2") == 0) {
		archive_write_add_filter_bzip2(ar);
		archive_write_set_options(ar, "compression-level=9");
	} else if (strcmp(compression, "none") == 0) {
		/* empty */
	} else {
		die("unknown compression format %s");
	}

	archive_write_set_format_pax_restricted(ar);
	if ((resolver = archive_entry_linkresolver_new()) == NULL)
		die("cannot create link resolver");
	archive_entry_linkresolver_set_strategy(resolver,
	    archive_format(ar));

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

#ifdef HAVE_FDATASYNC
	(void)fdatasync(pkg_fd);
#else
	(void)fsync(pkg_fd);
#endif
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
