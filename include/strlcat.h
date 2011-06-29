#ifndef STRLCAT_H
#define STRLCAT_H

#include <sys/types.h>

#ifndef HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif

#endif
