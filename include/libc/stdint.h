#ifndef LIBC_STDINT_H
#define LIBC_STDINT_H

#include "PR/ultratypes.h"

#if defined(PORT) && defined(__LP64__)
/* Spelled exactly like glibc's <stdint.h> (long, not long long, same width either way)
 * so a TU that sees both headers gets identical typedefs, not a conflicting-types error. */
typedef long intptr_t;
typedef unsigned long uintptr_t;
#elif defined(PORT)
/* Windows LLP64: host pointers are 64-bit, and the N64-width uintptr_t below truncated
 * every pointer round-tripped through game code into a low32 token. */
typedef s64 intptr_t;
typedef u64 uintptr_t;
#else
typedef s32 intptr_t;
typedef u32 uintptr_t;
#endif

#define INT8_MIN    (-0x80)
#define INT16_MIN   (-0x8000)
#define INT32_MIN   (-0x80000000)
#define INT64_MIN   (-0x8000000000000000)

#define INT8_MAX    0x7F
#define INT16_MAX   0x7FFF
#define INT32_MAX   0x7FFFFFFF
#define INT64_MAX   0x7FFFFFFFFFFFFFFF

#define UINT8_MAX   0xFF
#define UINT16_MAX  0xFFFF
#define UINT32_MAX  0xFFFFFFFF
#define UINT64_MAX  0xFFFFFFFFFFFFFFFF

#ifdef PORT
#define INTPTR_MIN  (-0x8000000000000000)
#define INTPTR_MAX  0x7FFFFFFFFFFFFFFF
#define UINTPTR_MAX 0xFFFFFFFFFFFFFFFF
#else
#define INTPTR_MIN  (-0x80000000)
#define INTPTR_MAX  0x7FFFFFFF
#define UINTPTR_MAX 0xFFFFFFFF
#endif


#endif /* STDINT_H */
