#ifndef _XBPS_MACRO_H
#define _XBPS_MACRO_H

#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x))[0])

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#if __has_builtin(__builtin_imaxabs)
# define ABS(x) __builtin_imaxabs((x))
#elif __has_builtin(__builtin_llabs)
# define ABS(x) __builtin_llabs((x))
#else
# define ABS(x) ((x) > 0 ? (x) : -(x))
#endif

#define typeof(x) __typeof__(x)

#define container_of(ptr, type, member)                                        \
	({                                                                     \
		const typeof(((type *)0)->member) *__mptr = ptr;               \
		((type *)((uintptr_t)__mptr - offsetof(type, member)));        \
	})

#define struct_size(ptr, member, count)                                        \
	((sizeof(*(ptr)->member) * (count)) + sizeof(*(ptr)))

#define STRLEN(x) (sizeof("" x "") - sizeof(typeof(x[0])))

#define streq(a, b)         (strcmp((a), (b)) == 0)
#define strneq(a, b, n)     (strncmp((a), (b), (n)) == 0)
#define strcaseeq(a, b)     (strcasecmp((a), (b)) == 0)
#define strcaseneq(a, b, n) (strcasencmp((a), (b), (n)) == 0)

#endif /* !_XBPS_MACRO_H */
