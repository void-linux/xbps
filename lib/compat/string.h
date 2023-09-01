#ifndef XBPS_COMPAT_STRING_H
#define XBPS_COMPAT_STRING_H

#include_next <string.h>

#ifndef HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRCASESTR
char *strcasestr(const char *, const char *);
#endif

#endif /*!XBPS_COMPAT_STRING_H*/
