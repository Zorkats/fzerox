#ifndef LIBC_STDDEF_H
#define LIBC_STDDEF_H

/* macOS: the SDK headers conflict with these N64 libc replacements, so use the system ones. */
#if defined(PORT) && defined(__APPLE__)
#include <stddef.h>
#else
#include "PR/ultratypes.h"

#ifdef __ANDROID__
/* Bionic's <strings.h> -- which PR/os_libc.h includes on a PORT build, for the real bcopy/bzero
 * signatures -- pulls in <sys/types.h> and through it the compiler's <stddef.h>. glibc's does
 * not, which is why the IDO typedef below has survived on desktop: nothing there ever offers a
 * competing definition. Here it collides, and the compiler's is the one that is right for an
 * LP64 target, so take it.
 *
 * Nothing in the game uses ptrdiff_t, so this widens no interface -- it only stops the two
 * definitions meeting. */
#include <stddef.h>
#else
typedef s32 ptrdiff_t;
#endif

#ifdef __GNUC__
#define offsetof(structure, member) __builtin_offsetof (structure, member)
#else
#define offsetof(structure, member) ((size_t)&(((structure*)0)->member))
#endif
#endif

#endif /* STDDEF_H */
