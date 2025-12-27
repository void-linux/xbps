#include <dirent.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INI_HANDLER_LINENO 1
#define INI_CALL_HANDLER_ON_NEW_SECTION 1
#include "external/inih/ini.h"

#include "xbps.h"
#include "xbps_api_impl.h"

enum match_pkg_action {
	MATCH_PKG_ALL = 0,
	MATCH_PKG_INSTALL,
	MATCH_PKG_UPDATE,
	MATCH_PKG_REMOVE,
	MATCH_PKG_REINSTALL,
	MATCH_PKG_CONFIGURE,
};

struct match_pkg {
	enum match_pkg_action action;
	enum {
		MATCH_PKG_NAME,
		MATCH_PKG_CONSTRAINT,
		MATCH_PKG_PATTERN,
	} kind;
	char *name;
	char *pattern;
};

enum match_path_action {
	MATCH_PATH_CHANGED = 0,
	MATCH_PATH_CREATED,
	MATCH_PATH_MODIFIED,
	MATCH_PATH_DELETED,
};

struct match_path {
	enum match_path_action action;
	enum {
		PATH_STR,
		PATH_PATTERN,
	} match;
	char *pattern;
};

struct match {
	size_t npackages;
	struct match_pkg *packages;
	size_t npaths;
	struct match_path *paths;
};

static void
match_free(struct match *m)
{
	if (!m)
		return;
	for (size_t i = 0; i < m->npackages; i++) {
		free(m->packages[i].name);
		free(m->packages[i].pattern);
	}
	for (size_t i = 0; i < m->npaths; i++) {
		free(m->paths[i].pattern);
	}
	free(m->paths);
	free(m);
}

enum when {
	HOOK_PRE_TRANSACTION  = 1 << 0,
	HOOK_POST_TRANSACTION = 1 << 1,
};

struct hook {
	char *filename;
	char *name;

	enum when when;

	int argc;
	char **argv;

	size_t nmatches;
	struct match **matches;
};

static void
hook_free(struct hook *hook)
{
	if (!hook)
		return;

	free(hook->filename);
	free(hook->name);

	if (hook->argv) {
		for (char **pp = hook->argv; *pp; pp++)
			free(*pp);
	}
	free(hook->argv);

	for (size_t i = 0; i < hook->nmatches; i++)
		match_free(hook->matches[i]);
	free(hook->matches);

	free(hook);
}

struct parse_ctx {
	const char *path;
	struct hook *hook;
	enum {
		SECTION_HOOK = 1,
		SECTION_MATCH,
	} section;
	struct match *match;
};

static void __attribute__((format(printf, 3, 4)))
syntax_error(struct parse_ctx *ctx, int lineno, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "ERROR: syntax error: %s:%d: ", ctx->path, lineno);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
}

static size_t
word_count(const char *s)
{
	size_t n = 0;

	for (const char *p = s; *p;) {
		for (; *p == ' ' || *p == '\t'; p++)
			;
		for (; *p; p++) {
			switch (*p) {
			case '\t':
			case ' ':
				break;
			case '\\':
				if (p[1] == ' ' || p[1] == '\t') {
					p += 1;
					continue;
				}
				// fallthrough
			default:
				continue;
			}
			break;
		}
		n += 1;
	}

	return n;
}

static int
word_iter(char *dst, size_t dstsz, const char **pp)
{
	const char *p = *pp;
	size_t n = 0;

	for (; *p == ' ' || *p == '\t'; p++)
		;
	if (*p == '\0')
		return 0;

	for (; *p; p++) {
		switch (*p) {
		case '\t':
		case ' ':
			break;
		case '\\':
			if (p[1] == ' ' || p[1] == '\t') {
				p += 1;
				if (n + 1 >= dstsz)
					return -ENOBUFS;
				dst[n++] = *p;
				continue;
			}
			// fallthrough
		default:
			if (n + 1 >= dstsz)
				return -ENOBUFS;
			dst[n++] = *p;
			continue;
		}
		break;
	}

	dst[n] = '\0';

	*pp = p;
	return 1;
}

static bool UNUSED
word_empty(const char *s)
{
	return s[strspn(s, " \t")] == '\0';
}

static int
parse_exec(struct parse_ctx *ctx, int lineno, const char *value)
{
	char buf[4096];
	char **argv = 0;
	int r;
	int n = 0;
	int argc = 0;

	if (ctx->hook->argv) {
		syntax_error(ctx, lineno, "Hook: Exec: defined multiple times");
		return -EINVAL;
	}

	n = word_count(value);
	if (n == 0) {
		syntax_error(ctx, lineno, "Hook: Exec: missing command");
		return -EINVAL;
	}

	argv = calloc(n + 1, sizeof(*argv));
	if (!argv) {
		r = xbps_error_oom();
		goto err;
	}

	for (const char *iter = value;;) {
		r = word_iter(buf, sizeof(buf), &iter);
		if (r < 0) {
			syntax_error(ctx, lineno, "Hook: Exec: %s", strerror(-r));
			r = -EINVAL;
			goto err;
		}
		if (r == 0)
			break;

		argv[argc] = strdup(buf);
		if (!argv[argc]) {
			r = xbps_error_oom();
			goto err;
		}
		xbps_dbg_printf("argv[%d]='%s'\n", argc, argv[argc]);
		argc++;
	}
	xbps_dbg_printf("argc=%d\n", argc);

	ctx->hook->argc = argc;
	ctx->hook->argv = argv;

	return 0;
err:
	for (int i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
	return r;
}

static int
parse_when(struct parse_ctx *ctx, int lineno, const char *value)
{
	for (const char *p = value; *p;) {
		const char *e = strchrnul(p, ' ');

		if (strncmp("PreTransaction", p, e - p) == 0) {
			ctx->hook->when |= HOOK_PRE_TRANSACTION;
		} else if (strncmp("PostTransaction", p, e - p) == 0) {
			ctx->hook->when |= HOOK_POST_TRANSACTION;
		} else {
			syntax_error(ctx, lineno, "Hook: When: unknown value: %.*s",
			    (int)(e - p), p);
			return -EINVAL;
		}

		for (; *e == ' '; e++)
			;
		p = e;
	}
	return 0;
}

static struct match_pkg *
match_alloc_pkg(struct match *m)
{
	size_t nmemb = m->npackages + 1;
	struct match_pkg *tmp = reallocarray(m->packages, nmemb, sizeof(*tmp));
	if (!tmp)
		return NULL;
	m->packages = tmp;
	return &m->packages[m->npackages++];
}

static int
match_parse_package(struct parse_ctx *ctx, int lineno UNUSED, const char *value,
    enum match_pkg_action action)
{
	struct match_pkg *m;
	const char *d;

	xbps_dbg_printf("[hooks] %s: match package: %s\n", ctx->path, value);

	m = match_alloc_pkg(ctx->match);
	if (!m)
		return xbps_error_oom();

	m->action = action;

	d = strpbrk(value, "><*?[]");
	if (!d) {
		m->kind = MATCH_PKG_NAME;
		m->name = strdup(value);
		if (!m->name)
			return xbps_error_oom();
		return 0;
	}
	switch (*d) {
	case '>':
	case '<':
		m->kind = MATCH_PKG_CONSTRAINT;
		m->name = strndup(value, d - value);
		if (!m->name)
			return xbps_error_oom();
		m->pattern = strdup(value);
		if (!m->pattern)
			return xbps_error_oom();
		return 0;
	default:
		m->kind = MATCH_PKG_PATTERN;
		m->pattern = strdup(value);
		if (!m->pattern)
			return xbps_error_oom();
		return 0;
	}
}

static struct match_path *
match_alloc_path(struct match *m)
{
	size_t nmemb = m->npaths + 1;
	struct match_path *tmp = reallocarray(m->paths, nmemb, sizeof(*tmp));
	if (!tmp)
		return NULL;
	m->paths = tmp;
	return &m->paths[m->npaths++];
}

static int
match_parse_path(struct parse_ctx *ctx, int lineno UNUSED, const char *value,
    enum match_path_action action)
{
	struct match_path *m;

	xbps_dbg_printf("[hooks] %s: match path: %s\n", ctx->path, value);

	m = match_alloc_path(ctx->match);
	if (!m)
		return xbps_error_oom();
	m->action = action;
	m->pattern = strdup(value);
	if (!m->pattern)
		return xbps_error_oom();

	return 0;
}

#define STRLEN(a) (sizeof((a)) - sizeof((a)[0]))
#define HASPREFIX(a, b) (strncmp(, STRLEN((a))) == 0)

static int
match_section_handler(struct parse_ctx *ctx, int lineno, const char *name, const char *value)
{
	if (strcmp(name, "PackageInstall") == 0) {
		int r = match_parse_package(ctx, lineno, value, MATCH_PKG_INSTALL);
		return r == 0;
	} else if (strcmp(name, "PackageUpdate") == 0) {
		int r = match_parse_package(ctx, lineno, value, MATCH_PKG_UPDATE);
		return r == 0;
	} else if (strcmp(name, "PackageRemove") == 0) {
		int r = match_parse_package(ctx, lineno, value, MATCH_PKG_REMOVE);
		return r == 0;
	} else if (strcmp(name, "PackageReinstall") == 0) {
		int r = match_parse_package(ctx, lineno, value, MATCH_PKG_REINSTALL);
		return r == 0;
	} else if (strcmp(name, "PackageConfigure") == 0) {
		int r = match_parse_package(ctx, lineno, value, MATCH_PKG_CONFIGURE);
		return r == 0;
	} else if (strcmp(name, "PathCreated") == 0) {
		int r = match_parse_path(ctx, lineno, value, MATCH_PATH_CREATED);
		return r == 0;
	} else if (strcmp(name, "PathChanged") == 0) {
		int r = match_parse_path(ctx, lineno, value, MATCH_PATH_CHANGED);
		return r == 0;
	} else if (strcmp(name, "PathModified") == 0) {
		int r = match_parse_path(ctx, lineno, value, MATCH_PATH_MODIFIED);
		return r == 0;
	} else if (strcmp(name, "PathDeleted") == 0) {
		int r = match_parse_path(ctx, lineno, value, MATCH_PATH_DELETED);
		return r == 0;
	}
	syntax_error(ctx, lineno,
	    "section: Match: unknown key: %s", name);
	return 0;
}

static int
hook_handler(
    struct parse_ctx *ctx, int lineno, const char *name, const char *value)
{
	if (strcmp(name, "Name") == 0) {
		ctx->hook->name = strdup(value);
		if (!ctx->hook->name)
			return 0;
		return 1;
	} else if (strcmp(name, "Exec") == 0) {
		int r = parse_exec(ctx, lineno, value);
		if (r < 0)
			return 0;
		return 1;
	} else if (strcmp(name, "When") == 0) {
		int r = parse_when(ctx, lineno, value);
		if (r < 0)
			return 0;
		return 1;
	}
	syntax_error(ctx, lineno,
	    "section: Hook: unknown key: %s", name);
	return 0;
}

static int
add_match(struct hook *hook, struct match *item)
{
	size_t nmemb = hook->nmatches + 1;
	struct match **tmp = reallocarray(hook->matches, nmemb, sizeof(*tmp));
	if (!tmp)
		return xbps_error_oom();
	hook->matches = tmp;
	hook->matches[hook->nmatches++] = item;
	return 0;
}

static int
hook_ini_handler(void *user, const char *section, const char *name,
    const char *value, int lineno)
{
	struct parse_ctx *ctx = user;
	int r;

	// xbps_dbg_printf("[hooks] %s %s %s %s\n",ctx->path, section, name, value);

	// new section
	if (!name) {
		if (strcmp(section, "Hook") == 0) {
			ctx->section = SECTION_HOOK;
			return 1;
		} else if (strcmp(section, "Match") == 0) {
			ctx->section = SECTION_MATCH;
			ctx->match = calloc(1, sizeof(*ctx->match));
			if (!ctx->match) {
				xbps_error_oom();
				return 0;
			}
			r = add_match(ctx->hook, ctx->match);
			if (r < 0)
				return 0;
			return 1;
		}
		syntax_error(ctx, lineno, "unknown section: %s", section);
		return 0;
	}

	if (section[0] == '\0') {
		syntax_error(ctx, lineno,
		    "variable defined outside of section: %s", name);
		return 0;
	}

	switch (ctx->section) {
	case SECTION_HOOK:
		return hook_handler(ctx, lineno, name, value);
		break;
	case SECTION_MATCH:
		return match_section_handler(ctx, lineno, name, value);
		break;
	} 
	return 1;
}

static struct hook *
hook_parse(const char *dir, const char *filename)
{
	char path[PATH_MAX];
	struct parse_ctx ctx;
	struct hook *hook;
	FILE *fp;
	int r;

	if (xbps_path_join(path, sizeof(path), dir, filename, (char *)NULL) == -1) {
		xbps_error_printf("failed to open hook: %s/%s: %s\n", dir,
		    filename, strerror(ENAMETOOLONG));
		return NULL;
	}

	hook = calloc(1, sizeof(*hook));
	if (!hook) {
		xbps_error_oom();
		return NULL;
	}

	hook->filename = strdup(filename);
	if (!hook->filename) {
		xbps_error_oom();
		free(hook);
		return NULL;
	}

	fp = fopen(path, "rb");
	if (!fp) {
		r = -errno;
		xbps_error_printf(
		    "failed to open hook file: %s: %s\n", path, strerror(-r));
		hook_free(hook);
		return NULL;
	}

	ctx.path = path;
	ctx.hook = hook;
	r = ini_parse_file(fp, hook_ini_handler, &ctx);
	if (r < 0) {
		if (r == -2)
			r = -ENOMEM;
		else
			r= -EIO;
		xbps_error_printf(
		    "failed to parse hook: %s: %s\n", path, strerror(-r));
		goto err;
	}
	if (r > 0) {
		xbps_error_printf(
		    "failed to parse hook: %s:%d: syntax error\n", path, r);
		goto err;
	}

	fclose(fp);
	return hook;
err:
	fclose(fp);
	hook_free(hook);
	return NULL;
}

struct xbps_hooks {
	size_t nhooks;
	struct hook **hooks;
};

static int
hooks_add_hook(struct xbps_hooks *hooks, struct hook *item)
{
	size_t nmemb = hooks->nhooks + 1;
	struct hook **tmp = reallocarray(hooks->hooks, nmemb, sizeof(*tmp));
	if (!tmp)
		return xbps_error_oom();
	hooks->hooks = tmp;
	hooks->hooks[hooks->nhooks++] = item;
	return 0;
}

static bool
seen_hook_filename(struct xbps_hooks *hooks, const char *filename)
{
	for (size_t i = 0; i < hooks->nhooks; i++) {
		if (strcmp(hooks->hooks[i]->filename, filename) == 0)
			return true;
	}
	return false;
}

static int
hooks_scan_dir(struct xbps_hooks *hooks, const char *dir)
{
	struct dirent **namelist;
	int n;
	int r;

	xbps_dbg_printf("[hooks] scanning directory: %s\n", dir);

	n = scandir(dir, &namelist, NULL, alphasort);
	if (n == -1) {
		if (errno != ENOENT)
			return -errno;
		return 0;
	}

	for (int i = 0; i < n; i++) {
		struct hook *hook;
		if (namelist[i]->d_name[0] == '.')
			continue;
		if (seen_hook_filename(hooks, namelist[i]->d_name)) {
			xbps_dbg_printf(
			    "[hooks] skipping hook: %s/%s: filename masked\n",
			    dir, namelist[i]->d_name);
			continue;
		}
		xbps_dbg_printf(
		    "[hooks] parsing hook: %s/%s\n",
		    dir, namelist[i]->d_name);
		hook = hook_parse(dir, namelist[i]->d_name);
		if (!hook) {
			r = -errno;
			goto err;
		}
		r = hooks_add_hook(hooks, hook);
		if (r < 0)
			goto err;
	}

	r = 0;
err:
	for (int i = 0; i < n; i++)
		free(namelist[i]);
	free(namelist);
	return r;
}

struct xbps_hooks *
xbps_hooks_init(struct xbps_handle *xhp)
{
	char dir[PATH_MAX];
	struct xbps_hooks *hooks = NULL;
	int r;

	hooks = calloc(1, sizeof(*hooks));
	if (!hooks) {
		xbps_error_oom();
		return NULL;
	}

	if (xbps_path_join(dir, sizeof(dir), xhp->confdir, "hooks", (char *)NULL) == -1) {
		xbps_error_printf("%s: %s\n", xhp->confdir, strerror(ENAMETOOLONG));
		r = -ENAMETOOLONG;
		goto err;
	}
	r = hooks_scan_dir(hooks, dir);
	if (r < 0)
		goto err;

	if (xbps_path_join(dir, sizeof(dir), xhp->sysconfdir, "hooks", (char *)NULL) == -1) {
		xbps_error_printf("%s: %s\n", xhp->confdir, strerror(ENAMETOOLONG));
		r = -ENAMETOOLONG;
		goto err;
	}
	r = hooks_scan_dir(hooks, dir);
	if (r < 0)
		goto err;

	return hooks;
err:
	xbps_hooks_free(hooks);
	errno = -r;
	return NULL;
}

void
xbps_hooks_free(struct xbps_hooks *hooks)
{
	if (!hooks)
		return;
	free(hooks);
}

static bool
match_package(const struct hook *hook, const char *pkgver UNUSED,
    const char *pkgname, enum match_pkg_action action)
{
	if (hook->matches == 0)
		return false;
	for (size_t i = 0; i < hook->nmatches; i++) {
		struct match *m = hook->matches[i];
		for (size_t j = 0; j < m->npackages; j++) {
			struct match_pkg *p = &m->packages[j];
			if (p->action != action)
				continue;
			switch (p->kind) {
			case MATCH_PKG_NAME:
				return strcmp(m->packages[i].name, pkgname) == 0;
			case MATCH_PKG_CONSTRAINT:
				xbps_error_printf("match constraint not implemented\n");
				return false;
			case MATCH_PKG_PATTERN:
				xbps_error_printf("match pattern not implemented\n");
				return false;
			}
		}
	}
	return false;
}

static int
match_package_hooks(struct xbps_handle *xhp, struct xbps_hooks *hooks,
    bool *matches, enum when when)
{
	xbps_object_t pkgd;
	xbps_object_iterator_t iter;
	int r;

	iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	if (!iter)
		return xbps_error_oom();

	while ((pkgd = xbps_object_iterator_next(iter))) {
		char pkgname[XBPS_NAME_SIZE] = {0};
		const char *pkgver = NULL;
		xbps_trans_type_t ttype;
		enum match_pkg_action action;

		if(!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
			abort();

		if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver))
			abort();

		ttype = xbps_transaction_pkg_type(pkgd);
		switch (ttype) {
		case XBPS_TRANS_INSTALL:
			action = MATCH_PKG_INSTALL;
			break;
		case XBPS_TRANS_REINSTALL:
			action = MATCH_PKG_REINSTALL;
			break;
		case XBPS_TRANS_UPDATE:
			action = MATCH_PKG_UPDATE;
			break;
		case XBPS_TRANS_CONFIGURE:
			action = MATCH_PKG_CONFIGURE;
			break;
		case XBPS_TRANS_REMOVE:
			action = MATCH_PKG_REMOVE;
			break;
		case XBPS_TRANS_UNKNOWN:
		case XBPS_TRANS_HOLD:
		case XBPS_TRANS_DOWNLOAD:
			continue;
		}

		for (size_t i = 0; i < hooks->nhooks; i++) {
			const struct hook *h = hooks->hooks[i];

			// hook already matched
			if (matches[i])
				continue;

			if ((h->when & when) == 0)
				continue;

			if (!match_package(h, pkgver, pkgname, action))
				continue;

			matches[i] = true;
		}
	}

	r = 0;
// err:
	xbps_object_iterator_release(iter);
	return r;
}

static int
hook_run(const struct xbps_handle *xhp, const struct hook *hook, enum when when UNUSED)
{
	int r;

	xbps_dbg_printf("[hooks] running hook: %s\n", hook->filename);

	r = xbps_file_exec_argv(xhp, __UNCONST(hook->argv));
	if (r == -1)
		return -errno;
	xbps_dbg_printf("%d\n", r);

	return 0;
}

static int
run_hooks(struct xbps_handle *xhp, struct xbps_hooks *hooks, enum when when)
{
	int r;
	bool *matches;

	if (hooks->nhooks == 0)
		return 0;

	// XXX: get a bitset?
	matches = calloc(hooks->nhooks, sizeof(*matches));
	if (!matches)
		return xbps_error_oom();

	r = match_package_hooks(xhp, hooks, matches, when);
	if (r < 0)
		goto err;

	for (size_t i = 0; i < hooks->nhooks; i++) {
		const struct hook *h = hooks->hooks[i];
		if (!matches[i])
			continue;

		r = hook_run(xhp, h, when);
		if (r < 0)
			goto err;
	}

	free(matches);
	return 0;
err:
	free(matches);
	return r;
}

int
xbps_hooks_pre_transaction(struct xbps_handle *xhp, struct xbps_hooks *hooks)
{
	int r;

	xbps_dbg_printf("[hooks] running pre-transaction hooks\n");

	r = run_hooks(xhp, hooks, HOOK_PRE_TRANSACTION);
	if (r < 0)
		return r;

	return 0;
}

int
xbps_hooks_post_transaction(struct xbps_handle *xhp, struct xbps_hooks *hooks)
{
	int r;

	xbps_dbg_printf("[hooks] running post-transaction hooks\n");

	r = run_hooks(xhp, hooks, HOOK_POST_TRANSACTION);
	if (r < 0)
		return r;

	return 0;
}
