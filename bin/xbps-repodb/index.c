#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <xbps.h>
#include "defs.h"
#include "uthash.h"

#include <picosat.h>

#define VARIABLE_TYPE_REAL_PACKAGE 0
#define VARIABLE_TYPE_VIRTUAL_PACKAGE 1
#define VARIABLE_TYPE_SOLIB 2
#define VARIABLE_NUMBER_STEP 4

enum source {
	SOURCE_PUBLIC = 0,
	SOURCE_STAGE,
	SOURCE_NONE,
};

enum clause_type {
	// length has to be 1
	CLAUSE_TYPE_CERTAINTY,
	// first literal implies disjunction of rest
	CLAUSE_TYPE_IMPLICATION,
	// first literal is equivalent to disjunction of rest
	CLAUSE_TYPE_EQUIVALENCE,
};

struct variable_t {
	char *str;
	int number;
	UT_hash_handle hh_by_name;
	UT_hash_handle hh_by_number;
};

struct hash_str_holder_t {
	char *str;
	UT_hash_handle hh;
};

struct clause;

struct clause {
	struct clause *next;
	char *label;
	enum clause_type type;
	int backing_clauses;
	int literals[];
};

struct package_t {
	const char *pkgver;
	xbps_dictionary_t dict;
	int repo;
};

struct node_t;

struct node_t {
	char *pkgname;
	struct package_t packages[2];
	enum source source;
	/**
	 * NULL means package should be updated on its own pace.
	 * Pointer to self means base node is not present and this node should be removed from repo.
	 * Other value means that only package matching indexed package from other node should be indexed.
	 */
	struct node_t *base_node;
	UT_hash_handle hh;
};

struct repo_t {
	xbps_dictionary_t idx;
	xbps_dictionary_t meta;
	struct xbps_repo *repo;
	char *lock_name;
	int lock_fd;
};

struct repos_group_t {
	struct node_t *nodes;
	/**
	 * key is solib name, value is array of pkgvers providing it
	 */
	xbps_dictionary_t shlib_providers;
	/**
	 * key is virtual pkgname, value is dictionary,
	 * where key is pkgname of real package, value is pkgver of virtual it provides
	 */
	xbps_dictionary_t virtual_providers;
	int repos_count;
	/** array of pairs of repo_t */
	struct repo_t (*repos)[2];
	struct clause *clauses;
	struct clause *clauses_last;
	struct xbps_handle *xhp;
	bool explaining_pass;
	bool pushed_out_packages;
};

static struct hash_str_holder_t *owned_strings_container = NULL;

static struct variable_t *variables_by_name = NULL;
static struct variable_t *variables_by_number = NULL;

// zero means end of clause, cannot be used as variable
static int variable_next_number = VARIABLE_NUMBER_STEP;

static char *
owned_string(const char *content) {
	struct hash_str_holder_t *holder = NULL;
	size_t len = strlen(content);

	HASH_FIND(hh, owned_strings_container, content, len, holder);
	if (!holder) {
		holder = calloc(1, sizeof *holder);
		holder->str = strdup(content);
		HASH_ADD_KEYPTR(hh, owned_strings_container, holder->str, len, holder);
	}
	return holder->str;
}

static void
free_owned_strings(void) {
	struct hash_str_holder_t *holder = NULL;
	struct hash_str_holder_t *tmp = NULL;

	HASH_ITER(hh, owned_strings_container, holder, tmp) {
		HASH_DEL(owned_strings_container, holder);
		free(holder->str);
		free(holder);
	}
}

static int
variable_by_name(const char *pkgver) {
	struct variable_t *holder = NULL;
	unsigned int len = strlen(pkgver);
	char *owned = owned_string(pkgver);

	HASH_FIND(hh_by_name, variables_by_name, owned, len, holder);
	if (!holder) {
		holder = calloc(1, sizeof *holder);
		holder->str = owned;
		holder->number = variable_next_number;
		variable_next_number += VARIABLE_NUMBER_STEP;
		HASH_ADD_KEYPTR(hh_by_name, variables_by_name, owned, len, holder);
		HASH_ADD(hh_by_number, variables_by_number, number, sizeof holder->number, holder);
	}
	return holder->number;
}

static int
variable_real_package(const char *pkgver) {
	return variable_by_name(pkgver) + VARIABLE_TYPE_REAL_PACKAGE;
}

static int
variable_virtual_from_real(int number) {
	return number - VARIABLE_TYPE_REAL_PACKAGE + VARIABLE_TYPE_VIRTUAL_PACKAGE;
}

static int
variable_virtual_package(const char *pkgver) {
	return variable_virtual_from_real(variable_real_package(pkgver));
}

static int
variable_shlib(const char *shlib) {
	return variable_by_name(shlib) + VARIABLE_TYPE_SOLIB;
}

static const char*
variable_name(int number) {
	struct variable_t *holder = NULL;

	number = abs(number);
	number -= number % VARIABLE_NUMBER_STEP;
	HASH_FIND(hh_by_number, variables_by_number, &number, sizeof(number), holder);
	return (holder ? holder->str : NULL);
}

static char*
variable_text(int variable) {
	// name + negation + virtual + terminator
	static char buffer[XBPS_NAME_SIZE + 3 + 6 + 1];
	bool virtual;

	buffer[0] = '\0';
	if (variable < 0) {
		strcat(buffer, "¬ ");
		variable = -variable;
	}
	virtual = (variable % VARIABLE_NUMBER_STEP == VARIABLE_TYPE_VIRTUAL_PACKAGE);
	if (virtual) {
		strcat(buffer, "virt(");
	}
	strncat(buffer, variable_name(variable), XBPS_NAME_SIZE);
	if (virtual) {
		strcat(buffer, ")");
	}
	return buffer;
}

static void
free_variables(void) {
	struct variable_t *holder = NULL;
	struct variable_t *tmp = NULL;

	HASH_ITER(hh_by_name, variables_by_name, holder, tmp) {
		HASH_DELETE(hh_by_name, variables_by_name, holder);
	}
	HASH_ITER(hh_by_number, variables_by_number, holder, tmp) {
		HASH_DELETE(hh_by_number, variables_by_number, holder);
		free(holder);
	}
}

static struct clause*
clause_alloc(enum clause_type type, int capacity) {
	struct clause *clause = malloc(sizeof *clause + (capacity + 1) * sizeof *clause->literals);
	clause->next = NULL;
	clause->label = NULL;
	clause->type = type;
	return clause;
}

static void
clause_print(struct clause *clause, FILE *f) {
	switch (clause->type) {
		case CLAUSE_TYPE_CERTAINTY:
			if (clause->literals[0] > 0) {
				fprintf(f, "⊤ → %s", variable_text(clause->literals[0]));
			} else {
				fprintf(f, "%s → ⊥", variable_text(-clause->literals[0]));
			}
			break;
		case CLAUSE_TYPE_IMPLICATION:
		case CLAUSE_TYPE_EQUIVALENCE:
			fprintf(f, "%s %s (", variable_text(clause->literals[0]), (clause->type == CLAUSE_TYPE_IMPLICATION ? "→" : "↔"));
			for (int *lit = &clause->literals[1]; *lit; ++lit) {
				fprintf(f, "%s ∨ ", variable_text(*lit));
			}
			fprintf(f, "⊥)");
			break;
	}
	if (clause->label) {
		fprintf(f, " {%s}", clause->label);
	}
	fprintf(f, "\n");
}

static void
clause_add(struct repos_group_t *group, PicoSAT *solver, struct clause *clause, int length) {
	clause->literals[length] = 0;
	if (clause->type == CLAUSE_TYPE_IMPLICATION || clause->type == CLAUSE_TYPE_EQUIVALENCE) {
		// 1. p → (q ∨ r) == ¬p ∨ q ∨ r
		picosat_add(solver, -clause->literals[0]);
		picosat_add_lits(solver, clause->literals + 1);
	} else {
		picosat_add_lits(solver, clause->literals);
	}
	clause->backing_clauses = 1;
	if (clause->type == CLAUSE_TYPE_EQUIVALENCE) {
		// p ↔ (q ∨ r) == (1.) ∧ (2.)
		// 2. (q ∨ r) → p == (q → p) ∧ (r → p) == (¬q ∨ p) ∧ (¬r ∨ p)
		for (int *lit = clause->literals + 1; *lit; ++lit) {
			picosat_add_arg(solver, -*lit, clause->literals[0], 0);
			++clause->backing_clauses;
		}
	}
	if (group->explaining_pass) {
		if (group->clauses) {
			group->clauses_last->next = clause;
			group->clauses_last = clause;
		} else {
			group->clauses = group->clauses_last = clause;
		}
	} else {
		if (group->xhp->flags & XBPS_FLAG_DEBUG) {
			clause_print(clause, stderr);
		}
		free(clause);
	}
}

static void
package_init(struct package_t *package, xbps_dictionary_t pkg, int repo_serial) {
	xbps_object_retain(pkg);
	xbps_dictionary_get_cstring_nocopy(pkg, "pkgver", &package->pkgver);
	package->dict = pkg;
	package->repo = repo_serial;
}

static void
package_release(struct package_t *package) {
	if (!package) {
		return;
	}
	if (package->dict) {
		xbps_object_release(package->dict);
	}
}

static void
repo_group_purge_packages(struct repos_group_t *group) {
	struct node_t *current_node = NULL;
	struct node_t *tmp_node = NULL;

	HASH_ITER(hh, group->nodes,current_node, tmp_node) {
		HASH_DEL(group->nodes, current_node);
		package_release(&current_node->packages[SOURCE_PUBLIC]);
		package_release(&current_node->packages[SOURCE_STAGE]);
		free(current_node);
	}
	while (group->clauses) {
		struct clause *current_clause = group->clauses;
		group->clauses = current_clause->next;
		free(current_clause);
	}
	group->clauses_last = NULL;
	xbps_object_release(group->shlib_providers);
	xbps_object_release(group->virtual_providers);
	group->shlib_providers = xbps_dictionary_create();
	group->virtual_providers = xbps_dictionary_create();
}

static void
repo_group_init(struct repos_group_t *group, struct xbps_handle *xhp, int repos_count) {
	group->nodes = NULL;
	group->shlib_providers = xbps_dictionary_create();
	group->virtual_providers = xbps_dictionary_create();
	group->repos_count = repos_count;
	group->repos = calloc(group->repos_count, sizeof *group->repos);
	group->clauses = NULL;
	group->clauses_last = NULL;
	group->xhp = xhp;
	group->explaining_pass = false;
	group->pushed_out_packages = false;
}

static void
repo_group_release(struct repos_group_t *group) {
	repo_group_purge_packages(group);
	xbps_object_release(group->shlib_providers);
	xbps_object_release(group->virtual_providers);
	for(int i = 0; i < group->repos_count; ++i) {
		if (group->repos[i][SOURCE_PUBLIC].repo) {
			xbps_repo_release(group->repos[i][SOURCE_PUBLIC].repo);
		}
		if (group->repos[i][SOURCE_STAGE].repo) {
			xbps_repo_release(group->repos[i][SOURCE_STAGE].repo);
		}
	}
	free(group->repos);
}

static int
load_repo(struct repos_group_t *group, struct xbps_repo *current_repo, enum source source, int repo_serial) {
	xbps_object_iterator_t iter = NULL;
	xbps_dictionary_keysym_t keysym = NULL;

	xbps_dbg_printf(group->xhp, "loading repo '%s'\n", current_repo->uri);
	iter = xbps_dictionary_iterator(current_repo->idx);
	while ((keysym = xbps_object_iterator_next(iter))) {
		xbps_dictionary_t pkg = xbps_dictionary_get_keysym(current_repo->idx, keysym);
		char *pkgname = owned_string(xbps_dictionary_keysym_cstring_nocopy(keysym));
		struct node_t *new_node = NULL;
		struct node_t *existing_node = NULL;
		struct package_t *existing_package = NULL;

		HASH_FIND(hh, group->nodes, pkgname, strlen(pkgname), existing_node);
		if (existing_node) {
			existing_package = &existing_node->packages[source];
		}
		if (!existing_node) {
			new_node = calloc(1, sizeof *new_node);
			new_node->pkgname = pkgname;
			package_init(&new_node->packages[source], pkg, repo_serial);
			HASH_ADD_KEYPTR(hh, group->nodes, pkgname, strlen(pkgname), new_node);
		} else if (existing_package->pkgver) {
			const char *pkgver = xbps_string_cstring_nocopy(xbps_dictionary_get(pkg, "pkgver"));
			group->pushed_out_packages = true;
			if (xbps_pkg_version_order(existing_package->dict, pkg) >= 0) {
				fprintf(stderr, "'%s' from '%s' is about to push out '%s' from '%s'\n",
				    existing_package->pkgver, group->repos[existing_package->repo][source].repo->uri,
				    pkgver, group->repos[repo_serial][source].repo->uri);
				continue;
			}
			fprintf(stderr, "'%s' from '%s' is about to push out '%s' from '%s'\n",
			    pkgver, group->repos[repo_serial][source].repo->uri,
			    existing_package->pkgver,group->repos[existing_package->repo][source].repo->uri);
			package_release(existing_package);
			package_init(existing_package, pkg, repo_serial);
		} else {
			package_init(existing_package, pkg, repo_serial);
		}
	}
	xbps_object_iterator_release(iter);
	return 0;
}

static xbps_array_t
get_possibly_new_array(xbps_dictionary_t dict, const char *key) {
	xbps_array_t array = xbps_dictionary_get(dict, key);
	if (!array) {
		array = xbps_array_create();
		if (array) {
			xbps_dictionary_set_and_rel(dict, key, array);
		}
	}
	return array;
}

static xbps_dictionary_t
get_possibly_new_dictionary(xbps_dictionary_t dict, const char *key) {
	xbps_dictionary_t member = xbps_dictionary_get(dict, key);
	if (!member) {
		member = xbps_dictionary_create();
		if (member) {
			xbps_dictionary_set_and_rel(dict, key, member);
		}
	}
	return member;
}

static int
build_group(struct repos_group_t *group) {
	int rv = 0;

	for (int i = 0; i < group->repos_count; ++i) {
		for (enum source source = SOURCE_PUBLIC; source <= SOURCE_STAGE; ++source) {
			struct xbps_repo *repo = group->repos[i][source].repo;
			fprintf(stderr, "loading repo %d %p '%s', source %x\n", i, repo, (repo ? repo->uri : NULL), source);
			if (!repo) {
				continue;
			}
			rv = load_repo(group, repo, source, i);
			if (rv) {
				fprintf(stderr, "can't load '%s' repo into group, exiting\n", repo->uri);
				goto exit;
			}
		}
	}

	for (struct node_t *curr_node = group->nodes; curr_node; curr_node = curr_node->hh.next) {
		curr_node->source = SOURCE_STAGE;
		for (enum source source = SOURCE_PUBLIC; source <= SOURCE_STAGE; ++source) {
			struct package_t *curr_package = NULL;
			xbps_array_t shlib_provides = NULL;
			xbps_array_t provides = NULL;

			curr_package = &curr_node->packages[source];
			if (!curr_package->pkgver) {
				continue;
			}

			shlib_provides = xbps_dictionary_get(curr_package->dict, "shlib-provides");
			for (unsigned int i = 0; i < xbps_array_count(shlib_provides); i++) {
				const char *shlib = xbps_string_cstring_nocopy(xbps_array_get(shlib_provides, i));
				xbps_array_t providers;

				providers = get_possibly_new_array(group->shlib_providers, shlib);
				if (!providers) {
					return ENOMEM;
				}
				xbps_array_add_cstring_nocopy(providers, curr_package->pkgver);
			}

			provides = xbps_dictionary_get(curr_package->dict, "provides");
			for (unsigned int i = 0; i < xbps_array_count(provides); i++) {
				const char *virtual = xbps_string_cstring_nocopy(xbps_array_get(provides, i));
				xbps_dictionary_t providers;
				char virtual_pkgname[XBPS_NAME_SIZE] = {0};
				bool ok = false;

				ok = xbps_pkg_name(virtual_pkgname, sizeof virtual_pkgname, virtual);
				if (ok) {
					xbps_dbg_printf(group->xhp, "virtual '%s' (%s) provided by '%s'\n", virtual_pkgname, virtual, curr_node->pkgname);
				} else {
					xbps_dbg_printf(group->xhp, "invalid virtual pkgver '%s' provided by package '%s', ignoring\n", virtual, curr_node->pkgname);
					continue;
				}
				providers = get_possibly_new_dictionary(group->virtual_providers, owned_string(virtual_pkgname));
				if (!providers) {
					return ENOMEM;
				}
				xbps_dictionary_set_cstring_nocopy(providers, curr_package->pkgver, owned_string(virtual));
			}
		}
	}

exit:
	if (rv) {
		fprintf(stderr, "group failed to build\n");
		repo_group_purge_packages(group);
	}
	return rv;
}

static void
generate_constraints_add_update_remove(struct repos_group_t *group, PicoSAT* solver, struct node_t *curr_node)
{
	const char *curr_public_pkgver = curr_node->packages[SOURCE_PUBLIC].pkgver;
	const char *curr_stage_pkgver = curr_node->packages[SOURCE_STAGE].pkgver;
	char *last_dash = strrchr(curr_node->pkgname, '-');

	if (last_dash && strcmp(last_dash, "-dbg") == 0) {
		// debug packages should be kept in sync with packages they are generated from and not updated on its own pace
		curr_node->base_node = curr_node;
		for (enum source source = SOURCE_PUBLIC; source <= SOURCE_STAGE; ++source) {
			char basepkg[XBPS_MAXPATH] = {0};
			const char *curr_pkgver = curr_node->packages[source].pkgver;
			struct node_t *basepkg_node = NULL;
			unsigned int basepkgname_len = last_dash - curr_node->pkgname;
			int variable_curr;

			if (!curr_pkgver) {
				continue;
			}
			variable_curr = variable_real_package(curr_pkgver);
			memcpy(basepkg, curr_pkgver, basepkgname_len);
			HASH_FIND(hh, group->nodes, basepkg, basepkgname_len, basepkg_node);
			strcpy(basepkg + basepkgname_len, curr_pkgver + basepkgname_len + 4);
			if (!basepkg_node || ((!basepkg_node->packages[SOURCE_PUBLIC].pkgver || strcmp(basepkg_node->packages[SOURCE_PUBLIC].pkgver, basepkg) != 0) && (!basepkg_node->packages[SOURCE_STAGE].pkgver || strcmp(basepkg_node->packages[SOURCE_STAGE].pkgver, basepkg) != 0))) {
				struct clause *clause = clause_alloc(CLAUSE_TYPE_CERTAINTY, 1);
				clause->literals[0] = -variable_curr;
				clause_add(group, solver, clause, 1);
			} else {
				struct clause *clause = clause_alloc(CLAUSE_TYPE_EQUIVALENCE, 2);

				curr_node->base_node = basepkg_node;
				clause->literals[0] = variable_curr;
				clause->literals[1] = variable_real_package(basepkg);
				clause_add(group, solver, clause, 2);
			}
		}
	} else if (curr_public_pkgver && curr_stage_pkgver) {
		if (strcmp(curr_public_pkgver, curr_stage_pkgver) == 0) {
			struct clause *clause = clause_alloc(CLAUSE_TYPE_CERTAINTY, 1);
			clause->literals[0] = variable_real_package(curr_public_pkgver);
			clause_add(group, solver, clause, 1);
		} else {
			int public_variable = variable_real_package(curr_public_pkgver);
			int stage_variable = variable_real_package(curr_stage_pkgver);
			struct clause *clause = clause_alloc(CLAUSE_TYPE_EQUIVALENCE, 2);
			clause->literals[0] = public_variable;
			clause->literals[1] = -stage_variable;
			clause_add(group, solver, clause, 2);
			if (!group->explaining_pass) {
				picosat_assume(solver, stage_variable);
			}
		}
	} else if (curr_public_pkgver) {
		if (!group->explaining_pass) {
			picosat_assume(solver, -variable_real_package(curr_public_pkgver));
		}
	} else if (curr_stage_pkgver) {
		if (!group->explaining_pass) {
			picosat_assume(solver, variable_real_package(curr_stage_pkgver));
		}
	}
}

static void
generate_constraints_shlib_requires(struct repos_group_t *group, PicoSAT* solver, struct package_t *curr_package)
{
	xbps_array_t shlib_requires = NULL;

	shlib_requires = xbps_dictionary_get(curr_package->dict, "shlib-requires");
	for (unsigned int i = 0; i < xbps_array_count(shlib_requires); i++) {
		const char *shlib = xbps_string_cstring_nocopy(xbps_array_get(shlib_requires, i));
		struct clause *clause = clause_alloc(CLAUSE_TYPE_IMPLICATION, 2);
		clause->literals[0] = variable_real_package(curr_package->pkgver);
		clause->literals[1] = variable_shlib(shlib);
		clause_add(group, solver, clause, 2);
	}
}

static int
generate_constraints_depends(struct repos_group_t *group, PicoSAT* solver, struct package_t *curr_package)
{
	xbps_array_t run_depends = NULL;
	int rv = 0;

	run_depends = xbps_dictionary_get(curr_package->dict, "run_depends");
	for (unsigned int i = 0; i < xbps_array_count(run_depends); i++) {
		const char *deppattern = xbps_string_cstring_nocopy(xbps_array_get(run_depends, i));
		struct clause *clause = NULL;
		xbps_dictionary_t providers = NULL;
		char depname[XBPS_NAME_SIZE];
		int pv_idx = 0;
		bool ok = false;

		ok = xbps_pkgpattern_name(depname, sizeof depname, deppattern);
		if (!ok) {
			ok = xbps_pkg_name(depname, sizeof depname, deppattern);
		}
		if (!ok) {
			fprintf(stderr, "'%s' requires '%s' that has no package name\n", curr_package->pkgver, deppattern);
			rv = ENXIO;
			continue;
		}
		providers = xbps_dictionary_get(group->virtual_providers, depname);
		// virtual on left side + real public + real staged + providers
		clause = clause_alloc(CLAUSE_TYPE_IMPLICATION, xbps_dictionary_count(providers) + 3);
		clause->literals[pv_idx++] = variable_real_package(curr_package->pkgver);
		{
			struct node_t *dep_node = NULL;

			HASH_FIND(hh, group->nodes, depname, strlen(depname), dep_node);

			if (dep_node) {
				const char *dep_public_pkgver = dep_node->packages[SOURCE_PUBLIC].pkgver;
				const char *dep_stage_pkgver = dep_node->packages[SOURCE_STAGE].pkgver;
				if (dep_public_pkgver && xbps_pkgpattern_match(dep_public_pkgver, deppattern)) {
					clause->literals[pv_idx++] = variable_virtual_package(dep_public_pkgver);
				}
				if (dep_stage_pkgver && (!dep_public_pkgver || (strcmp(dep_public_pkgver, dep_stage_pkgver) != 0) ) && xbps_pkgpattern_match(dep_stage_pkgver, deppattern)) {
					clause->literals[pv_idx++] = variable_virtual_package(dep_stage_pkgver);
				}
			}
		}
		{
			if (providers) {
				xbps_object_iterator_t iter = NULL;
				xbps_dictionary_keysym_t keysym = NULL;

				iter = xbps_dictionary_iterator(providers);
				while ((keysym = xbps_object_iterator_next(iter))) {
					const char *virtual = xbps_string_cstring_nocopy(xbps_dictionary_get_keysym(providers, keysym));
					if (xbps_pkgpattern_match(virtual, deppattern)) {
						const char *provider = xbps_dictionary_keysym_cstring_nocopy(keysym);
						clause->literals[pv_idx++] = variable_virtual_package(provider);
					}
				}
				xbps_object_iterator_release(iter);
			}
		}
		clause->label = owned_string(deppattern);
		clause_add(group, solver, clause, pv_idx);
	}
	return rv;
}

static void
generate_constraints_virtual_or_real(struct repos_group_t *group, PicoSAT* solver, struct node_t *curr_node, struct package_t *curr_package)
{
	xbps_dictionary_t providers = xbps_dictionary_get(group->virtual_providers, curr_node->pkgname);
	// virtual package on left side + real package on right side + providers
	struct clause *clause = clause_alloc(CLAUSE_TYPE_EQUIVALENCE, xbps_dictionary_count(providers) + 2);
	int pv_idx = 0;
	int curr_package_real_variable = variable_real_package(curr_package->pkgver);
	int curr_package_virtual_variable = variable_virtual_from_real(curr_package_real_variable);

	clause->literals[pv_idx++] = curr_package_virtual_variable;
	clause->literals[pv_idx++] = curr_package_real_variable;
	if (providers) {
		xbps_object_iterator_t iter = xbps_dictionary_iterator(providers);
		xbps_dictionary_keysym_t keysym = NULL;

		while ((keysym = xbps_object_iterator_next(iter))) {
			const char *virtual = xbps_string_cstring_nocopy(xbps_dictionary_get_keysym(providers, keysym));
			if (strcmp(curr_package->pkgver, virtual) == 0) {
				const char *provider = xbps_dictionary_keysym_cstring_nocopy(keysym);
				clause->literals[pv_idx++] = variable_real_package(provider);
			}
		}
		xbps_object_iterator_release(iter);
	}
	clause_add(group, solver, clause, pv_idx);
}

static void
generate_constraints_virtual_pure(struct repos_group_t *group, PicoSAT* solver)
{
	xbps_object_iterator_t virtual_pkgs_iter = xbps_dictionary_iterator(group->virtual_providers);
	xbps_dictionary_keysym_t virtual_pkgs_keysym = NULL;

	while ((virtual_pkgs_keysym = xbps_object_iterator_next(virtual_pkgs_iter))) {
		const char *virtual_pkgname = xbps_dictionary_keysym_cstring_nocopy(virtual_pkgs_keysym);
		xbps_dictionary_t providers = xbps_dictionary_get_keysym(group->virtual_providers, virtual_pkgs_keysym);
		xbps_dictionary_t processed_pkgvers = xbps_dictionary_create();
		struct node_t *realpkg_node = NULL;
		xbps_object_iterator_t providers_outer_iter = NULL;
		xbps_dictionary_keysym_t providers_outer_keysym = NULL;

		HASH_FIND(hh, group->nodes, virtual_pkgname, strlen(virtual_pkgname), realpkg_node);

		if (realpkg_node) {
			if (realpkg_node->packages[SOURCE_PUBLIC].pkgver) {
				xbps_dictionary_set_bool(processed_pkgvers, realpkg_node->packages[SOURCE_PUBLIC].pkgver, true);
			}
			if (realpkg_node->packages[SOURCE_STAGE].pkgver) {
				xbps_dictionary_set_bool(processed_pkgvers, realpkg_node->packages[SOURCE_STAGE].pkgver, true);
			}
		}

		providers_outer_iter = xbps_dictionary_iterator(providers);
		while ((providers_outer_keysym = xbps_object_iterator_next(providers_outer_iter))) {
			const char *outer_virtual = xbps_string_cstring_nocopy(xbps_dictionary_get_keysym(providers, providers_outer_keysym));
			xbps_object_iterator_t providers_inner_iter = NULL;
			xbps_dictionary_keysym_t providers_inner_keysym = NULL;
			// virtual package on left side + providers
			struct clause *clause = NULL;
			int pv_idx = 0;
			int outer_virtual_variable = variable_virtual_package(outer_virtual);

			if (xbps_bool_true(xbps_dictionary_get(processed_pkgvers, outer_virtual))) {
				continue;
			}
			clause = clause_alloc(CLAUSE_TYPE_EQUIVALENCE, 1 + xbps_dictionary_count(providers));
			clause->literals[pv_idx++] = outer_virtual_variable;
			providers_inner_iter = xbps_dictionary_iterator(providers);
			while ((providers_inner_keysym = xbps_object_iterator_next(providers_inner_iter))) {
				const char *inner_provider = xbps_dictionary_keysym_cstring_nocopy(providers_inner_keysym);
				const char *inner_virtual = xbps_string_cstring_nocopy(xbps_dictionary_get_keysym(providers, providers_inner_keysym));
				if (strcmp(outer_virtual, inner_virtual) == 0) {
					clause->literals[pv_idx++] = variable_real_package(inner_provider);
				}
			}
			xbps_object_iterator_release(providers_inner_iter);
			clause_add(group, solver, clause, pv_idx);
			xbps_dictionary_set_bool(processed_pkgvers, outer_virtual, true);
		}
		xbps_object_iterator_release(providers_outer_iter);
		xbps_object_release(processed_pkgvers);
	}
	xbps_object_iterator_release(virtual_pkgs_iter);
}

static void
generate_constraints_shlib_provides(struct repos_group_t *group, PicoSAT* solver, struct package_t *curr_package, xbps_dictionary_t processed_providers)
{
	xbps_array_t shlib_requires = xbps_dictionary_get(curr_package->dict, "shlib-requires");

	for (unsigned int j = 0; j < xbps_array_count(shlib_requires); ++j) {
		const char *shlib = xbps_string_cstring_nocopy(xbps_array_get(shlib_requires, j));
		xbps_array_t providers = xbps_dictionary_get(group->shlib_providers, shlib);
		// library on left side + providers
		struct clause *clause = NULL;
		int pv_idx = 0;

		if (xbps_dictionary_get(processed_providers, shlib)) {
			continue;
		}
		clause = clause_alloc(CLAUSE_TYPE_EQUIVALENCE, xbps_array_count(providers) + 1);
		xbps_dictionary_set_bool(processed_providers, shlib, true);
		clause->literals[pv_idx++] = variable_shlib(shlib);
		for (unsigned int i = 0; i < xbps_array_count(providers); ++i) {
			const char *provider = xbps_string_cstring_nocopy(xbps_array_get(providers, i));
			clause->literals[pv_idx++] = variable_real_package(provider);
		}
		clause_add(group, solver, clause, pv_idx);
	}
}

static int
generate_constraints(struct repos_group_t *group, PicoSAT* solver)
{
	int rv = 0;
	xbps_dictionary_t processed_providers = xbps_dictionary_create();

	for (struct node_t *curr_node = group->nodes; curr_node; curr_node = curr_node->hh.next) {
		generate_constraints_add_update_remove(group, solver, curr_node);
		for (enum source source = SOURCE_PUBLIC; source <= SOURCE_STAGE; ++source) {
			struct package_t *curr_package = &curr_node->packages[source];

			if (!curr_package->pkgver) {
				continue;
			}
			generate_constraints_shlib_requires(group, solver, curr_package);
			generate_constraints_shlib_provides(group, solver, curr_package, processed_providers);
			rv |= generate_constraints_depends(group, solver, curr_package);
			generate_constraints_virtual_or_real(group, solver, curr_node, curr_package);
		}
	}
	generate_constraints_virtual_pure(group, solver);
	xbps_object_release(processed_providers);
	return rv;
}

static int
explain_inconsistency(struct repos_group_t *group) {
	// In picosat 965, picosat_coreclause is documented to not interact well with picosat_assume.
	// Therefore constraints are generated second time, without assumptions.
	PicoSAT *solver = picosat_init();
	int rv = 0;
	int decision;
	int clause_number = 0;

	picosat_enable_trace_generation(solver);
	group->explaining_pass = true;
	rv = generate_constraints(group, solver);
	if (rv) {
		fprintf(stderr, "Failed to generate constraints for explaining: %s\n", strerror(rv));
		goto exit;
	}
	decision = picosat_sat(solver, -1);
	if (decision != PICOSAT_UNSATISFIABLE) {
		fprintf(stderr, "Cannot explain inconsistency, expected state is %d, actual state is %d\n", PICOSAT_UNSATISFIABLE, decision);
		goto exit;
	}
	fprintf(stderr, "Inconsistent clauses:\n");
	for (struct clause *clause = group->clauses; clause; clause = clause->next) {
		for (int i = 0; i < clause->backing_clauses; ++i) {
			if (picosat_coreclause(solver, clause_number + i)) {
				clause_print(clause, stderr);
				break;
			}
		}
		clause_number += clause->backing_clauses;
	}
exit:
	picosat_reset(solver);
	return rv;
}

static int
update_repodata(struct repos_group_t *group) {
	int rv = 0;
	const int *correcting = NULL;
	PicoSAT *solver = picosat_init();

	rv = generate_constraints(group, solver);
	if (rv) {
		fprintf(stderr, "Failed to generate constraints: %s\n", strerror(rv));
		goto exit;
	}
	correcting = picosat_next_minimal_correcting_subset_of_assumptions(solver);
	if (!correcting) {
		fprintf(stderr, "Repodata is inconsistent and no updates in stagedata fix it\n");
		picosat_reset(solver);
		explain_inconsistency(group);
		return EPROTO;
	}
	xbps_dbg_printf(group->xhp, "correcting set: %p\n",correcting);
	for (;correcting && *correcting; ++correcting) {
		struct node_t *node = NULL;
		char pkgname[XBPS_NAME_SIZE] = {0};
		const char *pkgver = variable_name(*correcting);

		xbps_pkg_name(pkgname, sizeof pkgname, pkgver);
		printf("not updating '%s'\n", pkgver);
		HASH_FIND(hh, group->nodes, pkgname, strlen(pkgname), node);
		if (!node) {
			fprintf(stderr, "No package '%s' (%s) found\n", pkgname, pkgver);
			rv = EFAULT;
			goto exit;
		}
		node->source = SOURCE_PUBLIC;
	}
	for (struct node_t *curr_node = group->nodes; curr_node; curr_node = curr_node->hh.next) {
		const char *base_pkgver = NULL;
		const char *base_version = NULL;

		if (!curr_node->base_node) {
			continue;
		}
		curr_node->source = SOURCE_NONE;
		if (curr_node->base_node == curr_node) {
			continue;
		}
		base_pkgver = curr_node->base_node->packages[curr_node->base_node->source].pkgver;
		if (!base_pkgver) {
			continue;
		}
		base_version = xbps_pkg_version(base_pkgver);
		for (enum source curr_source = SOURCE_PUBLIC; curr_source <= SOURCE_STAGE; ++curr_source) {
			const char *curr_pkgver = curr_node->packages[curr_source].pkgver;
			const char *curr_version = NULL;

			if (!curr_pkgver) {
				continue;
			}
			curr_version = xbps_pkg_version(curr_pkgver);
			if (base_version && curr_version && strcmp(base_version, curr_version) == 0) {
				curr_node->source = curr_source;
			}
		}
	}
exit:
	picosat_reset(solver);
	return rv;
}

static int
write_repos(struct repos_group_t *group, const char *compression, char *repos[]) {
	xbps_dictionary_t* dictionaries = NULL;
	int rv = 0;
	bool need_write = group->pushed_out_packages;

	dictionaries = calloc(group->repos_count, sizeof *dictionaries);
	if (!dictionaries) {
		fprintf(stderr, "failed to allocate memory\n");
		return EFAULT;
	}
	for (int i = 0; i < group->repos_count; ++i) {
		dictionaries[i] = xbps_dictionary_create();
		if (!dictionaries[i]) {
			fprintf(stderr, "failed to allocate memory\n");
			rv = ENOMEM;
			goto exit;
		}
	}
	for (struct node_t *node = group->nodes; node; node = node->hh.next) {
		struct package_t *package;

		if (node->source == SOURCE_NONE) {
			if (node->packages[SOURCE_PUBLIC].pkgver) {
				need_write = true;
				printf("Removing '%s'\n", node->packages[SOURCE_PUBLIC].pkgver);
			}
			continue;
		}
		package = &node->packages[node->source];
		if (node->source == SOURCE_STAGE) {
			if (!node->packages[SOURCE_PUBLIC].pkgver) {
				need_write = true;
				printf("Adding '%s'\n", package->pkgver);
			} else if (!package->pkgver) {
				need_write = true;
				printf("Removing '%s'\n", node->packages[SOURCE_PUBLIC].pkgver);
			} else if (strcmp(node->packages[SOURCE_PUBLIC].pkgver, package->pkgver) != 0) {
				need_write = true;
				printf("Updating from '%s' to '%s'\n", node->packages[SOURCE_PUBLIC].pkgver, package->pkgver);
			}
		}
		if (package->dict) {
			xbps_dictionary_set(dictionaries[package->repo], node->pkgname, package->dict);
			xbps_dbg_printf(group->xhp, "Putting %s (%s) into %s \n", node->pkgname, package->pkgver, repos[package->repo]);
		}
	}
	if (need_write) {
		for (int i = 0; i < group->repos_count; ++i) {
			xbps_repodata_flush(group->xhp, repos[i], "repodata", dictionaries[i], group->repos[i][SOURCE_PUBLIC].meta, compression);
		}
	} else {
		xbps_dbg_printf(group->xhp, "No updates to write\n");
	}
exit:
	for (int i = 0; i < group->repos_count; ++i) {
		if (dictionaries[i]) {
			xbps_object_release(dictionaries[i]);
		}
	}
	free(dictionaries);
	return rv;
}

int
index_repos(struct xbps_handle *xhp, const char *compression, int argc, char *argv[])
{
	int rv = 0;
	struct repos_group_t group;

	repo_group_init(&group, xhp, argc);
	for (int i = 0; i < group.repos_count; ++i) {
		const char *path = argv[i];
		struct repo_t *public = &group.repos[i][SOURCE_PUBLIC];
		struct repo_t *stage = &group.repos[i][SOURCE_STAGE];
		bool locked = xbps_repo_lock(xhp, path, &public->lock_fd, &public->lock_name);

		if (!locked) {
			rv = errno;
			fprintf(stderr, "repo '%s' failed to lock\n", path);
			goto exit;
		}
		public->repo = xbps_repo_public_open(xhp, path);
		if (public->repo) {
			public->idx = public->repo->idx;
			public->meta = public->repo->idxmeta;
			xbps_object_retain(public->idx);
		} else if (errno == ENOENT) {
			public->idx = xbps_dictionary_create();
			public->meta = NULL;
			xbps_dbg_printf(group.xhp, "repo index '%s' is not there\n", path);
		} else {
			fprintf(stderr, "repo index '%s' failed to open\n", path);
			rv = errno;
			goto exit;
		}
		stage->repo = xbps_repo_stage_open(xhp, path);
		if (stage->repo) {
			stage->idx = stage->repo->idx;
			stage->meta = stage->repo->idxmeta;
		} else if (errno == ENOENT) {
			xbps_dbg_printf(group.xhp, "repo stage '%s' is not there\n", path);
		} else {
			fprintf(stderr, "repo stage '%s' failed to open\n", path);
			rv = errno;
			goto exit;
		}
	}
	rv = build_group(&group);
	for (int i = 0; i < group.repos_count; ++i) {
		xbps_object_release(group.repos[i][SOURCE_PUBLIC].idx);
	}
	if (rv) {
		goto exit;
	}
	rv = update_repodata(&group);
	if (!rv) {
		rv = write_repos(&group, compression, argv);
	}
exit:
	for (int i = group.repos_count - 1; i >= 0; --i) {
		if (group.repos[i][SOURCE_PUBLIC].lock_fd) {
			xbps_repo_unlock(group.repos[i][SOURCE_PUBLIC].lock_fd, group.repos[i][SOURCE_PUBLIC].lock_name);
		}
	}
	repo_group_release(&group);
	free_variables();
	free_owned_strings();
	return rv;
}
