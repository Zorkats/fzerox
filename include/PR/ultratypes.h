#ifndef _ULTRATYPES_H_
#define _ULTRATYPES_H_


/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/


/*************************************************************************
 *
 *  File: ultratypes.h
 *
 *  This file contains various types used in Ultra64 interfaces.
 *
 *  $Revision: 1.6 $
 *  $Date: 1997/12/17 04:02:06 $
 *  $Source: /exdisk2/cvs/N64OS/Master/cvsmdev2/PR/include/ultratypes.h,v $
 *
 **************************************************************************/



/**********************************************************************
 * General data types for R4300
 */
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/* PORT (host builds): the IDO-era `long` spellings below are 32-bit only on ILP32 (the
 * N64) and LLP64 (Windows). On LP64 Linux/macOS `long` is 64-bit, which would double the
 * size of every u32/s32 field and break every struct layout, save format, and pointer
 * pun in the decomp. Detect via the compiler's own data-model macro and use `int` (32-bit
 * on every supported host ABI) there; keep the original spellings everywhere else so the
 * matching N64/Windows builds are textually untouched. */
#if defined(__LP64__) /* any LP64 host build; N64 MIPS builds are ILP32 and never define this */
typedef unsigned char       u8;     /* unsigned  8-bit */
typedef unsigned short      u16;    /* unsigned 16-bit */
typedef unsigned int        u32;    /* unsigned 32-bit */
typedef unsigned long long  u64;    /* unsigned 64-bit */

typedef signed char         s8;     /* signed  8-bit */
typedef short               s16;    /* signed 16-bit */
typedef int                 s32;    /* signed 32-bit */
typedef long long           s64;    /* signed 64-bit */

typedef volatile unsigned char      vu8;    /* unsigned  8-bit */
typedef volatile unsigned short     vu16;   /* unsigned 16-bit */
typedef volatile unsigned int       vu32;   /* unsigned 32-bit */
typedef volatile unsigned long long vu64;   /* unsigned 64-bit */

typedef volatile signed char        vs8;    /* signed  8-bit */
typedef volatile short              vs16;   /* signed 16-bit */
typedef volatile int                vs32;   /* signed 32-bit */
typedef volatile long long          vs64;   /* signed 64-bit */
#else
typedef unsigned char       u8;     /* unsigned  8-bit */
typedef unsigned short      u16;    /* unsigned 16-bit */
typedef unsigned long       u32;    /* unsigned 32-bit */
typedef unsigned long long  u64;    /* unsigned 64-bit */

typedef signed char         s8;     /* signed  8-bit */
typedef short               s16;    /* signed 16-bit */
typedef long                s32;    /* signed 32-bit */
typedef long long           s64;    /* signed 64-bit */

typedef volatile unsigned char      vu8;    /* unsigned  8-bit */
typedef volatile unsigned short     vu16;   /* unsigned 16-bit */
typedef volatile unsigned long      vu32;   /* unsigned 32-bit */
typedef volatile unsigned long long vu64;   /* unsigned 64-bit */

typedef volatile signed char        vs8;    /* signed  8-bit */
typedef volatile short              vs16;   /* signed 16-bit */
typedef volatile long               vs32;   /* signed 32-bit */
typedef volatile long long          vs64;   /* signed 64-bit */
#endif

typedef float   f32;    /* single prec floating point */
typedef double  f64;    /* double prec floating point */

#if !defined(_SIZE_T) && !defined(_SIZE_T_) && !defined(_SIZE_T_DEF)
#define _SIZE_T
#define _SIZE_T_DEF         /* exeGCC size_t define label */
#if (_MIPS_SZLONG == 32)
typedef unsigned int    size_t;
#elif (_MIPS_SZLONG == 64)
typedef unsigned long   size_t;
#elif defined(__GNUC__) && !defined(_MIPS_SZLONG)
/* Host build on GCC/Clang: _MIPS_SZLONG is undefined, so neither MIPS branch above
 * typedefs size_t -- yet the _SIZE_T guard set above ALSO makes glibc's <stddef.h>
 * skip its own definition (it honors that guard), leaving size_t undefined for the
 * whole decomp build. Use the compiler built-in so the definition always matches
 * the host ABI. MSVC is unaffected (its runtime uses _SIZE_T_DEFINED and defines
 * size_t before any decomp header runs). */
typedef __SIZE_TYPE__ size_t;
#endif
#endif

#endif  /* _LANGUAGE_C */

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

#ifndef NULL
#define NULL    0
#endif

#endif  /* _ULTRATYPES_H_ */
