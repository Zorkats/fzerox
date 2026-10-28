#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

/* macOS: the SDK headers conflict with these N64 libc replacements, so use the system ones. */
#if defined(PORT) && defined(__APPLE__)
#include <stdlib.h>
#else
#include "libc/stddef.h"

typedef struct {
    /* 0x0 */ int quot;
    /* 0x4 */ int rem;
} div_t;

typedef struct {
    /* 0x0 */ long quot;
    /* 0x4 */ long rem;
} ldiv_t;

typedef struct {
    /* 0x0 */ long long quot;
    /* 0x8 */ long long rem;
} lldiv_t;

#ifdef __ANDROID__
/* Both come from Bionic here, reached through the <strings.h> that PR/os_libc.h includes on a
 * PORT build -- and its ssize_t is a plain typedef with no guard macro to suppress, so the
 * declarations below cannot coexist with it. See the note in libc/stddef.h.
 *
 * ssize_t is the only one of the two the game uses (the AudioLoad/AudioHeap size parameters),
 * and it takes small sizes that a wider type carries unchanged. */
#include <sys/types.h>
#include <stddef.h>
#else
typedef int ssize_t;

typedef long wchar_t;
#endif

ldiv_t ldiv(long num, long denom);
lldiv_t lldiv(long long num, long long denom);
#endif

#endif /* STDLIB_H */
