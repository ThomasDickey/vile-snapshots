/*
 * $Id: vl_regex.h,v 1.10 2019/12/19 09:38:15 tom Exp $
 *
 * Copyright 2005-2015,2019 Thomas E. Dickey and Paul G. Fox
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

#ifndef VL_REGEX_H_incl
#define VL_REGEX_H_incl 1

#ifndef OPT_VILE_CTYPE
#define OPT_VILE_CTYPE 0
#endif

/*
 * Definitions etc. for regexp(3) routines.
 *
 *	the regexp code is:
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 */
#define NSUBEXP  10
typedef struct regexp {
    char *startp[NSUBEXP];
    char *endp[NSUBEXP];
    size_t mlen;		/* convenience:  endp[0] - startp[0] */
    int regstart;		/* Internal use only. */
    char reganch;		/* Internal use only. */
    int regmust;		/* Internal use only. */
    size_t regmlen;		/* Internal use only. */
    size_t size;		/* vile addition -- how big is this */
    size_t uppercase;		/* vile addition -- uppercase chars in pattern */
    char program[1];		/* Unwarranted chumminess with compiler. */
} regexp;

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	REGEXP_MAGIC	0234

#ifndef CHARBITS
#define	UCHAR_AT(p)	((int)*(unsigned const char *)(p))
#else
#define	UCHAR_AT(p)	((int)*(p)&CHARBITS)
#endif

/* end of regexp stuff */

#ifndef GCC_PRINTFLIKE
#ifdef GCC_PRINTF
#define GCC_PRINTFLIKE(fmt,var) __attribute__((format(printf,fmt,var)))
#else
#define GCC_PRINTFLIKE(fmt,var)	/*nothing */
#endif
#endif /* GCC_PRINTFLIKE */

#ifndef _estruct_h
extern void mlforce(const char *fmt, ...) GCC_PRINTFLIKE(1,2);
#endif /* _estruct_h */
/* *INDENT-OFF* */
extern void regerror (const char *s);
extern regexp * regcomp (const char *origexp, size_t exp_len, int magic);
extern int regexec (regexp *prog, char *string, char *stringend, int startoff, int endoff, int ic);
extern int regexec2 (regexp *prog, char *string, char *stringend, int startoff, int endoff, int at_bol, int ic);
extern void regfree (regexp *prog);
extern char *regparser (const char **s);
/* *INDENT-ON* */

#endif /* VL_REGEX_H_incl */
