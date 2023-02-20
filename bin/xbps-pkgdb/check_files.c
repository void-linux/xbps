#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "defs.h"

int
file_mode_check(const char *file, const mode_t mode) {
	struct stat sb;

	assert(file != NULL);
	assert(mode);

	if (lstat(file, &sb) == -1)
		return -errno;

	if (sb.st_mode != mode)
		return ERANGE;

	return 0;
}

int
file_user_check(struct idtree * idt, const char *file, const char *user) {
	struct stat sb;
	char *act_user;

	assert(file != NULL);
	assert(user != NULL);

	if (lstat(file, &sb) == -1)
		return -errno;

	act_user = idtree_username(idt, sb.st_uid);
	return strcmp(user, act_user) == 0 ? 0 : ERANGE;
}

int
file_group_check(struct idtree * idt, const char *file, const char *grp) {
	struct stat sb;
	char *act_grp;

	assert(file != NULL);
	assert(grp != NULL);

	if (lstat(file, &sb) == -1)
		return -errno;

	act_grp = idtree_groupname(idt, sb.st_gid);
	return strcmp(grp, act_grp) == 0 ? 0 : ERANGE;
}
