/*
 * $Id: regexp.c,v 1.229 2025/01/26 17:08:26 tom Exp $
 *
 * Copyright 2005-2022,2025 Thomas E. Dickey and Paul G. Fox
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
 *
 * ----------------------------------------------------------------------------
 *
 *	This code has been MODIFIED for use in vile (see the original
 *	copyright information down below) -- in particular:
 *	 - regexec no longer needs to scan a null terminated string
 *	 - regexec takes extra arguments describing the first and
 *	 	just-past-last legal scan start offsets, to limit matches
 *		to beginning in that range, as well as to denote the state
 *		of being at the beginning of the line.
 *	 - inexact character matches are now handled, if the global ignorecase
 *	 	is set
 *	 - regexps are now relocatable, rather than locked down
 *
 *		pgf, 11/91
 *
 *	  - allow repeat counts in \{braces\}
 *	  - implement character-classes.
 *
 *		ted, 01/01
 *
 *	  - modify to support patterns with embedded nulls
 *
 *		ted, 02/03
 *
 *	  - add test-driver
 *
 *		ted, 05/03
 */

/*
 * regcomp and regexec -- regsub and regerror are elsewhere
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 */

#ifdef DEBUG_REGEXP
#define UNBUNDLED_VILE_REGEX
#endif

#ifdef UNBUNDLED_VILE_REGEX

#ifdef TEST_MULTIBYTE_REGEX

#include <estruct.h>

#define realdef			/* after estruct.h to omit mode stuff... */
#include <edef.h>

#else /* !OPT_MULTIBYTE */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#define UINT unsigned int
#define ULONG unsigned long
#endif

typedef ULONG B_COUNT;		/* byte-count */

#define OPT_VILE_CTYPE 0	/* use macros but not data */
#define OPT_MULTIBYTE 0

#include <vl_regex.h>
#include <vl_alloc.h>
#include <vl_ctype.h>

#define vl_index (strchr)
#define strmalloc(s) strdup(s)

#endif /* OPT_MULTIBYTE */

#else

#include <estruct.h>

#endif /* UNBUNDLED_VILE_REGEX */

#if OPT_VILE_CTYPE
#include <edef.h>		/* use global data from vile */
#endif

#if !defined(OPT_WORKING) || defined(DEBUG_REGEXP)
# undef  beginDisplay
# define beginDisplay()		/* nothing */
# undef  endofDisplay
# define endofDisplay()		/* nothing */
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef inline
#ifdef NO_INLINE
#define inline			/* nothing */
#endif
#endif

#ifndef NO_LEAKS
#define NO_LEAKS 0
#endif

#ifndef OPT_TRACE
#define OPT_TRACE 0
#endif

#ifndef TRACE
#define TRACE(p)		/* nothing */
#endif

#ifndef T_CALLED
#define T_CALLED ""
#endif

#ifndef returnCode
#define returnCode(n) return n
#endif

#if OPT_TRACE
/* #define REGDEBUG  1 */
#endif

#ifdef REGDEBUG
#define REGTRACE(p) TRACE(p)
#define returnReg(n) returnCode(n)
#else
#define REGTRACE(p)		/* nothing */
#define returnReg(n) return n
#endif

#undef PLUS			/* vile conflict */
#undef min
#undef max

static char *reg(int paren, int *flagp);
static char *regatom(int *flagp, int at_bop);
static char *regbranch(int *flagp);
static char *regnode(int op);
static char *regpiece(int *flagp, int at_bop);
static inline char *regnext(char *p);
static int regmatch(char *prog, int plevel, int ic);
static int regrepeat(const char *p, int ic);
static int regtry(regexp * prog, char *string, char *stringend, int plevel, int ic);
static void regc(int b);
static void regninsert(int n, char *opnd);
static void regopinsert(int op, char *opnd);
static void regoptail(char *p, char *val);
static void regrange(int op, char *opnd, int lo, int hi);
static void regtail(char *p, char *val);
static void set_opsize(void);

#ifdef REGDEBUG
static int regnarrate = 1;
static char *reg_program;
static char *regdump1(char *base, char *op);
static void regdump(regexp * r);
static const char *regprop(char *op);
#endif

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart	char that must begin a match; -1 if none obvious
 * reganch	is the match anchored (at beginning-of-line only)?
 * regmust	string (starting at program[offset]) that match must
 *				include, or NULL
 * regmlen	length of regmust string
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that regcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in regexec() needs it and regcomp() is computing
 * it anyway.
 */

/*
 * Structure for regexp "program".  This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.  (Here we
 * have one of the subtle syntax dependencies:  an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:  the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

/* definition		opnd?	meaning */
typedef enum {
    END = 0			/* no   End of program. */
    ,BOL			/* no   Match "" at beginning of line. */
    ,EOL			/* no   Match "" at end of line. */
    ,ANY			/* no   Match any one character. */
    ,ANYOF			/* str  Match any character in this string. */
    ,ANYBUT			/* str  Match any character not in this string. */
    ,BRANCH			/* node Match this alternative, or the next... */
    ,BACK			/* no   Match "", "next" ptr points backward. */
    ,EXACTLY			/* str  Match this string. */
    ,NOTHING			/* no   Match empty string. */
    ,STAR			/* node Match this (simple) thing 0 or more times. */
    ,PLUS			/* node Match this (simple) thing 1 or more times. */
    ,RSIMPLE			/* node Match this (simple) thing min..max times */
    ,RCOMPLX			/* node Match this (complex) thing min..max times */

    ,BEGWORD			/* node Match "" between nonword and word. */
    ,ENDWORD			/* node Match "" between word and nonword. */

    ,ALNUM			/* node Match any alphanumeric */
    ,ALPHA			/* node Match any alphabetic */
    ,BLANK			/* node Match any space char */
    ,CNTRL			/* node Match any control char */
    ,DIGIT			/* node Match any digit */
    ,GRAPH			/* node Match any graphic char (no whitesp) */
    ,IDENT			/* node Match any alphanumeric, include _ */
    ,LOWER			/* node Match any lowercase char */
    ,OCTAL			/* node Match any octal digit */
    ,PATHN			/* node Match any file pathname char */
    ,PRINT			/* node Match any printable char (including whitesp) */
    ,PUNCT			/* node Match any punctuation char */
    ,SPACE			/* node Match single whitespace, excluding BOL and EOL */
    ,UPPER			/* node Match any uppercase char */
    ,XDIGIT			/* node Match any hex digit */

    ,NALNUM			/* node Match any non-alphanumeric char */
    ,NALPHA			/* node Match any alphabetic */
    ,NBLANK			/* node Match any non-space char */
    ,NCNTRL			/* node Match any non-control char */
    ,NDIGIT			/* node Match any non-digit */
    ,NGRAPH			/* node Match any non-graphic char */
    ,NIDENT			/* node Match any non-identifier char */
    ,NLOWER			/* node Match any non-lowercase char */
    ,NOCTAL			/* node Match any non-octal digit */
    ,NPATHN			/* node Match any non-file pathname char */
    ,NPRINT			/* node Match any non-printable char */
    ,NPUNCT			/* node Match any non-punctuation char  */
    ,NSPACE			/* node Match single nonwhitespace, excluding BOL and EOL */
    ,NUPPER			/* node Match any non-uppercase char */
    ,NXDIGIT			/* node Match any non-hex digit */

    ,NEVER			/* no   No match */
    ,OPEN			/* no   Mark this point in input as start of #n. */
    ,OPEN1, OPEN2, OPEN3, OPEN4, OPEN5, OPEN6, OPEN7, OPEN8, OPEN9
    ,CLOSE			/* no   Analogous to OPEN. */
    ,CLOSE1, CLOSE2, CLOSE3, CLOSE4, CLOSE5, CLOSE6, CLOSE7, CLOSE8, CLOSE9
} REGEXP_OP;

#define is_OPENn(n) ((n) >= OPEN && (n) <= OPEN9)

#define OPENn  OPEN1: \
	  case OPEN2: \
	  case OPEN3: \
	  case OPEN4: \
	  case OPEN5: \
	  case OPEN6: \
	  case OPEN7: \
	  case OPEN8: \
	  case OPEN9

#define is_CLOSEn(n) ((n) >= CLOSE && (n) <= CLOSE9)

#define CLOSEn CLOSE1: \
	  case CLOSE2: \
	  case CLOSE3: \
	  case CLOSE4: \
	  case CLOSE5: \
	  case CLOSE6: \
	  case CLOSE7: \
	  case CLOSE8: \
	  case CLOSE9

/*
 * Characters used in escapes to correspond with character classes
 */
typedef enum {
    CHR_ALNUM = 'i',
    CHR_ALPHA = 'a',
    CHR_BLANK = 'b',
    CHR_CNTRL = 'c',
    CHR_DIGIT = 'd',
    CHR_GRAPH = 'g',
    CHR_IDENT = 'w',
    CHR_LOWER = 'l',
    CHR_OCTAL = 'o',
    CHR_PATHN = 'f',
    CHR_PRINT = 'p',
    CHR_PUNCT = 'q',
    CHR_SPACE = 's',
    CHR_UPPER = 'u',
    CHR_XDIGIT = 'x',

    CHR_NALNUM = 'I',
    CHR_NALPHA = 'A',
    CHR_NBLANK = 'B',
    CHR_NCNTRL = 'C',
    CHR_NDIGIT = 'D',
    CHR_NGRAPH = 'G',
    CHR_NIDENT = 'W',
    CHR_NLOWER = 'L',
    CHR_NOCTAL = 'O',
    CHR_NPATHN = 'F',
    CHR_NPRINT = 'P',
    CHR_NPUNCT = 'Q',
    CHR_NSPACE = 'S',
    CHR_NUPPER = 'U',
    CHR_NXDIGIT = 'X'
} REGEXP_CHR;

#define chr_CLASS(name) CHR_ ## name

#define expand_case_CLASSES() \
	case_CLASSES(ALNUM, NALNUM); \
	case_CLASSES(ALPHA, NALPHA); \
	case_CLASSES(BLANK, NBLANK); \
	case_CLASSES(CNTRL, NCNTRL); \
	case_CLASSES(DIGIT, NDIGIT); \
	case_CLASSES(GRAPH, NGRAPH); \
	case_CLASSES(IDENT, NIDENT); \
	case_CLASSES(LOWER, NLOWER); \
	case_CLASSES(OCTAL, NOCTAL); \
	case_CLASSES(PATHN, NPATHN); \
	case_CLASSES(PRINT, NPRINT); \
	case_CLASSES(PUNCT, NPUNCT); \
	case_CLASSES(SPACE, NSPACE); \
	case_CLASSES(UPPER, NUPPER); \
	case_CLASSES(XDIGIT, NXDIGIT)

/*
 * Macros to ensure consistent use of character classes:
 */
#define is_ALPHA(c) isAlpha(c)
#define is_ALNUM(c) isAlnum(c)
#define is_BLANK(c) (isSpace(c) && !isreturn(c))
#define is_CNTRL(c) isCntrl(c)
#define is_DIGIT(c) isDigit(c)
#define is_GRAPH(c) isGraph(c)
#define is_IDENT(c) isident(c)
#define is_LOWER(c) isLower(c)
#define is_OCTAL(c) ((c) >= '0' && (c) <= '7')
#define is_PATHN(c) ispath(c)
#define is_PRINT(c) (isPrint(c) || (isSpace(c) && !isCntrl(c)))
#define is_PUNCT(c) isPunct(c)
#define is_SPACE(c) isSpace(c)
#define is_UPPER(c) isUpper(c)

#ifdef isXDigit
#define is_XDIGIT(c) isXDigit(c)
#else
#define is_XDIGIT(c) (isDigit(c) || (isLower(c) && (c) - 'a' < 6) || (isUpper(c) && (c) - 'A' < 6))
#endif

/*
 * Multibyte character classes are checked on the UTF-8 strings.  We could
 * alternatively convert the data to be scanned into an array of integers
 * holding Unicode codes (which might be faster).
 */
#if OPT_MULTIBYTE
#define is_CLASS(name,ptr) reg_ctype_ ## name(ptr)

#if USE_WIDE_CTYPE
#define sys_CTYPE_SIZE	((unsigned) 0xffff)
#else
#define sys_CTYPE_SIZE	((unsigned) 0xff)
#endif

#else
#define sys_CTYPE_SIZE	((unsigned) 0xff)
#define is_CLASS(name,ptr) is_ ## name(*(ptr))
#endif

/*
 * Opcode notes:
 *
 * BRANCH	The set of branches constituting a single choice are hooked
 *		together with their "next" pointers, since precedence prevents
 *		anything being concatenated to any individual branch.  The
 *		"next" pointer of the last BRANCH in a choice points to the
 *		thing following the whole choice.  This is also where the
 *		final "next" pointer of each individual branch points; each
 *		branch starts with the operand node of a BRANCH node.
 *
 * BACK		Normal "next" pointers all implicitly point forward; BACK
 *		exists to make loop structures possible.
 *
 * STAR,PLUS	'?', and complex '*' and '+', are implemented as circular
 *		BRANCH structures using BACK.  Simple cases (one character
 *		per match) are implemented with STAR and PLUS for speed
 *		and to minimize recursive plunges.
 *
 * OPEN,CLOSE	...are numbered at compile time.
 */

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 *
 * We added OPSIZE() to store the length of an ANYOF, ANYBUT or EXACTLY
 * OPERAND() so we can allow it to contain embedded nulls.  We cannot use
 * NEXT() for this purpose, since it may contain an offset past additional
 * operations following the current one.
 */

#define OP_HDR		5	/* header chars in normal operation */

#define	OP(p)		((p)[0])
#define	NEXT(p)		((unsigned)((CharOf((p)[1]) << 8) + CharOf((p)[2])))
#define	OPSIZE(p)	((unsigned)((CharOf((p)[3]) << 8) + CharOf((p)[4])))
#define	OPSIZE2(p)	(((size_t)(CharOf((p)[3])) << 8) + ((size_t)CharOf((p)[4])))
#define	OPERAND(p)	((p) + OP_HDR)

#define HI_BYTE(n)	(char)((n) >> 8)
#define LO_BYTE(n)	(char)(n)

#define SET_NEXT(p, n)	(p)[1] = HI_BYTE(n),\
 			(p)[2] = LO_BYTE(n)

#define SET_SIZE(p, n)	(p)[3] = HI_BYTE(n),\
 			(p)[4] = LO_BYTE(n)

/*
 * RSIMPLE/RCOMPLX operations have a different layout:  OP(), NEXT(), and
 * RR_MIN(), RR_MAX() are used to extract their data, i.e., the minimum and
 * maximum repeat counts, respectively.  A zero repeat-count means that the
 * value is unspecified.
 */
#define RR_BYTES  sizeof(int)
#define RR_LEN		(3 + (2 * RR_BYTES))	/* sizeof(RSIMPLE/RCOMPLX) */
#define RR_MIN(p) ((p)[3])
#define RR_MAX(p) ((p)[3 + RR_BYTES])

#define SAME(a,b,ic) (ic ? nocase_eq(a,b) : (CharOf(a) == CharOf(b)))
#define STRSKIP(s) ((s) + strlen(s))

/*
 * See regmagic.h for one further detail of program structure.
 */

/*
 * Utility definitions.
 */

#define L_CURL '{'
#define R_CURL '}'

#define	ISMULT(c)	((c) == '*' || (c) == '+' || (c) == '?' || (c) == L_CURL)
#define	META		"^$.[()|?+*\\<>{}"

/*
 * see REGEXP_CHR - this lists all of its values.
 */
#define ANY_ESC "aAbBcCdDfFgGiIlLoOpPqQsSuUwWxX"

/*
 * Flags to be passed up and down.
 */
#define	HASWIDTH	01	/* Known never to match null string. */
#define	SIMPLE		02	/* Simple enough to be STAR/PLUS operand. */
#define	SPSTART		04	/* Starts with * or +. */
#define	WORST		0	/* Worst case. */

/*
 * Global work variables for regcomp().
 */
static char *regparse;		/* Input-scan pointer. */
static char *reglimit;		/* end of input-scan pointer */
static int regnpar;		/* () count. */
static char regdummy;
static char *regcode;		/* Code-emit pointer; &regdummy = don't. */
static long regsize;		/* Code size. */

static char *op_pointer;	/* cached from regnode() */
static int op_length;		/* ...corresponding operand-length */

/*
 * Global work variables for regexec().
 */
static char *reginput;		/* String-input pointer. */
static char *regnomore;		/* String-input end pointer. */
static char *regbol;		/* Beginning of input, for ^ check. */
static char **regstartp;	/* Pointer to startp array. */
static char **regendp;		/* Ditto for endp. */
static int regok_bol;		/* "^" check is legal */
static int reg_cnts[NSUBEXP];	/* count closure of \(...\) groups */

#if OPT_MULTIBYTE
static int reg_utf8flag = 0;
#endif

#if OPT_MULTIBYTE
static void
set_utf8flag(BUFFER *bp)
{
#ifdef DEBUG_REGEXP
    (void) bp;			/* test-driver sets flag once, in main program */
#else
    ENC_CHOICES encoding = (bp
			    ? b_val(bp, VAL_FILE_ENCODING)
			    : global_b_val(VAL_FILE_ENCODING));
    if (encoding == enc_LOCALE)
	encoding = vl_encoding;
    reg_utf8flag = (encoding >= enc_UTF8);
#endif
}

#define doUTF8(value) (reg_utf8flag && ((value) >= 128))

static int
reg_bytes_at(const char *source, const char *last)
{
    int result = vl_conv_to_utf32((UINT *) 0, source, (B_COUNT) (last - source));
    if (result <= 0)
	result = 1;
    return result;
}
#define BYTES_AT(source, last) (doUTF8(UCHAR_AT(source)) ? reg_bytes_at(source, last) : 1)

static int
reg_char_at(const char *source, const char *last)
{
    int result = UCHAR_AT(source);
    UINT target;

    if (vl_conv_to_utf32(&target, source, (B_COUNT) (last - source)) > 0)
	result = (int) target;
    return result;
}
#define WCHAR_AT(source, last) (doUTF8(UCHAR_AT(source)) ? reg_char_at(source, last) : UCHAR_AT(source))

static int
reg_bytes_before(const char *source, const char *first)
{
    B_COUNT before;
    B_COUNT limit = (B_COUNT) (source - first);
    int result = 0;
    int check;

    for (before = 1; before <= limit; ++before) {
	check = vl_conv_to_utf32((UINT *) 0, source - before, before);
	if (check == (int) before) {
	    result = (int) before;
	    break;
	}
    }

    return result;
}
#define BYTES_BEFORE(source, first) (doUTF8(UCHAR_AT(source - 1)) ? reg_bytes_before(source, first) : 1)

#if 0
static int
reg_char_before(const char *source, const char *first)
{
    B_COUNT before;
    B_COUNT limit = (B_COUNT) (source - first);
    int result = 0;
    int check;
    UINT target;

    for (before = 1; before < limit; ++before) {
	check = vl_conv_to_utf32((UINT *) 0, source - before, before);
	if (check == (int) before) {
	    if (vl_conv_to_utf32(&target, source, before) == (int) before)
		result = (int) target;
	    break;
	}
    }

    return result;
}
#endif
#define WCHAR_BEFORE(source, first) (doUTF8(UCHAR_AT(source-1)) ? reg_char_before(source, first) : UCHAR_AT(source-1))

/*
 * Put a whole multibyte character in the output if we're in UTF-8 mode.
 */
static void
put_reg_char(int ch)
{
    UCHAR target[MAX_UTF8];
    int len = vl_conv_to_utf8(target, (UINT) ch, sizeof(target));
    int n;

    for (n = 0; n < len; ++n) {
	regc(target[n]);
    }
}
#define PUT_REGC(ch) (reg_utf8flag ? put_reg_char(ch) : regc(ch))

/*
 * The "ctype" functions all use a pointer, so we can do the conversion from
 * UTF-8 directly.  vile's feature for overriding ctype only applies to an
 * 8-bit table; outside of that, we'll use the system's ctype functions.
 */
static int
use_system_ctype(UINT * target, const char *source)
{
    int rc = FALSE;

    *target = 0;
    if (reg_utf8flag
	&& (vl_conv_to_utf32(target,
			     source,
			     (B_COUNT) (regnomore - source)) > 0)
	&& (*target <= sys_CTYPE_SIZE)
	&& (vl_mb_to_utf8((int) *target) == NULL)) {
	rc = TRUE;
    }
    return rc;
}

#define vl_toupper(ch) \
    ((reg_utf8flag \
	&& ((ch) <= (int) sys_CTYPE_SIZE) \
	&& (vl_mb_to_utf8(ch) == NULL)) \
	? (int) sys_toupper(ch) \
	: (int) toUpper(ch))

/*
 * Check if 'p' (from pattern) and 'q' (from actual data) are the
 * "same", ignoring case if 'ic' is true.
 */
static inline int
same_char(int p, int q, int ic)
{
    int rc;

    if (reg_utf8flag) {
	/* both parameters are Unicode */
	if (ic) {
	    rc = (vl_toupper(p) == vl_toupper(q));
	} else {
	    rc = (p == q);
	}
    } else {
	rc = SAME(p, q, ic);
    }
    return rc;
}
#define EQ_CHARS(p,q,ic) (reg_utf8flag ? same_char(p, q, ic) : SAME(p,q,ic))

/*
 * Evaluate a character-class expression, using vile's ctype (if possible),
 * or the system's ctype if not.
 */
#define reg_CTYPE(name,expr) \
static int reg_ctype_##name(const char *source) \
{ \
    int result = -1; \
    UINT target; \
    if (use_system_ctype(&target, source)) { \
	result = expr; \
	REGTRACE(("reg_ctype_" #name ": %#X ->%d\n", target, result)); \
    } \
    if (result < 0) { \
	if (target == 0) \
	    target = CharOf(*source); \
	result = is_##name(target); \
	REGTRACE(("reg_ctype_" #name ": %#x ->%d\n", target, result)); \
    } \
    return result; \
}

reg_CTYPE(ALPHA, sys_isalpha((sys_WINT_T) target))
reg_CTYPE(ALNUM, sys_isalnum((sys_WINT_T) target))
reg_CTYPE(BLANK, sys_isblank((sys_WINT_T) target))
reg_CTYPE(CNTRL, sys_iscntrl((sys_WINT_T) target))
reg_CTYPE(DIGIT, sys_isdigit((sys_WINT_T) target))
reg_CTYPE(GRAPH, sys_isgraph((sys_WINT_T) target))
reg_CTYPE(IDENT, sys_isalnum((sys_WINT_T) target))
reg_CTYPE(LOWER, (sys_isalpha((sys_WINT_T) target) &&
		  sys_islower((sys_WINT_T) target)))
reg_CTYPE(OCTAL, (target >= '0') && (target <= '7'))
reg_CTYPE(PATHN, (((target < 128) && ispath(target)) ||
		  ((target >= 128) && sys_isalnum((sys_WINT_T) target))))
reg_CTYPE(PRINT, sys_isprint((sys_WINT_T) target))
reg_CTYPE(PUNCT, sys_ispunct((sys_WINT_T) target))
reg_CTYPE(SPACE, sys_isspace((sys_WINT_T) target))
reg_CTYPE(UPPER, (sys_isalpha((sys_WINT_T) target) &&
		  sys_isupper((sys_WINT_T) target)))
reg_CTYPE(XDIGIT, sys_isxdigit((sys_WINT_T) target))
#else
#define BYTES_AT(source, last) 1
#define BYTES_BEFORE(source, first) 1
#define WCHAR_AT(source, last) UCHAR_AT(source)
#define WCHAR_BEFORE(source, first) UCHAR_AT(source[-1])
#define PUT_REGC(c) regc(c)
#define EQ_CHARS(p,q,ic) SAME(p,q,ic)
#define set_utf8flag(bp)	/* nothing */
#endif
/*
 *				regexp		in magic	in nomagic
 *				char		enter as	enter as
 *				-------		--------	--------
 *	0 or 1			?		\?		\?
 *	1 or more		+		\+		\+
 *	0 or more		*		*		\*
 *	beg. nest		(		\(		\(
 *	end nest		)		\(		\)
 *	beg chr class		[		[		\[
 *	beg "word"		<		\<		\<
 *	end "word"		>		\>		\>
 *	beg "limit"		{		\{		\{
 *	end "limit"		}		\}		\}
 *	beginning		^		^		^
 *	end			$		$		$
 *	any char		.		.		\.
 *	alternation		|		\|		\|
 *	flip or literal		\		\\		\\
 *	last replace		~		~		\~
 *	words			\w		\w		\w
 *	spaces			\s		\s		\s
 *	digits			\d		\d		\d
 *	printable		\p		\p		\p
 *
 *	So:  in magic mode, we remove \ from ? + ( ) < > { } |
 *			   and add \ to bare ? + ( ) < > { } |
 *	   in nomagic mode, we remove \ from ? + ( ) < > { } | * [ ] . ~
 *			   and add \ to bare ? + ( ) < > { } | * [ ] . ~
 */

#define MAGICMETA   "?+()<>{}|"
#define NOMAGICMETA "?+()<>{}|*[] .~"

static int
regmassage(const char *in_text,
	   size_t in_size,
	   char *out_text,
	   size_t *out_size,
	   size_t *out_uppercase,
	   int magic)
{
    const char *metas = ((magic > 0)
			 ? MAGICMETA
			 : (magic < 0)
			 ? "<>"
			 : NOMAGICMETA);
    char *nxt = out_text;
    size_t n;
    size_t uc = 0;

    for (n = 0; n < in_size; ++n) {
	/* count uppercase chars as a side-effect for smartcase */
	if (is_CLASS(UPPER, in_text + n))
	    uc++;
	if (in_text[n] == BACKSLASH) {	/* remove \ from these metas */
	    if ((n + 1) >= in_size) {
#ifdef FAIL_TRAILING_BS
		mlforce("trailing backslash");
		return FALSE;
#else
		*nxt++ = BACKSLASH;
		break;
#endif
	    }
	    if (in_text[n + 1] != EOS && vl_index(metas, in_text[n + 1])) {
		++n;
	    } else if (in_text[n + 1] == BACKSLASH) {
		*nxt++ = in_text[++n];	/* the escape */
	    }
	} else if (in_text[n] != EOS && vl_index(metas, in_text[n])) {	/* add \ to these metas */
	    *nxt++ = BACKSLASH;
	}
	*nxt++ = in_text[n];	/* the char */
    }
    *nxt = EOS;
    *out_size = (size_t) (nxt - out_text);
    *out_uppercase = uc;
    return TRUE;
}

/*
 - regcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.)
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.
 */

regexp *
regcomp(const char *exp_text, size_t exp_len, int magic)
{
    regexp *r;
    char *scan;
    char *longest;
    size_t len;
    size_t parsed_len;
    size_t uppercase = 0;
    int flags;
    static char *exp;
    static size_t explen;

    set_utf8flag(curbp);

    if (exp_text == NULL) {
	regerror("NULL argument");
	return NULL;
    }

    TRACE(("regcomp(\"%s\", %smagic)\n",
	   visible_buff(exp_text, (int) exp_len, 0),
	   magic ? "" : "no"));

    len = exp_len + 1;
    if (explen < 2 * len + 20) {

	beginDisplay();
	if (exp)
	    free(exp);
	exp = castalloc(char, 2 * len + 20);
	endofDisplay();

	if (exp == NULL) {
	    regerror("couldn't allocate exp copy");
	    return NULL;
	}
	explen = 2 * len + 20;
    }

    if (!regmassage(exp_text, exp_len, exp, &parsed_len, &uppercase, magic))
	return NULL;
    TRACE(("after regmassage: '%s'\n", visible_buff(exp, (int) strlen(exp), 0)));

    /* First pass: determine size, legality. */
    REGTRACE(("First pass: determine size, legality.\n"));
    regparse = exp;
    reglimit = exp + parsed_len;
    regnpar = 1;
    regsize = 0;
    regcode = &regdummy;
    regc(REGEXP_MAGIC);
    if (reg(0, &flags) == NULL)
	return (NULL);

    /* Small enough for pointer-storage convention? */
    if (regsize >= 32767) {	/* Probably could be 65535. */
	regerror("regexp too big");
	return NULL;
    }

    /* Allocate space. */
    beginDisplay();
    r = typeallocplus(regexp, (size_t) regsize);
    endofDisplay();
    if (r == NULL) {
	regerror("out of space");
	return NULL;
    }

    /* how big is it?  (vile addition) */
    r->size = sizeof(regexp) + (size_t) regsize;
    r->uppercase = uppercase;

    /* Second pass: emit code. */
    REGTRACE(("Second pass: emit code\n"));
    regparse = exp;
    reglimit = exp + parsed_len;
    regnpar = 1;
    regcode = r->program;
    regc(REGEXP_MAGIC);
    if (reg(0, &flags) == NULL) {
	free(r);
	return (NULL);
    }

    /* Dig out information for optimizations. */
    r->regstart = -1;		/* Worst-case defaults. */
    r->reganch = 0;
    r->regmust = -1;
    r->regmlen = 0;
    scan = r->program + 1;	/* First BRANCH. */
    if (OP(regnext(scan)) == END) {	/* Only one top-level choice. */
	scan = OPERAND(scan);

	/* Starting-point info. */
	if (OP(scan) == EXACTLY) {
	    r->regstart = WCHAR_AT(OPERAND(scan), regnext(scan));
	} else if (OP(scan) == BEGWORD && OP(regnext(scan)) == EXACTLY) {
	    r->regstart = WCHAR_AT(OPERAND(regnext(scan)),
				   regnext(regnext(scan)));
	} else if (OP(scan) == BOL) {
	    r->reganch++;
	}

	/*
	 * If there's something expensive in the r.e., find the
	 * longest literal string that must appear and make it the
	 * regmust.  Resolve ties in favor of later strings, since
	 * the regstart check works with the beginning of the r.e.
	 * and avoiding duplication strengthens checking.  Not a
	 * strong reason, but sufficient in the absence of others.
	 */
	if (flags & SPSTART) {
	    longest = NULL;
	    len = 0;
	    for (; scan != NULL; scan = regnext(scan))
		if (OP(scan) == EXACTLY && OPSIZE2(scan) >= len) {
		    longest = OPERAND(scan);
		    len = OPSIZE2(scan);
		}
	    if (longest) {
		r->regmust = (int) (longest - r->program);
		r->regmlen = len;
	    }
	}
    }
#if NO_LEAKS
    if (exp != NULL) {
	beginDisplay();
	free(exp);
	exp = NULL;
	explen = 0;
	endofDisplay();
    }
#endif
#ifdef REGDEBUG
    regdump(r);
#endif
    return (r);
}

/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
static char *
reg(int paren,			/* Parenthesized? */
    int *flagp)
{
    char *ret;
    char *br;
    char *ender;
    int parno;
    int flags;

    *flagp = HASWIDTH;		/* Tentatively. */

    /* Make an OPEN node, if parenthesized. */
    if (paren) {
	if (regnpar >= NSUBEXP) {
	    regerror("too many ()");
	    return NULL;
	}
	parno = regnpar;
	regnpar++;
	ret = regnode(OPEN + parno);
    } else {
	ret = NULL;
	parno = 0;
    }

    /* Pick up the branches, linking them together. */
    br = regbranch(&flags);
    if (br == NULL)
	return (NULL);
    if (ret != NULL)
	regtail(ret, br);	/* OPEN -> first. */
    else
	ret = br;
    if (!(flags & HASWIDTH))
	*flagp &= ~HASWIDTH;
    *flagp |= flags & SPSTART;
    while (*regparse == '|') {
	regparse++;
	br = regbranch(&flags);
	if (br == NULL)
	    return (NULL);
	regtail(ret, br);	/* BRANCH -> BRANCH. */
	if (!(flags & HASWIDTH))
	    *flagp &= ~HASWIDTH;
	*flagp |= flags & SPSTART;
    }

    /* Make a closing node, and hook it on the end. */
    ender = regnode((paren) ? CLOSE + parno : END);
    regtail(ret, ender);

    /* Hook the tails of the branches to the closing node. */
    for (br = ret; br != NULL; br = regnext(br))
	regoptail(br, ender);

    /* Check for proper termination. */
    if (paren && *regparse++ != ')') {
	regerror("unmatched ()");
	return NULL;
    } else if (!paren && regparse < reglimit) {
	if (*regparse == ')')
	    regerror("unmatched ()");
	else
	    regerror("Can't happen");
	return NULL;
    }

    return (ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
static char *
regbranch(int *flagp)
{
    char *ret;
    char *chain;
    char *latest;
    int flags;

    *flagp = WORST;		/* Tentatively. */

    ret = regnode(BRANCH);
    chain = NULL;
    while (regparse < reglimit && *regparse != '|' && *regparse != ')') {
	latest = regpiece(&flags, chain == NULL);
	if (latest == NULL)
	    return (NULL);
	if (chain && OP(chain) == EOL) {
	    regninsert(2, latest);
	    OP(chain) = EXACTLY;
	    if (chain != &regdummy) {
		SET_SIZE(chain, 1);
	    }
	    *latest++ = '$';
	    *latest++ = EOS;
	    flags |= HASWIDTH | SIMPLE;
	}
	*flagp |= flags & HASWIDTH;
	if (chain == NULL)	/* First piece. */
	    *flagp |= flags & SPSTART;
	else
	    regtail(chain, latest);
	chain = latest;
    }
    if (chain == NULL)		/* Loop ran zero times. */
	(void) regnode(NOTHING);

    return (ret);
}

static void
encode_zero_or_more(char *ret, int flags)
{
    if (flags & SIMPLE) {
	regopinsert(STAR, ret);
    } else {
	/* Emit x* as (x&|), where & means "self". */
	regopinsert(BRANCH, ret);	/* Either x */
	regoptail(ret, regnode(BACK));	/* and loop */
	regoptail(ret, ret);	/* back */
	regtail(ret, regnode(BRANCH));	/* or */
	regtail(ret, regnode(NOTHING));		/* null. */
    }
}

static void
encode_one_or_more(char *ret, int flags)
{
    char *next;

    if (flags & SIMPLE) {
	regopinsert(PLUS, ret);
    } else {
	/* Emit x+ as x(&|), where & means "self". */
	next = regnode(BRANCH);	/* Either */
	regtail(ret, next);
	regtail(regnode(BACK), ret);	/* loop back */
	regtail(next, regnode(BRANCH));		/* or */
	regtail(ret, regnode(NOTHING));		/* null. */
    }
}

static void
encode_zero_or_one(char *ret)
{
    char *next;

    /* Emit x? as (x|) */
    regopinsert(BRANCH, ret);	/* Either x */
    regtail(ret, regnode(BRANCH));	/* or */
    next = regnode(NOTHING);	/* null. */
    regtail(ret, next);
    regoptail(ret, next);
}

/*
 * The minimum/maximum repeat counts are stored as an unaligned integer.
 */
typedef union {
    int int_value;
    char chr_value[sizeof(int)];
} UNALIGNED_INT;

#define GET_UNALIGNED(macro) \
    UNALIGNED_INT temp; \
    memcpy(temp.chr_value, &(macro(op)), sizeof(int)); \
    return temp.int_value

static int
get_RR_MIN(char *op)
{
    GET_UNALIGNED(RR_MIN);
}

static int
get_RR_MAX(char *op)
{
    GET_UNALIGNED(RR_MAX);
}

#define SET_UNALIGNED(macro) \
    UNALIGNED_INT temp; \
    temp.int_value = value; \
    memcpy(&(macro(op)), temp.chr_value, sizeof(int))

static void
set_RR_MIN(char *op, int value)
{
    SET_UNALIGNED(RR_MIN);
}

static void
set_RR_MAX(char *op, int value)
{
    SET_UNALIGNED(RR_MAX);
}

/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
static char *
regpiece(int *flagp, int at_bop)
{
    char *ret;
    char op;
    int flags;

    ret = regatom(&flags, at_bop);
    if (ret == NULL)
	return (NULL);

    op = *regparse;
    if (!ISMULT(op)) {
	*flagp = flags;
	return (ret);
    }

    if (!(flags & HASWIDTH) && op != '?') {
	regerror("*+ operand could be empty");
	return NULL;
    }
    *flagp = (op != '+') ? (WORST | SPSTART) : (WORST | HASWIDTH);

    if (op == '*') {
	encode_zero_or_more(ret, flags);
    } else if (op == '+') {
	encode_one_or_more(ret, flags);
    } else if (op == '?') {
	encode_zero_or_one(ret);
    } else if (op == L_CURL) {
	int lo = 0, hi, value = 0, comma = 0;
	while (++regparse < reglimit) {
	    if (*regparse == R_CURL)
		break;
	    if (isDigit(*regparse)) {
		value = (value * 10) + (*regparse - '0');
	    } else if (*regparse == ',') {
		if (comma++)
		    break;
		lo = value;
		value = 0;
	    } else if (*regparse != BACKSLASH) {
		break;
	    }
	}
	if (*regparse != R_CURL) {
	    regerror("unmatched {}");
	    return NULL;
	}
	if (comma)
	    hi = value;
	else
	    hi = lo = value;

	if (lo == 0 && hi == 0)
	    encode_zero_or_more(ret, flags);
	else if (lo == 1 && hi == 0)
	    encode_one_or_more(ret, flags);
	else if (lo == 0 && hi == 1)
	    encode_zero_or_one(ret);
	else if (flags & SIMPLE)
	    regrange(RSIMPLE, ret, lo, hi);
	else {
	    char *next, *loop;

	    next = regnode(BRANCH);
	    regtail(ret, next);	/* Either */
	    loop = regnode(RCOMPLX);
	    if (loop != &regdummy) {
		set_RR_MIN(loop, lo);
		set_RR_MAX(loop, hi);
	    }
	    regtail(loop, ret);	/* loop back */
	    regtail(next, regnode(BRANCH));	/* or */
	    if (lo == 0)
		regtail(ret, regnode(NOTHING));		/* null. */
	    else
		regtail(ret, regnode(NEVER));	/* not! */
	}
    }
    regparse++;
    if (ISMULT(*regparse)) {
	regerror("nested *?+");
	return NULL;
    }

    return (ret);
}

/*
 * To make this efficient, we represent the character classes internally
 * as escaped characters, which happen to be usable themselves for that
 * purpose.
 */
static int
parse_char_class(char **src)
{
    /* this matches ANY_ESC */
    /* *INDENT-OFF* */
    static struct {
	const char *name;
	char escape;
    } char_classes[] = {
	{ "[:alnum:]",	CHR_ALNUM },
	{ "[:alpha:]",	CHR_ALPHA },
	{ "[:blank:]",	CHR_BLANK },
	{ "[:cntrl:]",	CHR_CNTRL },
	{ "[:digit:]",	CHR_DIGIT },
	{ "[:file:]",	CHR_PATHN },
	{ "[:graph:]",	CHR_GRAPH },
	{ "[:ident:]",	CHR_IDENT },
	{ "[:lower:]",	CHR_LOWER },
	{ "[:octal:]",	CHR_OCTAL },
	{ "[:print:]",	CHR_PRINT },
	{ "[:punct:]",	CHR_PUNCT },
	{ "[:space:]",	CHR_SPACE },
	{ "[:upper:]",	CHR_UPPER },
	{ "[:xdigit:]", CHR_XDIGIT },
    };
    /* *INDENT-ON* */

    unsigned n;
    for (n = 0; n < sizeof(char_classes) / sizeof(char_classes[0]); n++) {
	size_t len = strlen(char_classes[n].name);
	if (!strncmp(*src, char_classes[n].name, len)) {
	    *src += len;
	    return char_classes[n].escape;
	}
    }
    return 0;
}

/*
 * We don't implement collating symbols or equivalence class expressions, which
 * have similar syntax.
 */
static int
parse_unsupported(const char *src)
{
    if (strncmp(src, "[.", (size_t) 2) == 0 && strstr(src, ".]") != NULL) {
	regerror("collating symbols are not implemented");
	return 1;
    }
    if (strncmp(src, "[=", (size_t) 2) == 0 && strstr(src, "=]") != NULL) {
	regerror("equivalence class expressions are not implemented");
	return 1;
    }
    return 0;
}

/*
 * Like strcspn(), but limited by reglimit rather than a null.
 */
static int
reg_strcspn(const char *s, const char *reject)
{
    const char *base = s;

    while (s < reglimit) {
	if (*s != EOS) {
	    if (vl_index(reject, CharOf(*s)) != NULL) {
		break;
	    }
	}
	++s;
    }
    return (int) (s - base);
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 */
static char *
regatom(int *flagp, int at_bop)
{
    char *ret;
    int flags;
    int len = 1;
    int chop = 1;
    int data;

    *flagp = WORST;		/* Tentatively. */

    REGTRACE(("REGATOM:%s\n", regparse));
    switch (*regparse++) {
    case '^':
	if (!at_bop) {
	    regparse--;
	    goto defchar;
	}
	ret = regnode(BOL);
	break;
    case '$':
	ret = regnode(EOL);
	break;
    case '<':
	ret = regnode(BEGWORD);
	break;
    case '>':
	ret = regnode(ENDWORD);
	break;
    case '.':
	ret = regnode(ANY);
	*flagp |= HASWIDTH | SIMPLE;
	break;
    case '[':{
	    int ch;
	    int classbgn = (sys_CTYPE_SIZE + 1);
	    int classend;

	    if (*regparse == '^') {	/* Complement of range. */
		ret = regnode(ANYBUT);
		regparse++;
	    } else
		ret = regnode(ANYOF);
	    if (*regparse == ']' || *regparse == '-')
		regc(*regparse++);
	    while (regparse < reglimit && *regparse != ']') {
		if (*regparse == '-') {
		    regparse++;
		    if (*regparse == ']' || regparse >= reglimit) {
			regc('-');
		    } else {
			classend = WCHAR_AT(regparse, reglimit);
			if (classbgn > classend + 1) {
			    regerror("invalid [] range");
			    return NULL;
			}
			for (; classbgn <= classend; classbgn++) {
			    if (classbgn == BACKSLASH)
				regc(classbgn);
			    PUT_REGC(classbgn);
			}
			regparse += BYTES_AT(regparse, reglimit);
		    }
		} else if (parse_unsupported(regparse)) {
		    return NULL;
		} else if ((classbgn = parse_char_class(&regparse)) != 0) {
		    regc(BACKSLASH);
		    regc(classbgn);
		} else {
		    ch = WCHAR_AT(regparse, reglimit);
		    if (ch == BACKSLASH) {
			regc(ch);
			++regparse;
			ch = WCHAR_AT(regparse, reglimit);
			if (ch == EOS
			    || (strchr(ANY_ESC, ch) == NULL
				&& isAlpha(ch))) {
			    regerror("unexpected escape in range");
			    return NULL;
			}
		    }
		    PUT_REGC(ch);
		    regparse += BYTES_AT(regparse, reglimit);
		    classbgn = ch + 1;
		}
	    }
	    set_opsize();
	    if (*regparse != ']') {
		regerror("unmatched []");
		return NULL;
	    }
	    regparse++;
	    *flagp |= HASWIDTH | SIMPLE;
	}
	break;
    case '(':
	ret = reg(1, &flags);
	if (ret == NULL)
	    return (NULL);
	*flagp |= flags & (HASWIDTH | SPSTART);
	break;
    case '|':
    case ')':			/* Supposed to be caught earlier. */
	regerror("internal urp");
	return NULL;

    case '?':			/* FALLTHROUGH */
    case '+':			/* FALLTHROUGH */
    case L_CURL:		/* FALLTHROUGH */
    case '*':
	regerror("?+* follows nothing");
	return NULL;

    case BACKSLASH:
	if (regparse >= reglimit) {
#ifdef FAIL_TRAILING_BS
	    regerror("trailing \\");
	    ret = NULL;
#else
	    /* as a special case, treat a trailing '\' char as
	     * a trailing '.'.  This makes '\' work in isearch
	     * most of the time */
	    ret = regnode(ANY);
	    *flagp |= HASWIDTH | SIMPLE;
#endif
	    return ret;
	}
#define simple_node(name) \
	    ret = regnode(name); \
	    *flagp |= HASWIDTH | SIMPLE

#define case_CLASSES(with,without) \
	    case chr_CLASS(with): \
		simple_node(with); \
		break; \
	    case chr_CLASS(without): \
		simple_node(without); \
		break

	switch (*regparse) {

	    expand_case_CLASSES();

#undef case_CLASSES

	default:
	    ret = regnode(EXACTLY);
	    regc(*regparse);
	    set_opsize();
	    *flagp |= HASWIDTH | SIMPLE;
	    break;
	}
	regparse++;
	break;
    default:{
	    char ender;

	    regparse--;
	    len = reg_strcspn(regparse, META);
	    chop = BYTES_AT(regparse, regparse + len);
	    if (len <= 0) {
		regerror("internal disaster");
		return NULL;
	    }
	    ender = *(regparse + len);
	    if (len > chop && ISMULT(ender)) {
		len--;		/* Back off clear of ?+* operand. */
	    }
	  defchar:
	    *flagp |= HASWIDTH;
	    if (len == chop)
		*flagp |= SIMPLE;
	    ret = regnode(EXACTLY);
	    while (len > 0) {
		chop = BYTES_AT(regparse, regparse + len);
		data = WCHAR_AT(regparse, regparse + len);
		PUT_REGC(data);
		regparse += chop;
		len -= chop;
	    }
	    set_opsize();
	}
	break;
    }

    return (ret);
}

/*
 - regnode - emit a node
 */
static char *			/* Location. */
regnode(int op)
{
    char *ret;
    char *ptr;
    int length = (int) ((op == RSIMPLE || op == RCOMPLX) ? RR_LEN : OP_HDR);

    ret = regcode;
    if (ret == &regdummy) {
	regsize += length;
	return (ret);
    }

    ptr = ret;
    OP(ptr) = (char) op;
    SET_NEXT(ptr, 0);		/* Null "next" pointer. */
    switch (op) {
    case RSIMPLE:
    case RCOMPLX:
	set_RR_MIN(ptr, 0);
	set_RR_MAX(ptr, 0);
	break;
    case ANYOF:
    case ANYBUT:
    case EXACTLY:
	SET_SIZE(ptr, 0);
	break;
    default:
	SET_SIZE(ptr, 0xffff);	/* just so it is known */
	break;
    }
    regcode = ptr + length;

    op_pointer = ret;
    op_length = 0;
    return (ret);
}

/*
 - set_opsize - set the operand size, if known
 */
static void
set_opsize(void)
{
    if (regcode != &regdummy) {
	SET_SIZE(op_pointer, op_length);
    }
    regc(EOS);			/* FIXME - obsolete */
}

/*
 - regc - emit (if appropriate) a byte of code
 */
static void
regc(int b)
{
    ++op_length;
    if (regcode != &regdummy) {
	*regcode++ = (char) b;
    } else {
	regsize++;
    }
}

/*
 - regrange - emit (if appropriate) a range-code
 */
static void
regrange(int op, char *opnd, int lo, int hi)
{
    regninsert((int) RR_LEN, opnd);	/* like regopinsert */
    if (regcode == &regdummy)
	return;
    *opnd = (char) op;
    set_RR_MIN(opnd, lo);
    set_RR_MAX(opnd, hi);
}

/*
 - regninsert - insert n bytes in front of already-emitted operand
 *
 * Means relocating the operand.
 */
static void
regninsert(int n, char *opnd)
{
    char *src;
    char *dst;
    char *place;

    if (regcode == &regdummy) {
	regsize += n;
	return;
    }

    src = regcode;
    regcode += n;
    dst = regcode;
    while (src > opnd)
	*--dst = *--src;

    place = opnd;		/* Op node, where operand used to be. */
    while (n--)
	*place++ = EOS;
}

/*
 - regopinsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 */
static void
regopinsert(int op, char *opnd)
{
    regninsert(OP_HDR, opnd);
    if (regcode == &regdummy)
	return;
    *opnd = (char) op;
}

/*
 - regtail - set the next-pointer at the end of a node chain
 */
static void
regtail(char *p, char *val)
{
    char *scan;
    char *temp;
    int offset;

    if (p == &regdummy)
	return;

    /* Find last node. */
    scan = p;
    for (;;) {
	temp = regnext(scan);
	if (temp == NULL)
	    break;
	scan = temp;
    }

    if (OP(scan) == BACK
	|| OP(scan) == RCOMPLX)
	offset = (int) (scan - val);
    else
	offset = (int) (val - scan);
    SET_NEXT(scan, offset);
}

/*
 - regoptail - regtail on operand of first argument; nop if operandless
 */
static void
regoptail(char *p, char *val)
{
    /* "Operandless" and "op != BRANCH" are synonymous in practice. */
    if (p == NULL || p == &regdummy || OP(p) != BRANCH)
	return;
    regtail(OPERAND(p), val);
}

/*
 * regexec and friends
 */

/* this very special copy of strncmp allows for caseless operation,
 * and also for non-null terminated strings:
 * txt_a arg ends at position end_a, or txt_a + lenb, whichever is first.
 * txt_b begins a string of at most len_b characters, against which txt_a
 *	is compared.
 *
 * txt_b holds data from the regular expression.
 * txt_a holds the data within which we're searching.
 * ic is true for when a use caseless match is required
 */
static int
regstrncmp(const char *txt_a,
	   const char *txt_b,
	   size_t len_b,
	   const char *end_a,
	   int ic)
{
    int chr_a = 0, chr_b = 0;
    int rc;
    int diff = FALSE;
    size_t have;

    if (end_a == NULL
	|| (end_a < txt_a)
	|| (unsigned) (end_a - txt_a) > len_b) {
	end_a = txt_a + len_b;
    }
    have = (size_t) (end_a - txt_a);
    while (txt_a < end_a) {
	chr_a = WCHAR_AT(txt_a, end_a);
	chr_b = WCHAR_AT(txt_b, txt_b + len_b);
	if (!EQ_CHARS(chr_a, chr_b, ic)) {
	    diff = TRUE;
	    break;
	}
	txt_a += BYTES_AT(txt_a, end_a);
	txt_b += BYTES_AT(txt_b, txt_b + len_b);
    }

    if (diff || (have < len_b)) {
	rc = (txt_a >= end_a) ? (-chr_b) : (chr_a - chr_b);
    } else {
	rc = 0;
    }
    return rc;
}

static char *
regstrchr(char *s, int c, const char *e, int ic)
{
    char *result = NULL;

    while (s < e) {
	if (EQ_CHARS(WCHAR_AT(s, e), c, ic)) {
	    result = s;
	    break;
	}
	s += BYTES_AT(s, e);
    }
    return result;
}

/*
 * Same as 'regstrchr()', except that the string has special characters
 * escaped.  The 's' argument is always null-terminated.
 */
static int
RegStrChr2(const char *s, unsigned length, const char *cs, int ic)
{
    int matched = 0;
    int compare = WCHAR_AT(cs, regnomore);
    int pattern;
    const char *last = s + length;

    while ((s < last) && !matched) {
	if (*s == BACKSLASH) {
	    ++s;
	    pattern = WCHAR_AT(s, reglimit);
	    /* this matches ANY_ESC */
	    switch (*s) {
	    default:
		matched = (pattern == compare);
		break;

#define case_CLASSES(with,without) \
	    case chr_CLASS(with): \
		matched = is_CLASS(with,cs); \
		break; \
	    case chr_CLASS(without): \
		matched = !is_CLASS(with,cs); \
		break

		expand_case_CLASSES();

#undef case_CLASSES
	    }
	} else {
	    pattern = WCHAR_AT(s, reglimit);
	    matched = EQ_CHARS(pattern, compare, ic);
	}
	s += BYTES_AT(s, regnomore);
    }
    return matched;
}

/*
 - regexec - match a regexp against a string
 	prog is the compiled expression, string is the string, stringend
	points just after the string, and the match can begin at or after
	startoff, but must end before endoff
 */
int
regexec2(regexp * prog,
	 char *string,
	 char *stringend,	/* pointer to the null, if there were one */
	 int startoff,
	 int endoff,
	 int at_bol,
	 int ic)
{
    char *s, *endsrch;
    int skip;

    /* Be paranoid... */
    if (prog == NULL || string == NULL) {
	regerror("NULL parameter");
	return (0);
    }

    /* Check validity of program. */
    if (UCHAR_AT(prog->program) != REGEXP_MAGIC) {
	regerror("corrupted program");
	return (0);
    }

    /* supply an endpoint if none given */
    if (stringend == NULL) {
	stringend = &string[strlen(string)];
    } else if (stringend < string) {
	regerror("end less than start");
	return (0);
    }

    if (endoff < 0)
	endoff = (int) (stringend - string);

    endsrch = &string[endoff];

    /* if our outer limit is the end-of-string, let us scan there,
       in case we're trying to match a lone '$' */
    if (endsrch == stringend)
	endsrch++;

    /* If there is a "must appear" string, look for it. */
    if (prog->regmust != -1) {
	char *prog_must = &(prog->program[prog->regmust]);
	int char_must = WCHAR_AT(prog_must, regnomore);
	s = &string[startoff];
	while ((s = regstrchr(s, char_must, stringend, ic))
	       != NULL && s < endsrch) {
	    if (regstrncmp(s, prog_must, prog->regmlen, stringend, ic) == 0)
		break;		/* Found it. */
	    s += BYTES_AT(s, stringend);
	}
	if (s >= endsrch || s == NULL) {	/* Not present. */
	    return (0);
	}
    }

    /* Mark beginning of line for ^ . */
    regbol = string;
    regok_bol = at_bol;

    /* Simplest case:  anchored match need be tried only once. */
    if (startoff == 0 && prog->reganch) {
	return (regtry(prog, string, stringend, 0, ic));
    }

    /* Messy cases:  unanchored match. */
    s = &string[startoff];
    if (prog->regstart >= 0) {
	/* We know what char it must start with. */
	skip = 1;
	while (skip > 0 &&
	       (s = regstrchr(s, prog->regstart, stringend, ic)) != NULL &&
	       s < endsrch) {
	    if (regtry(prog, s, stringend, 0, ic))
		return (1);
	    skip = BYTES_AT(s, stringend);
	    s += skip;
	}
    } else {
	/* We don't -- general case. */
	do {
	    if (regtry(prog, s, stringend, 0, ic))
		return (1);
	    skip = ((s < stringend)
		    ? BYTES_AT(s, stringend)
		    : 0);
	} while (skip > 0 && (s += skip) < endsrch);
    }

    /* Failure. */
    return (0);
}

int
regexec(regexp * prog,
	char *string,
	char *stringend,	/* pointer to the null, if there were one */
	int startoff,
	int endoff,
	int ic)
{
    return regexec2(prog, string, stringend, startoff, endoff, TRUE, ic);
}

/*
 - regtry - try match at specific point
 */
static int			/* 0 failure, 1 success */
regtry(regexp * prog,
       char *string,
       char *stringend,
       int plevel,
       int ic)
{
    int i;
    char **sp;
    char **ep;

    reginput = string;
    regnomore = stringend;
    regstartp = prog->startp;
    regendp = prog->endp;
#ifdef REGDEBUG
    reg_program = prog->program;
#endif
    memset(reg_cnts, 0, sizeof(reg_cnts));

    sp = prog->startp;
    ep = prog->endp;
    for (i = NSUBEXP; i > 0; i--) {
	*sp++ = NULL;
	*ep++ = NULL;
    }
    if (regmatch(prog->program + 1, plevel, ic)) {
	prog->startp[0] = string;
	prog->endp[0] = reginput;
	prog->mlen = (size_t) (reginput - string);
	return (1);
    } else {
	prog->mlen = 0;		/* not indicative of anything */
	return (0);
    }
}

/*
 */
#define decl_state() \
		char *save_input

#define save_state() \
		save_input = reginput

#define restore_state() \
		reginput = save_input

/*
 * Use this to save/restore once.
 */
#define decl_state1() \
	decl_state(); \
		int save_cnts[NSUBEXP]; \
		char *save_1stp[NSUBEXP]; \
		char *save_endp[NSUBEXP]

#define save_state1() \
	save_state(); \
		memcpy(save_1stp, regstartp, sizeof(save_1stp)); \
		memcpy(save_endp, regendp,   sizeof(save_endp)); \
		memcpy(save_cnts, reg_cnts,  sizeof(save_cnts))

#define restore_state1() \
	restore_state(); \
		memcpy(regstartp, save_1stp, sizeof(save_1stp)); \
		memcpy(regendp,   save_endp, sizeof(save_endp)); \
		memcpy(reg_cnts,  save_cnts, sizeof(save_cnts))

/*
 * Use this to save/restore several times, to find the best.
 */
#define decl_state2() \
	decl_state1(); \
		int greedy = -1; \
		int best_cnts[NSUBEXP]; \
		char *best_1stp[NSUBEXP]; \
		char *best_endp[NSUBEXP]

#define save_state2() \
	save_state1(); \
		memcpy(best_1stp, regstartp, sizeof(save_1stp)); \
		memcpy(best_endp, regendp,   sizeof(save_endp)); \
		memcpy(best_cnts, reg_cnts,  sizeof(save_cnts))

#define restore_state2() \
	restore_state1()

#define update_greedy() \
		int diff = (int) (reginput - save_input); \
		if (greedy < diff) { \
		    REGTRACE(("update_greedy:%d\n", diff)); \
		    greedy = diff; \
		    memcpy(best_1stp, regstartp, sizeof(save_1stp)); \
		    memcpy(best_endp, regendp,   sizeof(save_endp)); \
		    memcpy(best_cnts, reg_cnts,  sizeof(save_cnts)); \
		}

#define use_greediest() \
		REGTRACE(("use_greediest:%d\n", greedy)); \
		reginput = (save_input + greedy); \
		memcpy(regstartp, best_1stp, sizeof(save_1stp)); \
		memcpy(regendp,   best_endp, sizeof(save_endp)); \
		memcpy(reg_cnts,  best_cnts, sizeof(save_cnts))

/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
static int			/* 0 failure, 1 success */
regmatch(char *prog, int plevel, int ic)
{
    char *scan;			/* Current node. */
    char *next;			/* Next node. */

    REGTRACE((T_CALLED "regmatch(%d) plevel %d\n",
	      (int) (reginput - regbol), plevel));

    if ((scan = prog) != NULL) {
#ifdef REGDEBUG
	if (scan != NULL && regnarrate) {
	    if (reginput < regnomore) {
		TRACE(("%2d%s %d{%.*s}\n",
		       (int) (scan - reg_program), regprop(scan),
		       (int) (regnomore - reginput),
		       (int) (regnomore - reginput), reginput));
	    } else {
		TRACE(("%2d%s %d{}\n",
		       (int) (scan - reg_program), regprop(scan),
		       (int) (regnomore - reginput)));
	    }
	}
#endif
    }
    while (scan != NULL) {
#ifdef REGDEBUG
	if (regnarrate)
	    (void) regdump1(reg_program, scan);
#endif
	next = regnext(scan);

	switch (OP(scan)) {
	case BOL:
	    if (!regok_bol)
		returnReg(0);
	    if (reginput != regbol)
		returnReg(0);
	    break;
	case EOL:
	    if (reginput != regnomore)
		returnReg(0);
	    break;
	case BEGWORD:
	    /* Match if current char isident
	     * and previous char BOL or !ident */
	    if ((reginput >= regnomore || !is_CLASS(IDENT, reginput))
		|| (reginput != regbol
		    && is_CLASS(IDENT, reginput - BYTES_BEFORE(reginput, regbol))))
		returnReg(0);
	    break;
	case ENDWORD:
	    /* Match if previous char is ident
	     * and current char EOL or !ident */
	    if ((reginput != regnomore && is_CLASS(IDENT, reginput))
		|| reginput == regbol
		|| !is_CLASS(IDENT, reginput - BYTES_BEFORE(reginput, regbol)))
		returnReg(0);
	    break;

#define case_CLASSES(with,without) \
	case with: \
	    if (reginput >= regnomore || !is_CLASS(with,reginput)) \
		returnReg(0); \
	    reginput += BYTES_AT(reginput, regnomore); \
	    break; \
	case without: \
	    if (reginput >= regnomore || is_CLASS(with,reginput)) \
		returnReg(0); \
	    reginput += BYTES_AT(reginput, regnomore); \
	    break

	    expand_case_CLASSES();

#undef case_CLASSES

	case ANY:
	    if (reginput >= regnomore)
		returnReg(0);
	    reginput += BYTES_AT(reginput, regnomore);
	    break;
	case EXACTLY:{
		unsigned len;
		char *opnd;

		if (reginput >= regnomore)
		    returnReg(0);

		opnd = OPERAND(scan);
		/* Inline the first character, for speed. */
		if (!EQ_CHARS(WCHAR_AT(opnd, regnext(scan)),
			      WCHAR_AT(reginput, regnomore), ic)) {
		    returnReg(0);
		}
		len = OPSIZE(scan);
		if (len > 1
		    && regstrncmp(reginput,
				  opnd,
				  (size_t) len,
				  regnomore, ic) != 0)
		    returnReg(0);
		reginput += len;
	    }
	    break;
	case ANYOF:
	    if (reginput >= regnomore
		|| RegStrChr2(OPERAND(scan), OPSIZE(scan), reginput, ic) == 0)
		returnReg(0);
	    reginput += BYTES_AT(reginput, regnomore);
	    break;
	case ANYBUT:
	    if (reginput >= regnomore
		|| RegStrChr2(OPERAND(scan), OPSIZE(scan), reginput, ic) != 0)
		returnReg(0);
	    reginput += BYTES_AT(reginput, regnomore);
	    break;
	case NEVER:
	    break;
	case NOTHING:
	    break;
	case BACK:
	    break;
	case OPENn:{
		int no;
		decl_state1();

		no = OP(scan) - OPEN;
		save_state1();

		/*
		 * Don't set startp if some earlier
		 * invocation of the same parentheses
		 * already has.
		 */
		if (regstartp[no] == NULL) {
		    regstartp[no] = save_input;
		    REGTRACE(("match atom%d:\n", no));
		}
		if (regmatch(next, plevel + 1, ic)) {
		    returnReg(1);
		} else {
		    restore_state1();
		    returnReg(0);
		}
	    }
	    /* NOTREACHED */

	case CLOSEn:{
		int no;
		decl_state();

		no = OP(scan) - CLOSE;
		save_state();

		reg_cnts[plevel] += 1;
		if ((plevel + 1) < NSUBEXP)
		    reg_cnts[plevel + 1] = 0;
		if (regmatch(next, plevel - 1, ic)) {
		    /*
		     * Don't set endp if some earlier
		     * invocation of the same parentheses
		     * already has.
		     */
		    if (regendp[no] == NULL) {
			regendp[no] = save_input;
			REGTRACE(("close atom%d:%p\n", no, save_input));
		    }
		    returnReg(1);
		} else {
		    restore_state();
		    regendp[no] = NULL;
		    returnReg(0);
		}
	    }
	    /* NOTREACHED */

	case BRANCH:
	    if (OP(next) != BRANCH) {	/* No choice. */
		next = OPERAND(scan);	/* Avoid recursion. */
		break;
	    } else if (OP(OPERAND(scan)) != RCOMPLX) {
		decl_state2();

		save_state2();
		do {
		    if (regmatch(OPERAND(scan), plevel, ic)) {
			update_greedy();
		    }
		    restore_state2();
		    scan = regnext(scan);
		} while (scan != NULL && OP(scan) == BRANCH);

		if (greedy >= 0) {
		    use_greediest();
		    returnReg(1);
		}

		returnReg(0);
	    } else {
		decl_state2();
		int first = 1;
		int firstok = 0;

		save_state2();
		do {
		    int success = 0;

		    next = OPERAND(scan);
		    if (OP(next) == RCOMPLX) {
			int max = get_RR_MAX(next);
			int min = get_RR_MIN(next);

			if ((max == 0
			     || reg_cnts[plevel + 1] < max)) {
			    (void) regmatch(next, plevel, ic);
			}

			REGTRACE(("compare %d vs \\{%d,%d\\}\n",
				  reg_cnts[plevel + 1], min, max));

			success = ((min == 0
				    || reg_cnts[plevel + 1] >= min)
				   && (max == 0
				       || reg_cnts[plevel + 1] <= max));

		    } else if (is_CLOSEn(OP(next))) {
			if ((plevel + 1) < NSUBEXP)
			    reg_cnts[plevel + 1] = 0;
			success = regmatch(next, plevel, ic);
		    } else {
			success = regmatch(next, plevel, ic);
		    }
		    if (first) {
			firstok = success;
			first = 0;
		    }
		    if (success) {
			update_greedy();
		    }
		    restore_state2();
		    scan = regnext(scan);
		} while (scan != NULL && OP(scan) == BRANCH);

		if (greedy >= 0) {
		    use_greediest();
		    returnReg(firstok);
		}

		returnReg(0);
	    }

	case RCOMPLX:
	    break;

	case RSIMPLE:		/* FALLTHROUGH */
	case STAR:		/* FALLTHROUGH */
	case PLUS:
	    {
		int nxtch;
		int no;
		int min;
		int max;
		char *rpt;
#if OPT_MULTIBYTE
#define LOCAL_RPTS 100
		char *repeats[LOCAL_RPTS];
		char **rpts = NULL;
#endif
		decl_state();

		/*
		 * Lookahead to avoid useless match attempts
		 * when we know what character comes next.
		 */
		nxtch = -1;
		if (OP(next) == EXACTLY) {
		    nxtch = WCHAR_AT(OPERAND(next), regnext(next));
		}

		switch (OP(scan)) {
		case STAR:
		    min = 0;
		    max = -1;
		    rpt = OPERAND(scan);
		    break;
		case PLUS:
		    min = 1;
		    max = -1;
		    rpt = OPERAND(scan);
		    break;
		default:	/* RSIMPLE */
		    min = get_RR_MIN(scan);
		    max = get_RR_MAX(scan);
		    rpt = scan + RR_LEN;
		    break;
		}

		save_state();
		no = regrepeat(rpt, ic);

		if (max > 0 && no > max) {
		    no = max;
		    reginput = save_input + no;
		}
#if OPT_MULTIBYTE
		if (reg_utf8flag) {
		    int j;

		    if (no >= LOCAL_RPTS) {
			rpts = typeallocn(char *, (size_t) no + 1);
			if (rpts == NULL)
			    returnReg(0);
		    } else {
			rpts = repeats;
		    }
		    rpts[0] = save_input;
		    for (j = 1; j <= no; ++j) {
			int skip;
			if (rpts[j - 1] >= regnomore
			    || (skip = BYTES_AT(rpts[j - 1], regnomore)) <= 0) {
			    no = (j - 1);
			    break;
			}
			rpts[j] = rpts[j - 1] + skip;
		    }
		    reginput = rpts[no];
		}
#endif

		while (no >= min) {
		    /* If it could work, try it. */
		    if ((nxtch == -1
			 || reginput >= regnomore
			 || EQ_CHARS(WCHAR_AT(reginput, regnomore), nxtch, ic))) {
			if (regmatch(next, plevel, ic)) {
#if OPT_MULTIBYTE
			    if (rpts && (rpts != repeats))
				free(rpts);
#endif
			    returnReg(1);
			}
		    }

		    /* Couldn't or didn't -- back up. */
		    no--;
#if OPT_MULTIBYTE
		    if (reg_utf8flag) {
			if (no < 0) {
			    break;
			}
			reginput = rpts[no];
		    } else
#endif
			reginput = save_input + no;
		}
		restore_state();
#if OPT_MULTIBYTE
		if (rpts && (rpts != repeats))
		    free(rpts);
#endif
		returnReg(0);
	    }
	    /* NOTREACHED */

	case END:
	    returnReg(1);	/* Success! */
	default:
	    regerror("memory corruption");
	    returnReg(0);
	}

	scan = next;
    }

    /*
     * We get here only if there's trouble -- normally "case END" is
     * the terminating point.
     */
    regerror("corrupted pointers");
    returnReg(0);
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
static int
regrepeat(const char *p, int ic)
{
    int count = 0;
    char *scan = reginput;
    unsigned size = OPSIZE(p);
    const char *opnd = OPERAND(p);
    int data = WCHAR_AT(opnd, opnd + size);

    switch (OP(p)) {
    case ANY:
	count = (int) (regnomore - scan);
	scan += count;
	break;
    case EXACTLY:
	while (scan < regnomore) {
	    if (!EQ_CHARS(data, WCHAR_AT(scan, regnomore), ic))
		break;
	    count++;
	    scan += BYTES_AT(scan, regnomore);
	}
	break;
    case ANYOF:
	while (scan < regnomore && RegStrChr2(opnd, size, scan, ic) != 0) {
	    count++;
	    scan += BYTES_AT(scan, regnomore);
	}
	break;
    case ANYBUT:
	while (scan < regnomore && RegStrChr2(opnd, size, scan, ic) == 0) {
	    count++;
	    scan += BYTES_AT(scan, regnomore);
	}
	break;

#define case_CLASSES(with,without) \
    case with: \
	while (scan < regnomore && is_CLASS(with,scan)) { \
	    count++; \
	    scan += BYTES_AT(scan, regnomore); \
	} \
	break; \
    case without: \
	while (scan < regnomore && !is_CLASS(with,scan)) { \
	    count++; \
	    scan += BYTES_AT(scan, regnomore); \
	} \
	break

	expand_case_CLASSES();

#undef case_CLASSES

    default:			/* Oh dear.  Called inappropriately. */
	regerror("internal foulup");
	count = 0;		/* Best compromise. */
	break;
    }
    reginput = scan;

    return (count);
}

/*
 - regnext - dig the "next" pointer out of a node
 */
static inline char *
regnext(char *p)
{
    int offset;

    if (p == &regdummy)
	return (NULL);

    offset = (int) NEXT(p);
    if (offset == 0)
	return (NULL);

    if (OP(p) == BACK
	|| OP(p) == RCOMPLX)
	return (p - offset);
    else
	return (p + offset);
}

#ifdef REGDEBUG

/*
 - regdump1 - dump a single regexp operation in vaguely comprehensible form
 */
static char *
regdump1(char *program, char *s)
{
    static TBUFF *dump;
    char temp[80];
    char *next;
    int op;
    unsigned len;

    tb_init(&dump, EOS);
    op = OP(s);
    sprintf(temp, "%2d%s", (int) (s - program), regprop(s));	/* Where, what. */
    tb_sappend0(&dump, temp);
    next = regnext(s);
    if (next == NULL)		/* Next ptr. */
	strcpy(temp, "(0)");
    else
	sprintf(temp, "(%d)", (int) ((s - program) + (next - s)));
    tb_sappend0(&dump, temp);
    len = OPSIZE(s);
    s += OP_HDR;
    if (op == ANYOF || op == ANYBUT || op == EXACTLY) {
	tb_sappend0(&dump, visible_buff(s, (int) len, FALSE));
	s += (len + 1);
    } else if (op == RSIMPLE || op == RCOMPLX) {
	s += (RR_LEN - OP_HDR);
    }
    TRACE(("%s\n", tb_values(dump)));
    return s;
}

/*
 - regdump - dump a regexp in vaguely comprehensible form
 */
static void
regdump(regexp * r)
{
    static TBUFF *dump;
    char temp[80];
    char *s;
    char op = EXACTLY;		/* Arbitrary non-END op. */

    TRACE(("regdump\n"));
    s = r->program + 1;
    while (op != END) {		/* While that wasn't END last time... */
	op = OP(s);
	s = regdump1(r->program, s);
    }

    /* Header fields of interest. */
    tb_init(&dump, EOS);
    if (r->regstart >= 0) {
	sprintf(temp, "start `%#x' ", r->regstart);
	tb_sappend0(&dump, temp);
    }
    if (r->reganch) {
	sprintf(temp, "anchored ");
	tb_sappend0(&dump, temp);
    }
    if (r->regmust != -1) {
	tb_sappend0(&dump, "must have \"");
	tb_sappend0(&dump, &(r->program[r->regmust]));
	tb_sappend0(&dump, "\"");
    }
    TRACE(("%s\n", tb_values(dump)));
    tb_free(&dump);
}

/*
 - regprop - printable representation of opcode
 */
static const char *
regprop(char *op)
{
    const char *p = "?";
    static char buf[50];

    (void) strcpy(buf, ":");

#define case_PROP(name) case name: p = #name; break
#define case_CLASSES(with,without) case_PROP(with); case_PROP(without)

    switch (OP(op)) {
	case_PROP(BOL);
	case_PROP(EOL);
	case_PROP(ANY);
	case_PROP(ANYOF);
	case_PROP(ANYBUT);
	case_PROP(BRANCH);
	case_PROP(EXACTLY);
	case_PROP(NEVER);
	case_PROP(NOTHING);
	case_PROP(BACK);
	case_PROP(END);
	case_PROP(BEGWORD);
	case_PROP(ENDWORD);

	expand_case_CLASSES();

    case OPENn:
	(void) sprintf(STRSKIP(buf), "OPEN%d", OP(op) - OPEN);
	p = NULL;
	break;

    case CLOSEn:
	(void) sprintf(STRSKIP(buf), "CLOSE%d", OP(op) - CLOSE);
	p = NULL;
	break;

	case_PROP(STAR);
	case_PROP(PLUS);

    case RSIMPLE:		/* FALLTHROUGH */
    case RCOMPLX:
	(void) sprintf(STRSKIP(buf), "%s%c", OP(op) == RSIMPLE
		       ? "RSIMPLE"
		       : "RCOMPLX", L_CURL);
	if (get_RR_MIN(op))
	    (void) sprintf(STRSKIP(buf), "%d", get_RR_MIN(op));
	strcat(buf, ",");
	if (get_RR_MAX(op))
	    (void) sprintf(STRSKIP(buf), "%d", get_RR_MAX(op));
	(void) sprintf(STRSKIP(buf), "%c", R_CURL);
	p = NULL;
	break;

    default:
	regerror("corrupted opcode");
	break;
    }
    if (p != NULL)
	(void) strcat(buf, p);
    return (buf);
}
#undef case_PROP
#undef case_CLASSES
#endif

#if defined(llength) && defined(lforw) && defined(lback)
/* vile support:
 * like regexec, but takes LINE * as input instead of char *, and may be used
 * multiple times per line.
 */
int
cregexec(regexp * prog,
	 LINE *lp,
	 int startoff,
	 int endoff,
	 int at_bol,
	 int ic)
{
    int s = 0;

    REGTRACE((T_CALLED "lregexec %s%d..%d\n",
	      at_bol ? "bol " : "",
	      startoff,
	      endoff));

    set_utf8flag(curbp);
    if (endoff >= startoff) {
	if (lvalue(lp)) {
	    s = regexec2(prog, lvalue(lp), &(lvalue(lp)[llength(lp)]),
			 startoff, endoff, at_bol, ic);
	} else {
	    /* the prog might be ^$, or something legal on a null string */
	    static char empty[1];
	    char *nullstr = empty;

	    if (startoff > 0) {
		s = 0;
	    } else {
		s = regexec2(prog, nullstr, nullstr, 0, 0, at_bol, ic);
	    }
	    if (s) {
		if (prog->mlen > 0) {
		    mlforce("BUG: non-zero match on null string");
		    s = 0;
		} else {
		    prog->startp[0] = prog->endp[0] = NULL;
		}
	    }
	}
    }
    returnReg(s);
}

/* vile support:
 * like regexec, but takes LINE * as input instead of char *
 */
int
lregexec(regexp * prog,
	 LINE *lp,
	 int startoff,
	 int endoff,
	 int ic)
{
    return cregexec(prog, lp, startoff, endoff, (startoff == 0), ic);
}

/* non-LINE regexec calls for vile */
int
nregexec(regexp * prog,
	 char *string,
	 char *stringend,	/* pointer to the null, if there were one */
	 int startoff,
	 int endoff,
	 int ic)
{
    int s;

    REGTRACE((T_CALLED "nregexec %d..%d\n", startoff, endoff));
    set_utf8flag(NULL);
    s = regexec(prog, string, stringend, startoff, endoff, ic);
    returnReg(s);
}
#endif /* VILE LINE */

/*
 * Parse one slice of a substitute expression, e.g., as needed for
 * s/part1/part2/
 */
char *
regparser(const char **source)
{
    const char *parse = *source;
    int delim = CharOf(*parse);
    char *result = NULL;
    int escape = 0;
    int braces = 0;

    TRACE(("regparser {%s}\n", parse));
    if (delim != '\\' && ispunct(delim)) {
	result = strmalloc(++parse);
	while (*parse != delim && *parse != '\0') {
	    if (escape) {
		escape = 0;
	    } else if (*parse == '\\') {
		escape = 1;
	    } else if (*parse == '[') {
		braces = 1;
	    } else if (braces && *parse == ']' && parse > (*source + 1)) {
		braces = 0;
	    }
	    ++parse;
	}
	if (*parse == '\0' || escape || braces) {
	    free(result);
	    result = NULL;
	} else {
	    result[parse - *source - 1] = '\0';
	}
	if (result != NULL) {
	    TRACE(("...regparser {%s}\n", result));
	    *source = parse;
	}
    }
    return result;
}

void
regfree(regexp * prog)
{
    free(prog);
}

#ifdef DEBUG_REGEXP

#ifdef TEST_MULTIBYTE_REGEX
#define NotImpl(name) fprintf(stderr, "Not implemented: " #name "\n")

TERM term;
FSM_BLIST fsm_file_encoding_blist;
FSM_BLIST fsm_byteorder_mark_blist;

extern char *strmalloc(const char *);
extern void tidy_exit(int);

void
rebuild_charclasses(int print_lo GCC_UNUSED, int print_hi GCC_UNUSED)
{
    NotImpl(rebuild_charclasses);
}

int
ffputline(const char *buf GCC_UNUSED, int nbuf GCC_UNUSED, const char
	  *ending GCC_UNUSED)
{
    NotImpl(ltextfree);
    return 0;
}

void
ltextfree(LINE *lp GCC_UNUSED, BUFFER *bp GCC_UNUSED)
{
    NotImpl(ltextfree);
}

void
set_bufflags(int glob_vals GCC_UNUSED, USHORT flags GCC_UNUSED)
{
    NotImpl(set_bufflags);
}

const char *
choice_to_name(FSM_BLIST * data GCC_UNUSED, int code GCC_UNUSED)
{
    NotImpl(choice_to_name);
    return 0;
}

char *
strmalloc(const char *src)
{
    char *dst = 0;
    if (src != 0) {
	dst = castalloc(char, strlen(src) + 1);
	if (dst != 0)
	    (void) strcpy(dst, src);
    }
    return dst;
}

void
tidy_exit(int code)
{
    exit(code);
}

#if OPT_TRACE
L_NUM
line_no(BUFFER *the_buffer GCC_UNUSED, LINE *the_line GCC_UNUSED)
{
    NotImpl(line_no);
    return 0;
}

int *
itb_values(ITBUFF * p GCC_UNUSED)
{
    NotImpl(itb_values);
    return 0;
}

size_t
itb_length(ITBUFF * p GCC_UNUSED)
{
    NotImpl(itb_length);
    return 0;
}

int
kcod2escape_seq(int c GCC_UNUSED, char *ptr GCC_UNUSED, size_t limit GCC_UNUSED)
{
    NotImpl(kcod2escape_seq);
    return 0;
}

SIGT
imdying(int ACTUAL_SIG_ARGS GCC_UNUSED)
{
    tidy_exit(BADEXIT);
}
#endif
#endif /* TEST_MULTIBYTE_REGEX */

void
mlforce(const char *dummy, ...)
{
    printf("? %s\n", dummy);
}

void
regerror(const char *msg)
{
    printf("? %s\n", msg);
}

static void
alloc_string(char **result, unsigned *have, unsigned need)
{
    if (*result == 0) {
	*have = BUFSIZ;
	*result = typeallocn(char, *have);
    } else if (need >= *have) {
	*have *= 2;
	*result = typereallocn(char, *result, *have);
    }
}

static char *
get_string(FILE *fp, unsigned *length, int linenum)
{
    static char *result = 0;
    static unsigned have = 0;

    int ch;
    int escaped = 0;
    int value = 0;
    int literal = -1;

    alloc_string(&result, &have, BUFSIZ);
    *length = 0;
    for (;;) {
	ch = fgetc(fp);
	if (ch == EOF || ferror(fp)) {
	    *length = 0;
	    if (result != 0) {
		have = 0;
		free(result);
		result = 0;
	    }
	    break;
	} else if (ch == '\n') {
	    break;
	} else if (escaped) {
	    if (escaped == 1) {
		switch (ch) {
		case '\\':
		    value = ch;
		    break;
		case 's':
		    value = ' ';
		    break;
		case 'n':
		    value = '\n';
		    break;
		case 'r':
		    value = '\r';
		    break;
		case 't':
		    value = '\t';
		    break;
#if OPT_MULTIBYTE
		case 'Z':
		    alloc_string(&result, &have, *length + 2);
		    result[*length] = 0xc2;
		    *length += 1;
		    value = 0xa0;
		    break;
#endif
		default:
		    if (isdigit(ch)) {
			value = ch - '0';
			escaped = -2;
		    } else {
			value = ch;
		    }
		}
		if (escaped > 0)
		    escaped = 0;
	    } else if (isdigit(ch)) {
		value = (value * 8) + (ch - '0');
		++escaped;
	    } else {
		fprintf(stderr,
			"line %d: expected 3 digits per octal escape\n", linenum);
		escaped = 0;
	    }
	} else if (ch == '\\' && !literal) {
	    escaped = 1;
	    value = 0;
	} else {
	    value = ch;
	}
	if (!escaped) {
	    alloc_string(&result, &have, *length + 2);
	    result[*length] = (char) value;
	    *length += 1;
	    if (literal < 0)
		literal = (value == '#' || isupper(value)) ? 1 : 0;
	}
    }
    if (result != 0)
	result[*length] = 0;
    return (*length != 0) ? result : 0;
}

static void
put_string(char *s, unsigned length, int literal)
{
    unsigned n;

    for (n = 0; n < length; ++n) {
	if (literal) {
	    putchar(CharOf(s[n]));
	} else {
	    switch (s[n]) {
	    case '\\':
		fputs("\\\\", stdout);
		break;
	    case ' ':
		fputs("\\s", stdout);
		break;
	    case '\n':
		fputs("\\n", stdout);
		break;
	    case '\r':
		fputs("\\r", stdout);
		break;
	    case '\t':
		fputs("\\t", stdout);
		break;
	    default:
#if OPT_MULTIBYTE
		if (reg_utf8flag && CharOf(s[n]) >= 128) {
		    int need = BYTES_AT(s + n, s + length);
		    int data = WCHAR_AT(s + n, s + length);
		    if (data == 0xa0) {
			printf("\\Z");
			n += (need - 1);
			break;
		    } else if (need > 1) {
			while (need-- > 0) {
			    putchar(CharOf(s[n++]));
			}
			n--;
			break;
		    }
		}
#endif
		if (CharOf(s[n]) < ' ' || CharOf(s[n]) > '~') {
		    printf("\\%03o", CharOf(s[n]));
		} else {
		    putchar(CharOf(s[n]));
		}
	    }
	}
    }
    putchar('\n');
}

/*
 * Read script containing patterns (p), test-data (q) and results (r).  The
 * first character of each line is its type:
 * >	Comments begin with '#'.
 * >	Use an uppercase p/q/r to suppress backslash interpretation of the line,
 *	e.g., to simplify typing patterns.
 * >	Use I/i to switch ignorecase on/off in the call to regexec().
 * >	Use M/m to switch magic on/off in the call to regcomp().
 * >	Use N to disable regmassage (used for vile), so expressions are POSIX.
 * >	Lines beginning with '?' are error messages.
 * >	Other lines are converted to comments.
 *
 * The output of the test driver contains the comment lines, p- and q-lines as
 * well as the computed r-lines.  Because special characters are encoded as
 * backslash sequences, it is simple to compare the driver's input and output
 * using "diff", to see where a test differs.
 */
static void
test_regexp(FILE *fp)
{
    char *s;
    int ic = FALSE;
    int magic = TRUE;
    int linenum = 0;
    int literal;
    int subexp;
    unsigned offset;
    unsigned length;
    regexp *pattern = 0;

    while ((s = get_string(fp, &length, ++linenum)) != 0) {
	literal = 0;
	REGTRACE(("-------------->%s\n", s));
	switch (*s) {
	case 'i':
	    ic = FALSE;
	    put_string(s, length, TRUE);
	    break;
	case 'I':
	    ic = TRUE;
	    put_string(s, length, TRUE);
	    break;
	case 'N':
	    magic = -1;
	    put_string(s, length, TRUE);
	    break;
	case 'M':
	    magic = 1;
	    put_string(s, length, TRUE);
	    break;
	case 'm':
	    magic = 0;
	    put_string(s, length, TRUE);
	    break;
	case 'P':
	    literal = 1;
	    /* FALLTHRU */
	case 'p':
	    put_string(s, length, literal);
	    /* compile pattern */
	    if (pattern != 0)
		free(pattern);
	    ++s, --length;
	    pattern = regcomp(s, length, magic);
	    break;
	case 'Q':
	    literal = 1;
	    /* FALLTHRU */
	case 'q':
	    put_string(s, length, literal);
	    /* compute results */
	    ++s, --length;
	    if (pattern != 0) {
		for (offset = 0; offset <= length; ++offset) {
		    if (regexec(pattern, s, s + length, offset, length, ic)) {
			for (subexp = 0; subexp < NSUBEXP; ++subexp) {
			    if (pattern->startp[subexp] != 0
				&& pattern->endp[subexp] != 0
				&& (pattern->endp[subexp] >=
				    pattern->startp[subexp])) {
				unsigned mlen =
				pattern->endp[subexp] -
				pattern->startp[subexp];
				if (mlen <= 1) {
				    printf("r%d [%d:%d] -> [%ld]",
					   subexp, offset, length - mlen,
					   pattern->startp[subexp] - s);
				} else {
				    printf("r%d [%d:%d] -> [%ld:%ld]",
					   subexp, offset, length - 1,
					   pattern->startp[subexp] - s,
					   pattern->endp[subexp] - s - 1);
				}
				put_string(pattern->startp[subexp], mlen, FALSE);
			    } else if (pattern->startp[subexp] != 0
				       && pattern->endp[subexp] != 0) {
				printf("? mlen = %ld\n",
				       pattern->endp[subexp] -
				       pattern->startp[subexp]);
			    } else if (pattern->startp[subexp] != 0) {
				printf("? null end[%d] pointer\n", subexp);
			    } else if (pattern->endp[subexp] != 0) {
				printf("? null start[%d] pointer\n", subexp);
			    }
			}
		    }
		}
	    }
	    break;
	case '#':
	    put_string(s, length, TRUE);
	    break;
	case 'R':
	case 'r':
	    /* absorb - see 'q' case */
	    break;
	case '?':
	    /* absorb - see 'p' case */
	    break;
	default:
	    putchar('#');
	    put_string(s, length, TRUE);
	    break;
	}
    }
    if (pattern != 0)
	free(pattern);
}

int
main(int argc, char *argv[])
{
    int n;

#if OPT_MULTIBYTE
    if (argc > 1 && strstr(argv[1], "utf8")) {
	vl_init_8bit("en_US.UTF-8", "en_US");
	reg_utf8flag = (vl_encoding >= enc_UTF8);
    } else {
	vl_init_8bit(0, 0);
    }
    vl_ctype_init(0, 0);
#endif
#if 0
    for (n = 0; n < 256; ++n) {
	printf("%3d [%2X] %c%c%c%c%c%c%c%c%c%c%c%c%c\n",
	       n, n,
	       is_ALPHA(n) ? CHR_ALPHA : '-',
	       is_ALNUM(n) ? CHR_ALNUM : '-',
	       is_BLANK(n) ? CHR_BLANK : '-',
	       is_CNTRL(n) ? CHR_CNTRL : '-',
	       is_DIGIT(n) ? CHR_DIGIT : '-',
	       is_GRAPH(n) ? CHR_GRAPH : '-',
	       is_IDENT(n) ? CHR_IDENT : '-',
	       is_LOWER(n) ? CHR_LOWER : '-',
	       is_PRINT(n) ? CHR_PRINT : '-',
	       is_PUNCT(n) ? CHR_PUNCT : '-',
	       is_SPACE(n) ? CHR_SPACE : '-',
	       is_UPPER(n) ? CHR_UPPER : '-',
	       is_XDIGIT(n) ? CHR_XDIGIT : '-');
    }
#endif

    if (argc > 1) {
	for (n = 1; n < argc; ++n) {
	    FILE *fp = fopen(argv[n], "r");
	    if (fp != 0) {
		test_regexp(fp);
		fclose(fp);
	    } else {
		perror(argv[n]);
		return EXIT_FAILURE;
	    }
	}
    } else {
	test_regexp(stdin);
    }
#if NO_LEAKS

    vl_ctype_discard();
#if OPT_LOCALE
    eightbit_leaks();
#endif
    tb_leaks();

    /* whatever is left over must be a leak */
    show_alloc();
    show_elapsed();
    trace_leaks();
#endif /* NO_LEAKS */
    return EXIT_SUCCESS;
}
#endif
