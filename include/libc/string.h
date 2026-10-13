#ifndef LIBC_STRING_H
#define LIBC_STRING_H

/* macOS: the SDK headers conflict with these N64 libc replacements, so use the system ones. */
#if defined(PORT) && defined(__APPLE__)
#include <string.h>
#else
#include "libc/stddef.h"

const char* strchr(const char* s, int c);
size_t strlen(const char* s);
void* memcpy(void* s1, const void* s2, size_t n);
#endif

#endif
