#ifndef COMPAT_H
#define COMPAT_H

#include <inttypes.h>
#include <sys/types.h>
#include <stdarg.h>

#if HAVE_VISIBILITY
#define HIDDEN __attribute__ ((visibility("hidden")))
#else
#define HIDDEN
#endif

#ifndef HAVE_STRLCAT
size_t HIDDEN strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCPY
size_t HIDDEN strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRCASESTR
char HIDDEN *strcasestr(const char *, const char *);
#endif

#ifndef HAVE_VASPRINTF
int HIDDEN vasprintf(char **, const char *, va_list);
#endif

#ifndef HAVE_HUMANIZE_NUMBER
/* Values for humanize_number(3)'s flags parameter. */
#define	HN_DECIMAL		0x01
#define	HN_NOSPACE		0x02
#define	HN_B			0x04
#define	HN_DIVISOR_1000		0x08
#define	HN_IEC_PREFIXES		0x10

/* Values for humanize_number(3)'s scale parameter. */
#define	HN_GETSCALE		0x10
#define	HN_AUTOSCALE		0x20

int HIDDEN humanize_number(char *_buf, size_t _len, int64_t _number,
		const char *_suffix, int _scale, int _flags);
#endif

#endif /* COMPAT_H */
