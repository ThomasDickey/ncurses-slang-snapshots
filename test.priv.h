/****************************************************************************
 * Copyright 2017 by Thomas E. Dickey                                       *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * This is a supporting work for discussion of the ncurses and slang        *
 * libraries, consequently the permission notice requires this URL to be    *
 * included:                                                                *
 *      https://invisible-island.net/ncurses/ncurses-slang.html             *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/
/*
 * $Id: test.priv.h,v 1.3 2017/11/15 23:11:54 tom Exp $
 * Use this to compile picsmap.c as a header-file for picsmap_slang
 */

#ifndef __TEST_PRIV_H
#define __TEST_PRIV_H 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <slang.h>

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifdef __GNUC__
#define GCC_NORETURN __attribute__((noreturn))
#else
#define GCC_NORETURN		/* nothing */
#endif

#ifndef GCC_PRINTFLIKE
#if defined(GCC_PRINTF) && !defined(printf)
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var)	/*nothing */
#endif
#endif

#ifndef GCC_SCANFLIKE
#if defined(GCC_SCANF) && !defined(scanf)
#define GCC_SCANFLIKE(fmt,var)  __attribute__((format(scanf,fmt,var)))
#else
#define GCC_SCANFLIKE(fmt,var)	/*nothing */
#endif
#endif

#ifndef GCC_UNUSED
#define GCC_UNUSED  __attribute__((unused))
#endif

#define typeMalloc(type,n) (type *) malloc((size_t)(n) * sizeof(type))
#define typeCalloc(type,elts) (type *) calloc((size_t)(elts), sizeof(type))
#define typeRealloc(type,n,p) (type *) realloc(p, (size_t)(n) * sizeof(type))

typedef SLsmg_Char_Type NCURSES_CH_T;
typedef short NCURSES_COLOR_T;
typedef unsigned chtype;

#define LINES SLtt_Screen_Rows
#define COLS  SLtt_Screen_Cols

#ifndef DATA_DIR
#define DATA_DIR  "."
#endif

#define UChar(n)  (unsigned char)(n)
#define SIZEOF(n) (sizeof(n)/sizeof(n[0]))
#define ExitProgram(code) exit(code)

#define USE_TERM_DRIVER 1

#define HAVE_STDINT_H   1
#define HAVE_TSEARCH    1
//#define HAVE_TDESTROY   1

#endif /* __TEST_PRIV_H */
