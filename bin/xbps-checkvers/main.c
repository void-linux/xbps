/*
 * Copyright (c) 2014-2019 Juan Romero Pardines
 * Copyright (c) 2012-2014 Dave Elusive <davehome@redthumb.info.tm>
 * Copyright (c) 2019      Duncan Overbruck <mail@duncano.de>
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
#include <limits.h>
#include <sys/stat.h>
#include <assert.h>

#include <xbps.h>

#ifndef __UNCONST
#define __UNCONST(a)    ((void *)(unsigned long)(const void *)(a))
#endif

#define GOT_PKGNAME_VAR 	0x1
#define GOT_VERSION_VAR 	0x2
#define GOT_REVISION_VAR 	0x4

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

#define SBUFSZ		128
#define ALIGN(n, a)	(((n) + (a) - 1) & ~((a) - 1))
#define MAX(a, b)	((a) < (b) ? (b) : (a))
#define NEXTSZ(o, r)	ALIGN(MAX((o) * 2, (o) + (r)), SBUFSZ)

struct sbuf {
	char *mem;
	size_t len;
	size_t size;
};

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
	uint8_t have_vars;
	size_t len;
	map_t *env;
	struct xbps_handle xhp;
	xbps_dictionary_t pkgd;
	bool show_all;
	bool manual;
	bool installed;
	const char *format;
} rcv_t;

typedef int (*rcv_check_func)(rcv_t *);
typedef int (*rcv_proc_func)(rcv_t *, const char *, rcv_check_func);

static void
sbuf_extend(struct sbuf *sb, int newsz)
{
	sb->size = newsz;
	sb->mem = realloc(sb->mem, newsz);
	if (sb->mem == NULL) {
		fprintf(stderr, "realloc: %s\n", strerror(errno));
		exit(1);
	}
}

static struct sbuf *
sbuf_make(void)
{
	struct sbuf *sb = calloc(1, sizeof(*sb));
	if (sb == NULL) {
		fprintf(stderr, "calloc: %s\n", strerror(errno));
		exit(1);
	}
	return sb;
}

static char *
sbuf_buf(struct sbuf *sb)
{
	if (!sb->mem)
		sbuf_extend(sb, 1);
	sb->mem[sb->len] = '\0';
	return sb->mem;
}

static size_t
sbuf_done(struct sbuf *sb, char **dest)
{
	size_t len = sb->len;
	*dest = sbuf_buf(sb);
	free(sb);
	return len;
}

static void
sbuf_chr(struct sbuf *sb, int c)
{
	if (sb->len + 2 >= sb->size)
		sbuf_extend(sb, NEXTSZ(sb->size, 1));
	sb->mem[sb->len++] = c;
}

static void
sbuf_mem(struct sbuf *sb, const char *src, size_t len)
{
	if (sb->len + len + 1 >= sb->size)
		sbuf_extend(sb, NEXTSZ(sb->size, len + 1));
	memcpy(sb->mem + sb->len, src, len);
	sb->len += len;
}

static void
sbuf_str(struct sbuf *sb, const char *src)
{
	sbuf_mem(sb, src, strlen(src));
}

#if 0
static void
sbuf_strn(struct sbuf *sb, const char *src, size_t n)
{
	sbuf_mem(sb, src, n);
}
#endif

static map_item_t
map_new_item(void)
{
	return (map_item_t){ .k = { NULL, 0, 0 }, .v = { NULL, 0, 0 }, .i = 0 };
}

static map_t *
map_create(void)
{
	size_t i = 0;
	map_t *map = malloc(sizeof(map_t));
	if (map == NULL)
		return NULL;

	map->size = 16;
	map->len = 0;
	map->items = calloc(map->size, sizeof(map_item_t));
	assert(map->items);
	for (; i < map->size; i++) {
		map->items[i] = map_new_item();
	}
	return map;
}

static map_item_t
map_find_n(map_t *map, const char *k, size_t n)
{
	size_t i = 0;
	map_item_t item;

	for (i = 0; i < map->len; i++) {
		item = map->items[i];
		if (item.k.len != 0)
			if ((strncmp(k, item.k.s, n) == 0))
				break;
	}
	if (map->len == i)
		return map_new_item();

	return item;
}

static map_item_t
map_add_n(map_t *map, const char *k, size_t kn, const char *v, size_t vn)
{
	size_t i;
	map_item_t item;

	assert(k);
	assert(v);

	if (++map->len > map->size) {
		map->size += 16;
		map->items = realloc(map->items,
			sizeof(map_item_t)*(map->size));
		assert(map->items);
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
"OPTIONS:\n"
" -h --help		Show this helpful help-message for help.\n"
" -C --config <dir> 	Set path to xbps.d\n"
" -D --distdir <dir>	Set (or override) the path to void-packages\n"
"  			(defaults to ~/void-packages).\n"
" -d --debug 		Enable debug output to stderr.\n"
" -f --format <fmt>	Output format.\n"
" -I --installed 	Check for outdated packages in rootdir, rather\n"
"  			than in the XBPS repositories.\n"
" -i --ignore-conf-repos	Ignore repositories defined in xbps.d.\n"
" -m --manual		Only process listed files.\n"
" -R --repository=<url>	Append repository to the head of repository list.\n"
" -r --rootdir <dir>	Set root directory (defaults to /).\n"
" -s --show-all		List all packages, in the format 'pkgname repover srcver'.\n"
"\n  [FILES...]		Extra packages to process with the outdated\n"
"			ones (only processed if missing).\n\n",
prog);
	return EXIT_FAILURE;
}

static void
rcv_init(rcv_t *rcv, const char *prog)
{
	rcv->prog = prog;
	rcv->have_vars = 0;
	rcv->ptr = rcv->input = NULL;
	if (rcv->xbps_conf != NULL) {
		xbps_strlcpy(rcv->xhp.confdir, rcv->xbps_conf, sizeof(rcv->xhp.confdir));
	}
	if (rcv->rootdir != NULL) {
		xbps_strlcpy(rcv->xhp.rootdir, rcv->rootdir, sizeof(rcv->xhp.rootdir));
	}
	if (xbps_init(&rcv->xhp) != 0)
		abort();
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
	long offset;
	rcv->fname = fname;

	if ((file = fopen(rcv->fname, "r")) == NULL) {
		if (!rcv->manual) {
			fprintf(stderr, "FileError: can't open '%s': %s\n",
				rcv->fname, strerror(errno));
		}
		return false;
	}

	fseek(file, 0, SEEK_END);
	offset = ftell(file);
	fseek(file, 0, SEEK_SET);

	if (offset == -1) {
		fclose(file);
		return false;
	}
	rcv->len = (size_t)offset;

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

static size_t
rcv_sh_substitute(rcv_t *rcv, const char *str, size_t len, char **outp)
{
	const char *p;
	char *cmd;
	struct sbuf *out = sbuf_make();

	for (p = str; *p && p < str+len; p++) {
		switch (*p) {
		case '$':
			if (p+1 < str+len) {
				const char *ref;
				size_t reflen;
				map_item_t item;
				p++;
				if (*p == '(') {
					FILE *fp;
					char c;
					for (ref = ++p; *p && p < str+len && *p != ')'; p++)
						;
					if (*p != ')')
						goto err1;
					cmd = strndup(ref, p-ref);
					if ((fp = popen(cmd, "r")) == NULL)
						goto err2;
					while ((c = fgetc(fp)) != EOF && c != '\n')
						sbuf_chr(out, c);
					if (pclose(fp) != 0)
						goto err2;
					free(cmd);
					cmd = NULL;
					continue;
				} else if (*p == '{') {
					for (ref = ++p; *p && p < str+len && (isalnum(*p) || *p == '_'); p++)
						;
					reflen = p-ref;
					switch (*p) {
					case '/': /* fallthrough */
					case '%': /* fallthrough */
					case '#': /* fallthrough */
					case ':':
						for (; *p && p < str+len && *p != '}'; p++)
							;
						if (*p != '}')
							goto err1;
						break;
					case '}':
						break;
					default:
						goto err1;
					}
				} else {
					for (ref = p; *p && p < str+len && (isalnum(*p) || *p == '_'); p++)
						;
					reflen = p-ref;
					p--;
				}
				item = map_find_n(rcv->env, ref, reflen);
				if ((strncmp(ref, item.k.s, reflen) == 0)) {
					sbuf_mem(out, item.v.s, item.v.len);
				} else {
					sbuf_str(out, "NULL");
				}
				break;
			}
			/* fallthrough */
		default:
			sbuf_chr(out, *p);
		}
	}
	return sbuf_done(out, outp);

err1:
	fprintf(stderr, "syntax error: in file '%s'\n", rcv->fname);
	exit(1);
err2:
	fprintf(stderr,
		"Shell cmd failed: '%s' for "
		"template '%s'",
		cmd, rcv->fname);
	if (errno > 0) {
		fprintf(stderr, ": %s\n",
			strerror(errno));
	} else {
		fputc('\n', stderr);
	}
	exit(1);
}

static void
rcv_get_pkgver(rcv_t *rcv)
{
	size_t klen, vlen;
	map_item_t _item;
	map_item_t *item = NULL;
	char c, *ptr = rcv->ptr, *e, *p, *k, *v, *comment, *d;
	uint8_t vars = 0;

	while ((c = *ptr) != '\0') {
		if (c == '#' || c == '.') {
			goto nextline;
		}
		if (c == '\n') {
			ptr++;
			continue;
		}
		if (c == 'u' && (strncmp("unset", ptr, 5)) == 0) {
			goto nextline;
		}
		if ((e = strchr(ptr, '=')) == NULL)
			goto nextline;

		p = strchr(ptr, '\n');
		k = ptr;
		v = e + 1;

		assert(p);
		assert(k);
		assert(v);

		klen = strlen(k) - strlen(e);
		vlen = strlen(v) - strlen(p);

		if (v[0] == '"' && vlen == 1) {
			while (*ptr++ != '"')
				;
			goto nextline;
		}
		if (v[0] == '"') {
			v++;
			vlen--;
		}
		if (v[vlen-1] == '"') {
			vlen--;
		}
		comment = strchr(v, '#');
		if (comment && comment < p && (comment > v && comment[-1] == ' ')) {
			while (v[vlen-1] != '#') {
				vlen--;
			}
			vlen--;
			while (isspace(v[vlen-1])) {
				vlen--;
			}
		}
		if (vlen == 0) {
			goto nextline;
		}
		_item = map_add_n(rcv->env, k, klen, v, vlen);
		item = &rcv->env->items[_item.i];
		item->v.vmalloc = 0;
		assert(item);
		assert(item->v.s);
		assert(item->v.len);

		if ((d = strchr(v, '$')) && d < v+vlen) {
			item->v.len = rcv_sh_substitute(rcv, item->v.s, item->v.len, &item->v.s);
			item->v.vmalloc = 1;
		}
		if (rcv->xhp.flags & XBPS_FLAG_DEBUG) {
			printf("%s: %.*s %.*s\n", rcv->fname,
			    (int)item->k.len-1, item->k.s,
			    (int)item->v.len, item->v.s);
		}
		if (strncmp("pkgname", k, klen) == 0) {
			rcv->have_vars |= GOT_PKGNAME_VAR;
			vars++;
		} else if (strncmp("version",  k, klen) == 0) {
			rcv->have_vars |= GOT_VERSION_VAR;
			vars++;
		} else if (strncmp("revision", k, klen) == 0) {
			rcv->have_vars |= GOT_REVISION_VAR;
			vars++;
		}
		if (vars > 2)
			return;

nextline:
		ptr = strchr(ptr, '\n') + 1;
	}
}

static int
rcv_process_file(rcv_t *rcv, const char *fname, rcv_check_func check)
{
	rcv->env = map_create();
	if (rcv->env == NULL) {
		rcv->env = NULL;
		return EXIT_FAILURE;
	}
	if (!rcv_load_file(rcv, fname)) {
		map_destroy(rcv->env);
		rcv->env = NULL;
		return EXIT_FAILURE;
	}
	rcv_get_pkgver(rcv);
	check(rcv);
	map_destroy(rcv->env);
	rcv->env = NULL;

	return 0;
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

static bool
check_reverts(const char *repover, const map_item_t reverts)
{
	bool rv = false;
	char *sreverts, *p;

	if (reverts.v.len == 0 || strlen(repover) < 1)
		return rv;

	sreverts = calloc(reverts.v.len+1, sizeof(char));
	assert(sreverts);
	strncpy(sreverts, reverts.v.s, reverts.v.len);
	sreverts[reverts.v.len] = '\0';

	for (p = sreverts; (p = strstr(p, repover));) {
		/*
		 * Check if it's the first character or the previous character is a
		 * whitespace.
		 */
		if (p > sreverts && !isalpha(p[-1]) && !isspace(p[-1])) {
			p++; // always advance
			continue;
		}
		p += strlen(repover);
		/*
		 * Check if it's the last character or if the next character is a
		 * whitespace
		 */
		if (isspace(*p) || *p == '\0') {
			rv = true;
			break;
		}
	}

	free(sreverts);
	return rv;
}

static void
rcv_printf(rcv_t *rcv, FILE *fp, const char *pkgname, const char *repover,
    const char *srcver)
{
	const char *f, *p;

	for (f = rcv->format; *f; f++) {
		if (*f == '\\') {
			f++;
			switch (*f) {
			case '\n': fputc('\n', fp); break;
			case '\t': fputc('\t', fp); break;
			case '\0': fputc('\0', fp); break;
			default:
				fputc('\\', fp);
				fputc(*f, fp);
				break;
			}
		}
		if (*f != '%') {
			fputc(*f, fp);
			continue;
		}
		switch (*++f) {
		case '%': fputc(*f, fp); break;
		case 'n': fputs(pkgname, fp); break;
		case 'r': fputs(repover, fp); break;
		case 's': fputs(srcver, fp); break;
		case 't':
			p = strchr(rcv->fname, '/');
			fwrite(rcv->fname, p ? (size_t)(p - rcv->fname) : strlen(rcv->fname), 1, fp);
			break;
		}
	}
	fputc('\n', fp);
}

static int
rcv_check_version(rcv_t *rcv)
{
	map_item_t pkgname, version, revision, reverts;
	const char *repover = NULL;
	char srcver[BUFSIZ] = { '\0' };
	char pkgn[128];
	int sz;

	assert(rcv);

	if ((rcv->have_vars & GOT_PKGNAME_VAR) == 0) {
		fprintf(stderr, "ERROR: '%s': missing pkgname variable!\n", rcv->fname);
		exit(EXIT_FAILURE);
	}
	if ((rcv->have_vars & GOT_VERSION_VAR) == 0) {
		fprintf(stderr, "ERROR: '%s': missing version variable!\n", rcv->fname);
		exit(EXIT_FAILURE);
	}
	if ((rcv->have_vars & GOT_REVISION_VAR) == 0) {
		fprintf(stderr, "ERROR: '%s': missing revision variable!\n", rcv->fname);
		exit(EXIT_FAILURE);
	}

	pkgname = map_find(rcv->env, "pkgname");
	version = map_find(rcv->env, "version");
	revision = map_find(rcv->env, "revision");
	reverts = map_find(rcv->env, "reverts");

	assert(pkgname.v.s);
	assert(version.v.s);
	assert(revision.v.s);

	sz = snprintf(pkgn, sizeof pkgn, "%.*s",
	    (int)pkgname.v.len, pkgname.v.s);
	if (sz < 0 || (size_t)sz >= sizeof pkgn)
		exit(EXIT_FAILURE);

	sz = snprintf(srcver, sizeof srcver, "%.*s-%.*s_%.*s",
	    (int)pkgname.v.len, pkgname.v.s,
	    (int)version.v.len, version.v.s,
	    (int)revision.v.len, revision.v.s);
	if (sz < 0 || (size_t)sz >= sizeof srcver)
		exit(EXIT_FAILURE);

	if (rcv->installed)
		rcv->pkgd = xbps_pkgdb_get_pkg(&rcv->xhp, pkgn);
	else
		rcv->pkgd = xbps_rpool_get_pkg(&rcv->xhp, pkgn);

	xbps_dictionary_get_cstring_nocopy(rcv->pkgd, "pkgver", &repover);

	if (!repover && rcv->manual)
		;
	else if (rcv->show_all)
		;
	else if (repover && (xbps_cmpver(repover+pkgname.v.len+1,
		    srcver+pkgname.v.len+1) < 0 ||
		    check_reverts(repover+pkgname.v.len+1, reverts)))
		;
	else
		return 0;

	repover = repover ? repover+pkgname.v.len+1 : "?";
	rcv_printf(rcv, stdout, pkgn, repover, srcver+pkgname.v.len+1);

	return 0;
}

static int
rcv_process_dir(rcv_t *rcv, const char *path, rcv_proc_func process)
{
	DIR *dir = NULL;
	struct dirent entry, *result;
	struct stat st;
	char filename[BUFSIZ];
	int i, ret = 0, errors = 0;

	dir = opendir(path);
error:
	if (errors > 0 || !dir) {
		fprintf(stderr, "Error: while processing dir '%s': %s\n", path,
			strerror(errno));
		exit(1);
	}

	if ((chdir(path)) == -1) {
		errors = errno;
		goto error;
	}
	for (;;) {
		i = readdir_r(dir, &entry, &result);
		if (i > 0) {
			errors = errno;
			goto error;
		}
		if (result == NULL)
			break;
		if ((strcmp(result->d_name, ".") == 0) ||
		    (strcmp(result->d_name, "..") == 0))
			continue;
		if ((lstat(result->d_name, &st)) != 0) {
			errors = errno;
			goto error;
		}
		if (S_ISLNK(st.st_mode) != 0)
			continue;

		snprintf(filename, sizeof(filename), "%s/template", result->d_name);
		ret = process(rcv, filename, rcv_check_version);
	}

	if ((closedir(dir)) == -1) {
		errors = errno;
		dir = NULL;
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
	const char *prog = argv[0], *sopts = "hC:D:df:iImR:r:sV";
	const struct option lopts[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "config", required_argument, NULL, 'C' },
		{ "distdir", required_argument, NULL, 'D' },
		{ "debug", no_argument, NULL, 'd' },
		{ "format", no_argument, NULL, 'f' },
		{ "installed", no_argument, NULL, 'I' },
		{ "ignore-conf-repos", no_argument, NULL, 'i' },
		{ "manual", no_argument, NULL, 'm' },
		{ "repository", required_argument, NULL, 'R' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "show-all", no_argument, NULL, 's' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	memset(&rcv, 0, sizeof(rcv_t));
	rcv.manual = false;
	rcv.format = "%n %r %s";

	while ((c = getopt_long(argc, argv, sopts, lopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			return show_usage(prog);
		case 'C':
			rcv.xbps_conf = strdup(optarg);
			break;
		case 'D':
			rcv_set_distdir(&rcv, optarg);
			break;
		case 'd':
			rcv.xhp.flags |= XBPS_FLAG_DEBUG;
			break;
		case 'f':
			rcv.format = optarg;
			break;
		case 'i':
			rcv.xhp.flags |= XBPS_FLAG_IGNORE_CONF_REPOS;
			break;
		case 'I':
			rcv.installed = true;
			break;
		case 'm':
			rcv.manual = true;
			break;
		case 'R':
			xbps_repo_store(&rcv.xhp, optarg);
			break;
		case 'r':
			rcv.rootdir = strdup(optarg);
			break;
		case 's':
			rcv.show_all = true;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		default:
			return show_usage(prog);
		}
	}
	/*
	 * If --distdir not set default to ~/void-packages.
	 */
	if (rcv.distdir == NULL) {
		distdir = xbps_xasprintf("%s/void-packages", getenv("HOME"));
		rcv_set_distdir(&rcv, distdir);
		free(distdir);
	}
	argc -= optind;
	argv += optind;

	rcv_init(&rcv, prog);

	if (!rcv.manual) {
		rcv_process_dir(&rcv, rcv.pkgdir, rcv_process_file);
	} else if ((chdir(rcv.pkgdir)) == -1) {
		fprintf(stderr, "Error: while processing dir '%s': %s\n", rcv.pkgdir,
			strerror(errno));
		exit(1);
	}
	rcv.manual = true;
	for (i = 0; i < argc; i++) {
		char tmp[PATH_MAX], *tmpl, *p;
		if (strncmp(argv[i], "srcpkgs/", sizeof ("srcpkgs/")-1) == 0) {
			argv[i] += sizeof ("srcpkgs/")-1;
		}
		if ((p = strrchr(argv[i], '/')) && (strcmp(p, "/template")) == 0) {
			tmpl = argv[i];
		} else {
			xbps_strlcat(tmp, argv[i], sizeof tmp);
			xbps_strlcat(tmp, "/template", sizeof tmp);
			tmpl = tmp;
		}
		rcv_process_file(&rcv, tmpl, rcv_check_version);
	}
	rcv_end(&rcv);
	exit(EXIT_SUCCESS);
}
