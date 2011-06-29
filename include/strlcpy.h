#ifndef STRLCPY_H
#define STRLCPY_H

#include <sys/types.h>

#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#endif
