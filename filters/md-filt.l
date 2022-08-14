%pointer
%s sTEXT sCODE FENCE QUOTED QUOTES HTML

%a 10000
%o 10000

%{

/*
 * $Id: md-filt.l,v 1.33 2022/08/14 23:13:39 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a markdown file.
 *
 * https://daringfireball.net/projects/markdown/
 * https://www.markdownguide.org/
 *
 * TODO optionally up to 3 spaces before item-lists, etc.
 * TODO markup ">line"
 * TODO markup ">paragraph"
 */

#include <filters.h>
#include <fltstack.h>
#undef YY_NO_UNPUT

DefineFilter(md);

typedef struct {
    int mark;			/* IsMark... */
    int c1st;			/* column for mark */
    int c2nd;			/* column for marked text */
} ITEM_STACK;

static void atx_header(const char *, int);
static void emphasize(const char *, int, int, const char *);
static void fence_start(const char *, int);
static void fence_finish(const char *, int);
static void indent_code(const char *, int);
static int valid_indent(const char *, int, int);

static char *Action_attr;
static char *Code_attr = "";
static char *Error_attr;
static char *Ident2_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static int nbl;			/* count characters on non-blank line */
static int pbl;			/* previous line's count for "nbl" */
static int item_level;		/* if >= 0, index in item_stack[] */

static int item_limit;
static int item_depth;
static ITEM_STACK *item_stack;

#define isMark(c) ((c) == '*' || (c) == '-' || (c) == '+' || (c) == ':' || isdigit(c))

%}

WHITE		[\t ]
DASH_A		([*]{WHITE}*)+[*]
DASH_M		([-]{WHITE}*)+[-]
DASH_U		([_]{WHITE}*)+[_]
DASHES		({DASH_A}|{DASH_M}|{DASH_U})
DASHED		[=]+

ITEM_A		([*]{WHITE}+[^*])
ITEM_C		([:]{WHITE}+[^:])
ITEM_M		([-]{WHITE}+[^-])
ITEM_O		([[:digit:]]+\.{WHITE}+[[:graph:]])
ITEM_P		([+]{WHITE}+[^+])
ITEM		({ITEM_A}|{ITEM_C}|{ITEM_M}|{ITEM_O}|{ITEM_P})

TEXT		[[:graph:]]

CAN_MARK	({TEXT}|{WHITE})

SIGN		[-+]
DECIMAL		[[:digit:]_]+
OCTAL		0[0-7_]+
HEXADECIMAL	0x[[:xdigit:]_]+
REAL		([[:digit:]_]*\.[[:digit:]][[:digit:]_]*)([eE]{SIGN}?[[:digit:]_]+)?
NUMBER		{SIGN}?({DECIMAL}|{OCTAL}|{HEXADECIMAL}|{REAL})

NAME		[[:alnum:]_.#-]+
ADDRESS		({NAME}|\.)+
EMAIL		{NAME}@{ADDRESS}

URL		[[:alpha:]]+"://"[[:alnum:]#%&+\,.\/?;@_=~-]+

%%

<sTEXT,HTML>^[\r\n]	{ pbl = nbl; nbl = 0;
			  if (cur_state == HTML) {
			    PopQuote();
			  } else {
			    ECHO;
			  }
			  new_state(sTEXT);
			}

<sTEXT>^\<.*\>		{ PushQuote(HTML, Preproc_attr); }
<HTML>[\r\n]		|
<HTML>.			{ flt_bfr_append(yytext, yyleng); }

<sTEXT,HTML>^```[^\r\n]* { fence_start(yytext, yyleng); }
<FENCE>^```[^\r\n]*	{ fence_finish(yytext, yyleng); }
<FENCE>^[^\r\n]+	{ WriteToken(String_attr); }
<FENCE>[\r\n]+		{ ECHO; }

<sTEXT>^"#"[^\r\n]+	{ atx_header(yytext, yyleng); }

<sTEXT>^{DASHES}$	{ WriteToken(Action_attr); }
<sTEXT>^{DASHED}+	{ WriteToken(Action_attr); }

<sTEXT>^{WHITE}*{ITEM}	{ if (valid_indent(yytext, yyleng, 1)) {
			      WriteToken3(Action_attr, yyleng - 1);
			      unput(yytext[yyleng-1]);
			  } else {
			      WriteToken3("", yyleng - 1);
			      unput(yytext[yyleng-1]);
			      indent_code(yytext, yyleng);
			  }
			}
<sTEXT>^{WHITE}*{TEXT}	{ if (valid_indent(yytext, yyleng, 0)) {
			      WriteToken3(Action_attr, yyleng - 1);
			      unput(yytext[yyleng-1]);
			  } else {
			      WriteToken3("", yyleng - 1);
			      unput(yytext[yyleng-1]);
			      indent_code(yytext, yyleng);
			  }
			}
<sTEXT>^{WHITE}*$	{ if (item_level >= 0) {
			    item_level = -1;
			    item_depth = 0;
			  }
			  ECHO;
			}

<sTEXT>[\\][[:punct:]]	{ WriteToken(Action_attr); }

<sTEXT>[_][_][_]{CAN_MARK}+[_][_][_]	{ emphasize(yytext, yyleng, 2, "IB"); }
<sTEXT>[_][_]{CAN_MARK}+[_][_]		{ emphasize(yytext, yyleng, 2, "B"); }
<sTEXT>[_]{CAN_MARK}+[_]		{ emphasize(yytext, yyleng, 1, "I"); }
<sTEXT>[*][*][*]{CAN_MARK}+[*][*][*]	{ emphasize(yytext, yyleng, 2, "IB"); }
<sTEXT>[*][*]{CAN_MARK}+[*][*]		{ emphasize(yytext, yyleng, 2, "B"); }
<sTEXT>[*]{CAN_MARK}+[*]		{ emphasize(yytext, yyleng, 1, "I"); }
<sTEXT>[`][`]{CAN_MARK}+[`][`]		{ emphasize(yytext, yyleng, 2, String_attr); }
<sTEXT>[`]{CAN_MARK}+[`]		{ emphasize(yytext, yyleng, 1, String_attr); }

<sTEXT,sCODE>{NUMBER}	{ WriteToken(Number_attr); }
<sTEXT,sCODE>{EMAIL}	{ WriteToken(Ident2_attr); }
<sTEXT,sCODE>{URL}	{ WriteToken(Ident2_attr); }

<sCODE>[^\r\n]+		{ WriteToken(Code_attr); }
<sCODE>[\r\n]		{ ECHO; BEGIN(sTEXT); }

<QUOTES,QUOTED>{EMAIL}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }
<QUOTES,QUOTED>{URL}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }

<sTEXT>\(\"\)		|
<sTEXT>\'.\'		{ ECHO; }

<sTEXT>\"		{ PushQuote(QUOTED, String_attr); }
<QUOTED>\"		{ PopQuote(); }
<QUOTED>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTED>^[\n]		{ PopQuote(); }
<QUOTED>[\n]		{ flt_bfr_append(yytext, yyleng); }

<sTEXT>\`\`		{ PushQuote(QUOTES, String_attr); }
<QUOTES>\'\'		{ PopQuote(); }
<QUOTES>.		{ flt_bfr_append(yytext, yyleng); }
<QUOTES>^[\n]		{ PopQuote(); }
<QUOTES>[\n]		{ flt_bfr_append(yytext, yyleng); }

<sTEXT>[\r\n]		{ ECHO; }
<sTEXT>.		{ ECHO; }

%%

#include <fltstack.h>

/*
 * atx-style headers use 1-6 hash characters at the start of the line,
 * corresponding to header levels 1-6.
 */
static void
atx_header(const char *value, int length)
{
    int level = 1;
    for (level = 0; level < length; ++level) {
	if (value[level] != '#' || (level == 6))
	    break;
    }
    if (level == length || !isspace(value[level])) {
	flt_puts(value, length, Error_attr);
    } else {
	flt_puts(value, level, Action_attr);
	flt_puts(value + level, length - level, String_attr);
    }
}

/*
 * Check the indent-level for the current token, updating item_level.
 */
static int
valid_indent(const char *value, int length, int mark)
{
    int result = 0;
    int actual = 0;
    int marked = 0;
    int c1st = -1;
    int c2nd = -1;
    int n;

    for (n = 0; n < length - 1; ++n) {
	int ch = value[n];
	if (ch == '\t') {
	    actual = 1 + (actual | 7);
	} else if (ch == ' ') {
	    ++actual;
	} else if (mark && !marked && isMark(ch)) {
	    c1st = actual++;
	    marked = 1;
	} else {
	    ++actual;
	}
    }

#define SameMark(tst,ref) (isdigit(ref) ? isdigit(tst) : ((ref) == (tst)))

    if (mark) {
	mark = *value;
	c2nd = actual;
	if (c1st == 0) {
	    result = 1;
	    /* begin/resume top-level */
	    item_depth = 0;
	    item_stack[item_depth].mark = mark;
	    item_stack[item_depth].c1st = c1st;
	    item_stack[item_depth].c2nd = c2nd;
	    item_depth++;
	} else if (item_depth > 0) {
	    if (SameMark(mark, item_stack[item_depth - 1].mark) &&
		c1st == item_stack[item_depth - 1].c1st &&
		c2nd == item_stack[item_depth - 1].c2nd) {
		/* continue existing level */
		result = 1;
	    } else if (c1st == item_stack[item_depth - 1].c2nd) {
		result = 1;
		/* begin newer level */
		if ((item_depth + 1) >= item_limit) {
		    item_limit += 10;
		    item_stack = typereallocn(ITEM_STACK, item_stack, item_limit);
		}
		item_stack[item_depth].mark = mark;
		item_stack[item_depth].c1st = c1st;
		item_stack[item_depth].c2nd = c2nd;
		item_depth++;
	    } else if (c1st < item_stack[item_depth - 1].c1st) {
		/* look for match in older/upper levels */
		for (n = item_depth; n > 0; --n) {
		    if (c1st == item_stack[n - 1].c1st) {
			result = 1;
			/* TODO warn if mismatch for mark/c2nd */
			item_depth = n;
			break;
		    }
		}
	    }
	}
    } else {
	c1st = c2nd = actual;
	if (item_depth > 0) {
	    if (c2nd == item_stack[item_depth - 1].c2nd) {
		result = 1;
	    } else if (c2nd < item_stack[item_depth - 1].c2nd) {
		/* look for match in older/upper levels */
		for (n = item_depth; n > 0; --n) {
		    if (c2nd == item_stack[n - 1].c2nd) {
			result = 1;
			item_depth = n;
			break;
		    }
		}
	    }
	}
    }
    if (result == 1) {
	item_level = (mark
		      ? item_stack[item_depth - 1].c1st
		      : item_stack[item_depth - 1].c2nd);
    } else {
	item_depth = 0;
	item_level = -1;
    }
    return result;
}

static void
emphasize(const char *value, int length, int margin, const char *attr)
{
    int actual = length;
    int limit = length - margin;
    int n;

    /*
     * Validate, determine the actual length, in case there is more than one
     * string to emphasize on the same line.
     */
    for (n = margin; n < limit - margin; ++n) {
	if (!strncmp(value + n, value, margin)) {
	    actual = n + margin;
	    break;
	}
    }
    if (actual < length) {
	if (actual == 0) {
	    flt_puts(value, margin, "U");
	    actual = margin;
	}
	while (length-- > actual)
	    unput(value[length]);
    }
    if (actual > 0) {
	flt_puts(value, margin, Action_attr);
	flt_puts(value + margin, actual - (2 * margin), attr);
	flt_puts(value, margin, Action_attr);
    }
}

static void
fence_start(const char *value, int length)
{
    flt_puts(value, 3, Action_attr);
    if (length > 3)
	flt_puts(value + 3, length - 3, String_attr);
    BEGIN(FENCE);
}

static void
fence_finish(const char *value, int length)
{
    flt_puts(value, 3, Action_attr);
    if (length > 3)
	flt_puts(value + 3, length - 3, Error_attr);
    BEGIN(sTEXT);
}

static void
indent_code(const char *value, int length)
{
    if (*value == '\t' || (length > 4 && !strncmp(value, "    ", 4)))
	BEGIN(sCODE);
}

static void
init_filter(int before GCC_UNUSED)
{
    (void) before;
}

static void
do_filter(FILE *inputs)
{
    InitLEX(inputs);

    Action_attr = class_attr(NAME_ACTION);
    Error_attr = class_attr(NAME_ERROR);
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    nbl = pbl = 0;
    item_depth = 0;
    item_level = -1;
    if (item_limit == 0) {
	item_limit = 10;
	item_stack = typecallocn(ITEM_STACK, item_limit);
    }
    begin_state(sTEXT);
    RunLEX();
    flt_bfr_finish();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
