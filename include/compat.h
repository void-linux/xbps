#ifndef COMPAT_H
#define COMPAT_H

#include <sys/types.h>

#include <stdarg.h>
#include <stdint.h>

#include "macro.h"


#ifndef HAVE_HUMANIZE_NUMBER
#define HN_DECIMAL              0x01
#define HN_NOSPACE              0x02
#define HN_B                    0x04
#define HN_DIVISOR_1000         0x08
#define HN_GETSCALE             0x10
#define HN_AUTOSCALE            0x20
int HIDDEN humanize_number(char *, size_t, int64_t, const char *, int, int);
#endif

#endif /* COMPAT_H */
