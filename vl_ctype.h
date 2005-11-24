/*
 * $Header: /users/source/archives/vile.vcs/RCS/vl_ctype.h,v 1.2 2005/11/20 19:57:02 tom Exp $
 *
 * Character-type tests, like <ctype.h> for vile (vi-like-emacs).
 *
 * Copyright 2005, Thomas E. Dickey
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

#ifndef VL_CTYPE_H_incl
#define VL_CTYPE_H_incl 1

#ifndef SMALLER
#define	SMALLER	0	/* strip some fluff -- not a lot smaller, but some */
#endif

#ifndef OPT_VILE_CTYPE
#define OPT_VILE_CTYPE 1
#endif

#ifndef OPT_WIDE_CTYPES
#define OPT_WIDE_CTYPES !SMALLER		/* extra char-types tests */
#endif

#define UCHAR unsigned char

#define N_chars    256			/* must be a power-of-2		*/

#define EOS        '\0'
#define BQUOTE     '`'
#define SQUOTE     '\''
#define DQUOTE     '"'
#define BACKSLASH  '\\'
#define CH_TILDE   '~'

# undef  istype
# undef  isAlnum
# undef  isAlpha
# undef  isCntrl
# undef  isDigit
# undef  isLower
# undef  isPrint
# undef  isPunct
# undef  isSpace
# undef  isUpper
# undef  toUpper
# undef  toLower
# undef  isident
# undef  isXDigit

#define CharOf(c)	((unsigned char)(c))

#if OPT_VILE_CTYPE

/* these are the bits that go into the vl_chartypes_ array */
/* the macros below test for them */
#define chrBIT(n) ((CHARTYPE)(1L<<(n)))

typedef enum {
	vl_UPPER = 0
	, vl_LOWER
	, vl_DIGIT
	, vl_SPACE
	, vl_BSPACE
	, vl_CNTRL
	, vl_PRINT
	, vl_PUNCT
	, vl_IDENT
	, vl_PATHN
	, vl_WILD
	, vl_LINESPEC
	, vl_FENCE
	, vl_NONSPACE
	, vl_QIDENT
#if OPT_WIDE_CTYPES
	, vl_SCRTCH
	, vl_SHPIPE
	, vl_XDIGIT
#endif
	, vl_UNUSED
} VL_CTYPES;

#define vl_upper    chrBIT(vl_UPPER)	/* upper case */
#define vl_lower    chrBIT(vl_LOWER)	/* lower case */
#define vl_digit    chrBIT(vl_DIGIT)	/* digits */
#define vl_space    chrBIT(vl_SPACE)	/* whitespace */
#define vl_bspace   chrBIT(vl_BSPACE)	/* backspace character (^H, DEL, and user's) */
#define vl_cntrl    chrBIT(vl_CNTRL)	/* control characters, including DEL */
#define vl_print    chrBIT(vl_PRINT)	/* printable */
#define vl_punct    chrBIT(vl_PUNCT)	/* punctuation */
#define vl_ident    chrBIT(vl_IDENT)	/* is typically legal in "normal" identifier */
#define vl_pathn    chrBIT(vl_PATHN)	/* is typically legal in a file's pathname */
#define vl_wild     chrBIT(vl_WILD)	/* is typically a shell wildcard char */
#define vl_linespec chrBIT(vl_LINESPEC)	/* ex-style line range: 1,$ or 13,15 or % etc.*/
#define vl_fence    chrBIT(vl_FENCE)	/* a fence, i.e. (, ), [, ], {, } */
#define vl_nonspace chrBIT(vl_NONSPACE)	/* non-whitespace */
#define vl_qident   chrBIT(vl_QIDENT)	/* is typically legal in "qualified" identifier */

#if OPT_WIDE_CTYPES
#define vl_scrtch   chrBIT(vl_SCRTCH)	/* legal in scratch-buffer names */
#define vl_shpipe   chrBIT(vl_SHPIPE)	/* legal in shell/pipe-buffer names */
#define vl_xdigit   chrBIT(vl_XDIGIT)	/* hex digit */
#define isXDigit(c)	istype(vl_xdigit, c)

typedef	unsigned long CHARTYPE;
#else
typedef USHORT CHARTYPE;
#endif

/* these parallel the ctypes.h definitions, except that
	they force the char to valid range first */
#define vlCTYPE(c)	vl_chartypes_[CharOf(c)]
#define istype(m,c)	((vlCTYPE(c) & (m)) != 0)

#define isAlnum(c)	istype(vl_lower|vl_upper|vl_digit, c)
#define isAlpha(c)	istype(vl_lower|vl_upper, c)
#define isCntrl(c)	istype(vl_cntrl, c)
#define isDigit(c)	istype(vl_digit, c)
#define isLower(c)	istype(vl_lower, c)
#define isPrint(c)	istype(vl_print, c)
#define isPunct(c)	istype(vl_punct, c)
#define isSpace(c)	istype(vl_space, c)
#define isUpper(c)	istype(vl_upper, c)

#define isbackspace(c)	(istype(vl_bspace, c) || (c) == backspc)
#define isfence(c)	istype(vl_fence, c)
#define isident(c)	istype(vl_ident, c)
#define isqident(c)	istype(vl_qident, c)
#define islinespecchar(c)	istype(vl_linespec, c)
#define ispath(c)	istype(vl_pathn, c)
#define iswild(c)	istype(vl_wild, c)

/* macro for whitespace (non-return) */
#define	isBlank(c)      ((c == '\t') || (c == ' '))

#define	isGraph(c)	(!isSpecial(c) && !isSpace(c) && isPrint(c))

/* DIFCASE represents the difference between upper
   and lower case letters, DIFCNTRL the difference between upper case and
   control characters.	They are xor-able values.  */
#define	DIFCASE		0x20
#define	DIFCNTRL	0x40
#define toUpper(c)	vl_uppercase[CharOf(c)]
#define toLower(c)	vl_lowercase[CharOf(c)]
#define tocntrl(c)	(((unsigned)(c))^DIFCNTRL)
#define toalpha(c)	(((unsigned)(c))^DIFCNTRL)

#else

# include <ctype.h>

# define isAlnum(c)	isalnum(c)
# define isAlpha(c)	isalpha(c)
# define isCntrl(c)	iscntrl(c)
# define isDigit(c)	isdigit(c)
# define isLower(c)	islower(c)
# define isGraph(c)	isgraph(c)
# define isPrint(c)	isprint(c)
# define isPunct(c)	ispunct(c)
# define isSpace(c)	isspace(c)
# define isUpper(c)	isupper(c)
# define toUpper(c)	toupper(c)
# define toLower(c)	tolower(c)
# define isident(c)     isalnum(c)
# define isXDigit(c)	isxdigit(c)

#endif

/* macro for cases where return & newline are equivalent */
#define	isreturn(c)	((c == '\r') || (c == '\n'))

#define nocase_eq(bc,pc) (CharOf(bc) == CharOf(pc) || (toUpper(bc) == toUpper(pc)))

#endif /* VL_CTYPE_H_incl */
