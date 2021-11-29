/*
 * $Id: vl_stdio.h,v 1.5 2007/05/05 15:24:04 tom Exp $
 *
 * Copyright 2007, Thomas E. Dickey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, distribute with modifications, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#ifndef VL_STDIO_H_incl
#define VL_STDIO_H_incl 1

#include <stdio.h>

/*
 * The non-threadsafe versions of getc/putc are generally faster.
 */
#ifdef HAVE_GETC_UNLOCKED
#define vl_getc(fp) getc_unlocked(fp)
#else
#define vl_getc(fp) getc(fp)
#endif

#ifdef HAVE_PUTC_UNLOCKED
#define vl_putc(c,fp) putc_unlocked(c,fp)
#else
#define vl_putc(c,fp) putc(c,fp)
#endif

#define vl_fputs(s,fp) fputs(s,fp)
#define vl_fgets(s,n,fp) fgets(s,n,fp)

#endif /* VL_STDIO_H_incl */
