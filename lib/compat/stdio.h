#ifndef XBPS_COMPAT_STDIO_H
#define XBPS_COMPAT_STDIO_H

#include_next <stdio.h>

#ifndef HAVE_VASPRINTF
int vasprintf(char **, const char *, va_list);
#endif

#endif /*!XBPS_COMPAT_STDIO_H*/
