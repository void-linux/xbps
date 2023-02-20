#ifndef IDTREE_H
#define IDTREE_H

#include <sys/types.h>

struct idtree {
	long id;
	char *name;
	struct idtree *left, *right;
	int level;
};

char * idtree_username(struct idtree *, uid_t);
char * idtree_groupname(struct idtree *, gid_t);
void idtree_free(struct idtree *);

#endif /* IDTREE_H */
