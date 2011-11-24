#ifndef COMPAT_H
#define COMPAT_H

#include <sys/types.h>
#include <stdarg.h>

#ifndef HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRCASESTR
char *strcasestr(const char *, const char *);
#endif

#if defined(HAVE_VASPRINTF) && !defined(_GNU_SOURCE)
int vasprintf(char **, const char *, va_list);
#endif

#endif /* COMPAT_H */
