#ifndef XBPS_MACRO_H
#define XBPS_MACRO_H

#include <stdio.h>

/*
 * By default all public functions have default visibility, unless
 * visibility has been detected by configure and the HIDDEN definition
 * is used.
 */
#if HAVE_VISIBILITY
#define HIDDEN __attribute__ ((visibility("hidden")))
#else
#define HIDDEN
#endif

#ifndef __UNCONST
#define __UNCONST(a)	((void *)(uintptr_t)(const void *)(a))
#endif

#ifndef __arraycount
#define __arraycount(x) (sizeof(x) / sizeof(*x))
#endif


/* XXX: struct_size overflow check */
#define struct_size(p, member, count) \
		((sizeof(*(p)->member) * count) + sizeof(*(p)))

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        void *__mptr = (void *)(ptr);                           \
        ((type *)(__mptr - offsetof(type, member))); })
#endif

#if 0
#undef assert
#define assert(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "assert failed %s\n", #expr); \
		abort(); \
	} \
} while(0)
#endif


#endif /*!XBPS_MACRO_H*/
