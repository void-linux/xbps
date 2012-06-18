#ifndef COMPAT_H
#define COMPAT_H

#include <sys/types.h>
#include <stdarg.h>
#include "xbps_api_impl.h"

#ifndef HAVE_STRLCAT
size_t HIDDEN strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCPY
size_t HIDDEN strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRCASESTR
char HIDDEN *strcasestr(const char *, const char *);
#endif

#if defined(HAVE_VASPRINTF) && !defined(_GNU_SOURCE)
int HIDDEN vasprintf(char **, const char *, va_list);
#endif

#endif /* COMPAT_H */
