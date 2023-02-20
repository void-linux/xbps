/*
 * Copyright (C) 2015-2020 Leah Neukirchen
 * Parts of code derived from musl libc, which is
 * Copyright (C) 2005-2014 Rich Felker, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

#include "idtree.h"

/* AA-tree implementation, adapted from https://github.com/ccxvii/minilibs */

static struct idtree idtree_sentinel = { 0, 0, &idtree_sentinel, &idtree_sentinel, 0 };

static struct idtree *
idtree_make(long id, char *name)
{
	struct idtree *node = malloc(sizeof (struct idtree));
	node->id = id;
	node->name = name;
	node->left = node->right = &idtree_sentinel;
	node->level = 1;
	return node;
}

static char *
idtree_lookup(struct idtree *node, long id)
{
	if (node) {
		while (node != &idtree_sentinel) {
			if (id == node->id)
				return node->name;
			else if (id < node->id)
				node = node->left;
			else
				node = node->right;
		}
	}

	return 0;
}

static struct idtree *
idtree_skew(struct idtree *node)
{
	if (node->left->level == node->level) {
		struct idtree *save = node;
		node = node->left;
		save->left = node->right;
		node->right = save;
	}
	return node;
}

static struct idtree *
idtree_split(struct idtree *node)
{
	if (node->right->right->level == node->level) {
		struct idtree *save = node;
		node = node->right;
		save->right = node->left;
		node->left = save;
		node->level++;
	}
	return node;
}

static struct idtree *
idtree_insert(struct idtree *node, long id, char *name)
{
	if (node && node != &idtree_sentinel) {
		if (id == node->id)
			return node;
		else if (id < node->id)
			node->left = idtree_insert(node->left, id, name);
		else
			node->right = idtree_insert(node->right, id, name);
		node = idtree_skew(node);
		node = idtree_split(node);
		return node;
	}
	return idtree_make(id, name);
}
/**/

static char *
strid(long id)
{
	static char buf[32];
	snprintf(buf, sizeof buf, "%ld", id);
	return buf;
}

char *
idtree_groupname(struct idtree *groups, gid_t gid)
{
	char *name = idtree_lookup(groups, gid);
	struct group *g;

	if (name)
		return name;

	g = getgrgid(gid);
	if (g) {
		name = strdup(g->gr_name);
		groups = idtree_insert(groups, gid, name);
		return name;
	}

	return strid(gid);
}

char *
idtree_username(struct idtree *users, uid_t uid)
{
	char *name = idtree_lookup(users, uid);
	struct passwd *p;

	if (name)
		return name;

	p = getpwuid(uid);
	if (p) {
		name = strdup(p->pw_name);
		users = idtree_insert(users, uid, name);
		return name;
	}

	return strid(uid);
}

void
idtree_free(struct idtree *node) {
	if (node && node != &idtree_sentinel) {
		idtree_free(node->left);
		idtree_free(node->right);
		free(node);
	}
}
