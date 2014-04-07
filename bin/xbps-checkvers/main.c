/*
 * Copyright (c) 2012-2014 Dave Elusive <davehome@redthumb.info.tm>
 * Copyright (c) 2014 Juan Romero Pardines
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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
 *
 */

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#include <xbps.h>

#ifndef __UNCONST
#define __UNCONST(a)    ((void *)(unsigned long)(const void *)(a))
#endif

#ifdef _RCV_DEBUG
# define _dprintf(...)							\
do {									\
	fprintf(stderr, "DEBUG => %s:%d in %s(): ",			\
		__FILE__, __LINE__, __PRETTY_FUNCTION__);		\
	fprintf(stderr, __VA_ARGS__);					\
} while (0)
#else
#define _dprintf(...)
#endif

typedef struct str_ptr_t {
	char *s;
	size_t len;
	int vmalloc;
} string;

typedef struct _map_item_t {
	string k, v;
	size_t i;
} map_item_t;

typedef struct _map_t {
	size_t size, len;
	map_item_t *items;
} map_t;

typedef struct _rcv_t {
	const char *prog, *fname;
	char *input, *ptr, *xbps_conf, *rootdir, *distdir, *pkgdir;
	size_t len, have_vars;
	map_t *env;
	struct xbps_handle xhp;
	xbps_dictionary_t pkgd;
	bool show_missing;
	bool manual;
	bool installed;
} rcv_t;

typedef int (*rcv_check_func)(rcv_t *);
typedef int (*rcv_proc_func)(rcv_t *, const char *, rcv_check_func);

static map_item_t
map_new_item(void)
{
	return (map_item_t){ .k = { NULL, 0, 0 }, .v = { NULL, 0, 0 } };
}

static map_t *
map_create(void)
{
	size_t i = 0;
	map_t *map = malloc(sizeof(map_t));
	map->size = 16;
	map->len = 0;
	map->items = calloc(map->size, sizeof(map_item_t));
	for (; i < map->size; i++) {
		map->items[i] = map_new_item();
	}
	return map;
}

static map_item_t
map_find_n(map_t *map, const char *k, size_t n)
{
	size_t i = 0;
	map_item_t item = map_new_item();

	if (map->len == 0)
		return item;

	while(i < map->len) {
		item = map->items[i++];
		if (item.k.len != 0)
			if ((strncmp(k, item.k.s, n) == 0))
				break;
	}
	return item;
}

static map_item_t
map_add_n(map_t *map, const char *k, size_t kn, const char *v, size_t vn)
{
	size_t i;
	map_item_t item;

	if (++map->len > map->size) {
		map->size += 16;
		map->items = realloc(map->items,
			sizeof(map_item_t)*(map->size));
		for (i = map->size - 10; i < map->size; i++) {
			map->items[i] = map_new_item();
		}
	}
	item = map_find_n(map, k, kn);
	if (item.k.len == 0) {
		item = map_new_item();
		item.k = (string){ (char *)__UNCONST(k), kn, 0 };
		item.i = map->len - 1;
	}
	if (item.v.vmalloc == 1)
		free(item.v.s);
	item.v = (string){ (char *)__UNCONST(v), vn, 0 };
	map->items[item.i] = item;
	return map->items[item.i];
}

static map_item_t
map_add(map_t *map, const char *k, const char *v)
{
	return map_add_n(map, k, strlen(k), v, strlen(v));
}


static map_item_t
map_find(map_t *map, const char *k)
{
	return map_find_n(map, k, strlen(k));
}

static void
map_destroy(map_t *map)
{
	size_t i = 0;
	while (i < map->len) {
		if (map->items[i].v.vmalloc == 1) {
			if (map->items[i].v.s != NULL) {
				free(map->items[i].v.s);
			}
		}
		i++;
	}
	free(map->items);
	free(map);
}

static int
show_usage(const char *prog)
{
	fprintf(stderr,
"Usage: %s [OPTIONS] [FILES...]\n\n"
" Options:\n"
"  -h,--help			Show this helpful help-message for help.\n"
"  -C,--xbps-conf FILENAME	Set (or override) the `xbps.conf' (which may\n"
"				have automatically been detected).\n"
"  -d,--xbps-packages DIRECTORY	Set (or override) the path to xbps-packages\n"
"				(defaults to ~/xbps-packages).\n"
"  -i,--installed 		Check for outdated packages in rootdir, rather\n"
"				than in the XBPS repositories.\n"
"  -r,--rootdir DIRECTORY	Set root directory (defaults to /).\n"
"  -s,--show-missing		List any binary packages which are not built.\n"
"\n  [FILES...]			Extra packages to process with the outdated\n"
"				ones (only processed if missing).\n\n",
prog);
	return EXIT_FAILURE;
}

static void
rcv_init(rcv_t *rcv, const char *prog)
{
	rcv->prog = prog;
	rcv->have_vars = 0;
	rcv->ptr = rcv->input = NULL;
	memset(&rcv->xhp, 0, sizeof(struct xbps_handle));
	if (rcv->xbps_conf != NULL)
		rcv->xhp.conffile = rcv->xbps_conf;
	if (rcv->rootdir != NULL)
		strncpy(rcv->xhp.rootdir, rcv->rootdir, sizeof(rcv->xhp.rootdir));
	xbps_init(&rcv->xhp);
}

static void
rcv_end(rcv_t *rcv)
{
	if (rcv->input != NULL) {
		free(rcv->input);
		rcv->input = NULL;
	}
	if (rcv->env != NULL) {
		map_destroy(rcv->env);
		rcv->env = NULL;
	}

	xbps_end(&rcv->xhp);

	if (rcv->xbps_conf != NULL)
		free(rcv->xbps_conf);
	if (rcv->distdir != NULL)
		free(rcv->distdir);
	if (rcv->pkgdir != NULL)
		free(rcv->pkgdir);
}

static bool
rcv_load_file(rcv_t *rcv, const char *fname)
{
	FILE *file;
	rcv->fname = fname;

	if ((file = fopen(rcv->fname, "r")) == NULL) {
		if (!rcv->manual) {
			fprintf(stderr, "FileError: can't open '%s': %s\n",
				rcv->fname, strerror(errno));
		}
		return false;
	}

	fseek(file, 0, SEEK_END);
	rcv->len = (size_t)ftell(file);
	fseek(file, 0, SEEK_SET);

	if (rcv->input != NULL)
		free(rcv->input);

	if ((rcv->input = calloc(rcv->len + 1, sizeof(char))) == NULL) {
		fprintf(stderr, "MemError: can't allocate memory: %s\n",
			strerror(errno));
		fclose(file);
		return false;
	}

	(void)fread(rcv->input, sizeof(char), rcv->len, file);
	rcv->input[rcv->len] = '\0';
	fclose(file);
	rcv->ptr = rcv->input;

	return true;
}

static char *
rcv_refs(rcv_t *rcv, const char *s, size_t len)
{
	map_item_t item = map_new_item();
	size_t i = 0, j = 0, k = 0, count = len*3;
	char *ref = calloc(count, sizeof(char));
	char *buf = calloc(count, sizeof(char));
	while (i < len) {
		if (s[i] == '$' && s[i+1] != '(') {
			j = 0;
			i++;
			if (s[i] == '{') { i++; }
			while (isalpha(s[i]) || s[i] == '_') {
				ref[j++] = s[i++];
			}
			if (s[i] == '}') { i++; }
			ref[j++] = '\0';
			item = map_find(rcv->env, ref);
			if ((strncmp(ref, item.k.s, strlen(ref)) == 0)) {
				buf = strcat(buf, item.v.s);
				k += item.v.len;
			} else {
				buf = strcat(buf, "NULL");
				k += 4;
			}
		} else {
			if (s[i] != '\n')
				buf[k++] = s[i++];
		}
	}
	buf[k] = '\0';
	free(ref);
	return buf;
}

static char *
rcv_cmd(rcv_t *rcv, const char *s, size_t len)
{
	int c, rv = 0;
	FILE *stream;
	size_t i = 0, j = 0, k = 0, count = len*3;
	char *cmd = calloc(count, sizeof(char));
	char *buf = calloc(count, sizeof(char));
	(void)rcv;
	while (i < len) {
		if (s[i] == '$' && s[i+1] != '{') {
			j = 0;
			i++;
			if (s[i] == '(') { i++; }
			while (s[i] != ')') { cmd[j++] = s[i++]; }
			if (s[i] == ')') { i++; }
			cmd[j++] = '\0';
			if ((stream = popen(cmd, "r")) == NULL)
				goto error;
			while ((c = fgetc(stream)) != EOF && c != '\n') {
				buf[k++] = (char)c;
			}
			rv = pclose(stream);
error:
			if (rv > 0 || errno > 0) {
				fprintf(stderr,
					"Shell cmd failed: '%s' for "
					"template '%s'",
					cmd, rcv->fname);
				if (errno > 0) {
					fprintf(stderr, ": %s\n",
						strerror(errno));
				}
				putchar('\n');
				exit(1);
			}

		} else {
			if (s[i] != '\n')
				buf[k++] = s[i++];
		}
	}
	buf[k] = '\0';
	free(cmd);
	free(__UNCONST(s));
	return buf;
}

static int
rcv_get_pkgver(rcv_t *rcv)
{
	size_t klen, vlen;
	map_item_t _item;
	map_item_t *item = NULL;
	char c, *ptr = rcv->ptr, *e, *p, *k, *v;

	while ((c = *ptr) != '\0') {
		if (c == '#') {
			while (*ptr++ != '\n');
			continue;
		}
		if (c == '\n') {
			ptr++;
			continue;
		}
		if (c == 'u' && (strncmp("unset", ptr, 5)) == 0) {
			goto end;
		}
		if (isalpha(c) || c == '_') {
			e = strchr(ptr, '=');
			p = strchr(ptr, '\n');
			k = ptr;
			v = e + 1;
			klen = strlen(k) - strlen(e);
			vlen = strlen(v) - strlen(p);
			if (v[0] == '"' && vlen == 1) {
				while (*ptr++ != '"');
				goto end;
			}
			if (v[0] == '"') { v++; vlen--; }
			if (v[vlen-1] == '"') { vlen--; }
			if (vlen == 0) { goto end; }
			_item = map_add_n(rcv->env, k, klen, v, vlen);
			item = &rcv->env->items[_item.i];
			if (strchr(v, '$')) {
				item->v.s = rcv_refs(rcv, item->v.s, item->v.len);
				item->v.len = strlen(item->v.s);
				item->v.vmalloc = 1;
			} else {
				item->v.vmalloc = 0;
			}
			if (strchr(item->v.s, '$') && item->v.vmalloc == 1) {
				item->v.s = rcv_cmd(rcv, item->v.s, item->v.len);
				item->v.len = strlen(item->v.s);
			}
			if ((strncmp("pkgname", k, klen) == 0) ||
			    (strncmp("version",  k, klen) == 0) ||
			    (strncmp("revision", k, klen) == 0)) {
				rcv->have_vars += 1;
			}
			/*printf("'%.*s':'%.*s'\n", item->k.len, item->k.s, item->v.len, item->v.s);*/
			if (rcv->have_vars > 2) break;
		}
		end:
		ptr = strchr(ptr, '\n') + 1;
	}

	return (rcv->have_vars > 2) ? 0 : 1;
}

static int
rcv_process_file(rcv_t *rcv, const char *fname, rcv_check_func check)
{

	rcv->env = map_create();
	rcv->have_vars = 0;

	if (!rcv_load_file(rcv, fname)) {
		map_destroy(rcv->env);
		rcv->env = NULL;
		return EXIT_FAILURE;
	}

	/*printf("Processing %s\n", fname);*/

	map_add(rcv->env, "HOME", getenv("HOME"));

	rcv_get_pkgver(rcv);

	check(rcv);

	map_destroy(rcv->env);
	rcv->env = NULL;

	return EXIT_SUCCESS;
}

static void
rcv_set_distdir(rcv_t *rcv, const char *distdir)
{
	if (rcv == NULL || distdir == NULL)
		return;

	rcv->distdir = strdup(distdir);
	rcv->pkgdir = strdup(distdir);
	rcv->pkgdir = realloc(rcv->pkgdir,
		sizeof(char)*(strlen(distdir)+strlen("/srcpkgs")+1));
	rcv->pkgdir = strcat(rcv->pkgdir, "/srcpkgs");
}

static void
rcv_find_conf(rcv_t *rcv)
{
	FILE *fp;
	const char **lp, *conf;

	const char *xbps_locs[] = {
		XBPS_SYSCONF_PATH "/xbps.conf",
		"/etc/xbps/xbps.conf",
		"/usr/local/etc/xbps/xbps.conf", NULL
	};

	if (!rcv->xbps_conf) {
		for (lp = xbps_locs; (conf = *lp++);) {
			if ((fp = fopen(conf, "r")) != NULL) {
				fclose(fp);
				rcv->xbps_conf = calloc(strlen(conf) + 1,
					sizeof(char));
				rcv->xbps_conf = strcpy(rcv->xbps_conf, conf);
				rcv->xbps_conf[strlen(conf)] = '\0';
				break;
			}
		}
	}
}

static int
rcv_check_version(rcv_t *rcv)
{
	map_item_t pkgname, version, revision;
	const char *repover = NULL;
	char _srcver[BUFSIZ] = { '\0' };
	char *srcver = _srcver;

	if (rcv->have_vars < 3) {
		printf("Error in '%s': missing '%s', '%s', or '%s' vars!\n",
			rcv->fname, "pkgname", "version", "revision");
		exit(EXIT_FAILURE);
	}

	pkgname = map_find(rcv->env, "pkgname");
	version = map_find(rcv->env, "version");
	revision = map_find(rcv->env, "revision");

	srcver = strncpy(srcver, pkgname.v.s, pkgname.v.len);
	if (rcv->installed)
		rcv->pkgd = xbps_pkgdb_get_pkg(&rcv->xhp, srcver);
	else
		rcv->pkgd = xbps_rpool_get_pkg(&rcv->xhp, srcver);

	srcver = strncat(srcver, "-", 1);
	srcver = strncat(srcver, version.v.s, version.v.len);
	srcver = strncat(srcver, "_", 1);
	srcver = strncat(srcver, revision.v.s, revision.v.len);
	xbps_dictionary_get_cstring_nocopy(rcv->pkgd, "pkgver", &repover);
	if (repover == NULL && (rcv->show_missing==true||rcv->manual==true)) {
		printf("pkgname: %.*s repover: ? srcpkgver: %s\n",
			(int)pkgname.v.len, pkgname.v.s, srcver+pkgname.v.len+1);
	}
	if (repover != NULL && rcv->show_missing == false) {
		if (xbps_cmpver(repover+pkgname.v.len+1,
		    srcver+pkgname.v.len+1) < 0) {
			printf("pkgname: %.*s repover: %s srcpkgver: %s\n",
				(int)pkgname.v.len, pkgname.v.s,
				repover+pkgname.v.len+1,
				srcver+pkgname.v.len+1);
		}
	}
	return 0;
}

static int
rcv_process_dir(rcv_t *rcv, const char *path, rcv_proc_func process)
{
	DIR *dir;
	struct dirent entry, *result;
	struct stat st;
	char filename[BUFSIZ];
	int i, ret = 0, errors = 0;

	dir = opendir(path);
error:
	if (errors > 0) {
		fprintf(stderr, "Error: while processing dir '%s': %s\n", path,
			strerror(errors));
		exit(1);
	}

	if ((chdir(path)) == -1) {
		errors = errno;
		goto error;
	}
	while(1) {
		i = readdir_r(dir, &entry, &result);
		if (i > 0) {
			errors = errno;
			goto error;
		}
		if (result == NULL) break;
		if (strcmp(result->d_name, ".") == 0) continue;
		if (strcmp(result->d_name, "..") == 0) continue;
		if ((lstat(result->d_name, &st)) != 0) {
			errors = errno;
			goto error;
		}
		if (S_ISLNK(st.st_mode) != 0) continue;
		if ((chdir("..")) == -1) {
			errors = errno;
			goto error;
		}
		strcpy(filename, "srcpkgs/");
		strcat(filename, result->d_name);
		strcat(filename, "/template");
		ret = process(rcv, filename, rcv_check_version);
		if ((chdir(path)) == -1) {
			errors = errno;
			goto error;
		}
	}

	if ((closedir(dir)) == -1) {
		errors = errno;
		goto error;
	}
	if ((chdir("..")) == -1) {
		errors = errno;
		goto error;
	}

	return ret;
}

int
main(int argc, char **argv)
{
	int i, c;
	rcv_t rcv;
	char *distdir = NULL;
	const char *prog = argv[0], *sopts = "hC:d:ir:sV", *tmpl;
	const struct option lopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "xbps-conf", required_argument, NULL, 'C' },
		{ "distdir", required_argument, NULL, 'd' },
		{ "installed", no_argument, NULL, 'i' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "show-missing", no_argument, NULL, 's' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	memset(&rcv, 0, sizeof(rcv));

	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			return show_usage(prog);
		case 'C':
			free(rcv.xbps_conf);
			rcv.xbps_conf = strdup(optarg);
			break;
		case 'd':
			free(rcv.distdir); rcv.distdir = NULL;
			free(rcv.pkgdir); rcv.pkgdir = NULL;
			rcv_set_distdir(&rcv, optarg);
			break;
		case 'i':
			rcv.installed = true;
			break;
		case 'r':
			rcv.rootdir = strdup(optarg);
			break;
		case 's':
			rcv.show_missing = true;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		default:
			return show_usage(prog);
		}
	}
	/*
	 * If --distdir not set default to ~/xbps-packages.
	 */
	if (rcv.distdir == NULL) {
		distdir = xbps_xasprintf("%s/xbps-packages", getenv("HOME"));
		rcv_set_distdir(&rcv, distdir);
		free(distdir);
	}

	argc -= optind;
	argv += optind;

	rcv_find_conf(&rcv);
	rcv_init(&rcv, prog);
	rcv.manual = false;
	rcv_process_dir(&rcv, rcv.pkgdir, rcv_process_file);
	rcv.manual = true;
	if (argc > 0) {
		for(i = 0; i < argc; i++) {
			tmpl = argv[i] + (strlen(argv[i]) - strlen("template"));
			if ((strcmp("template", tmpl)) == 0) {
				rcv_process_file(&rcv, argv[i],
					rcv_check_version);
			}
		}
	}
	rcv_end(&rcv);

	return 0;
}
