%pointer
%s sTEXT sCODE FENCE QUOTED QUOTES HTML NOTE

%a 10000
%o 10000

%{

/*
 * $Id: md-filt.l,v 1.52 2022/08/18 08:19:19 tom Exp $
 *
 * Filter to add vile "attribution" sequences to a markdown file.
 *
 * https://daringfireball.net/projects/markdown/
 * https://www.markdownguide.org/
 *
 * TODO `code`
 * TODO [label](link)
 * TODO [label][link] reference links
 * TODO markup ">line"
 * TODO markup ">paragraph"
 */

#include <filters.h>
#include <fltstack.h>
#undef YY_NO_UNPUT

#define flt_puts(b,n,a) flt_bfr_embed(b,n,a)

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
static void link_link(const char *, int);
static void link_cite(const char *, int);
static void scan_html(const char *, int);
static void underline(const char *, int, int, const char *);
static int reject_links(const char *, int);
static int valid_indent(const char *, int, int);

static char *Action_attr;
static char *Code_attr = "";
static char *Comment_attr;
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

static int html_level;

#define NoItem()  { item_depth = 0; item_level = -1; }

#define isMark(c) ((c) == '*' || (c) == '-' || (c) == '+' || (c) == ':' || isdigit(c))

%}

WS		[\t ]
DASH_A		([*]{WS}*)+[*]
DASH_M		([-]{WS}*)+[-]
DASH_U		([_]{WS}*)+[_]
DASHES		({DASH_A}|{DASH_M}|{DASH_U})
DASHED		[=]+

ITEM_A		([*]{WS}+[^*])
ITEM_C		([:]{WS}+[^:])
ITEM_M		([-]{WS}+[^-])
ITEM_O		([[:digit:]]+\.{WS}+[[:graph:]])
ITEM_P		([+]{WS}+[^+])
ITEM		({ITEM_A}|{ITEM_C}|{ITEM_M}|{ITEM_O}|{ITEM_P})

TEXT		[[:graph:]]

N_		[^[:alnum:]_\r\n]
Y_		[^_\r\n]
T_		[^\r\n]

CODE1		{WS}|[[:alnum:]]|[~!@#$%^&*()_+={}[\]|\\:"<>?;',./]|[-]
CODE2		{CODE1}|[`]

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
			    flt_bfr_append(yytext, yyleng);
			  }
			  new_state(sTEXT);
			}

<sTEXT>^[<]!--		{ PushQuote(NOTE, Comment_attr);
			  scan_html(yytext, yyleng);
			}
<NOTE>--[>]		{ scan_html(yytext, yyleng);
			  if (html_level <= 0) {
			    PopQuote();
			    html_level = 0;
			  } else {
			    flt_bfr_append(yytext, yyleng);
			  }
			}
<NOTE>[\r\n]		|
<NOTE>.			{ flt_bfr_append(yytext, yyleng);
			  scan_html(yytext, yyleng);
			}

<sTEXT>^\<		{
			  PushQuote(HTML, Preproc_attr);
			  scan_html(yytext, yyleng);
			}
<HTML>[\r\n]		{ flt_bfr_append(yytext, yyleng); }
<HTML>^{WS}*[<]		{ flt_bfr_append(yytext, yyleng);
			  scan_html(yytext, yyleng);
			}
<HTML>^{WS}*[^<]	{ int n;
			  if (html_level <= 0) {
			    html_level = 0;
			    for (n = yyleng; n > 0; --n)
				unput(yytext[n - 1]);
			    flt_bfr_finish();
			    pop_state();
			  } else {
			    flt_bfr_append(yytext, yyleng);
			    scan_html(yytext, yyleng);
			  }
			}
<HTML>[^\r\n]		{
			  flt_bfr_append(yytext, yyleng);
			  scan_html(yytext, yyleng);
			}

<sTEXT>^{WS}*```{T_}*	{ fence_start(yytext, yyleng); }
<FENCE>^{WS}*```{T_}*	{ fence_finish(yytext, yyleng); }
<FENCE>^{T_}+		{ WriteToken(String_attr); }
<FENCE>[\r\n]+		{ flt_bfr_append(yytext, yyleng); }

<sTEXT>"["[^]]+"]("[^)]+")" { link_link(yytext, yyleng); }
<sTEXT>"["[^]]+"]["[^)]+"]" { link_cite(yytext, yyleng); }

<sTEXT>^"#"{T_}+	{ atx_header(yytext, yyleng); NoItem(); }

<sTEXT>^{DASHES}$	|
<sTEXT>^{DASHED}+	{ WriteToken(Action_attr); NoItem(); }

<sTEXT>^{WS}*{ITEM}	{ if (valid_indent(yytext, yyleng, 1)) {
			      WriteToken3(Action_attr, yyleng - 1);
			      unput(yytext[yyleng-1]);
			  } else {
			      WriteToken3("", yyleng - 1);
			      unput(yytext[yyleng-1]);
			      indent_code(yytext, yyleng);
			  }
			}
<sTEXT>^{WS}*{TEXT}	{ if (valid_indent(yytext, yyleng, 0)) {
			      WriteToken3(Action_attr, yyleng - 1);
			      unput(yytext[yyleng-1]);
			  } else {
			      if (yyleng < 3 && yytext[0] != '\t')
			          NoItem();
			      WriteToken3("", yyleng - 1);
			      unput(yytext[yyleng-1]);
			      indent_code(yytext, yyleng);
			  }
			}

<sTEXT>[\\][[:punct:]]	{ WriteToken(Action_attr); }

<sTEXT>^[_][_][_]{Y_}+[_][_][_]$	{ underline(yytext, yyleng, 3, "IB"); }
<sTEXT>^[_][_][_]{Y_}+[_][_][_]{N_}	{ underline(yytext, yyleng, 3, "IB"); }
<sTEXT>{N_}[_][_][_]{Y_}+[_][_][_]$	{ underline(yytext, yyleng, 3, "IB"); }
<sTEXT>{N_}[_][_][_]{Y_}+[_][_][_]{N_}	{ underline(yytext, yyleng, 3, "IB"); }

<sTEXT>^[_][_]{Y_}+[_][_]$		{ underline(yytext, yyleng, 2, "B"); }
<sTEXT>^[_][_]{Y_}+[_][_]{N_}		{ underline(yytext, yyleng, 2, "B"); }
<sTEXT>{N_}[_][_]{Y_}+[_][_]$		{ underline(yytext, yyleng, 2, "B"); }
<sTEXT>{N_}[_][_]{Y_}+[_][_]{N_}	{ underline(yytext, yyleng, 2, "B"); }

<sTEXT>^[_]{Y_}+[_]$			{ underline(yytext, yyleng, 1, "I"); }
<sTEXT>^[_]{Y_}+[_]{N_}			{ underline(yytext, yyleng, 1, "I"); }
<sTEXT>{N_}[_]{Y_}+[_]$			{ underline(yytext, yyleng, 1, "I"); }
<sTEXT>{N_}[_]{Y_}+[_]{N_}		{ underline(yytext, yyleng, 1, "I"); }

<sTEXT>[*][*][*]{T_}+[*][*][*]		{ emphasize(yytext, yyleng, 3, "IB"); }
<sTEXT>[*][*]{T_}+[*][*]		{ emphasize(yytext, yyleng, 2, "B"); }
<sTEXT>[*]{T_}+[*]			{ emphasize(yytext, yyleng, 1, "I"); }

<sTEXT>[`][`]{CODE2}+[`][`]		{ emphasize(yytext, yyleng, 2, String_attr); }
<sTEXT>[`]{CODE1}+[`]			{ emphasize(yytext, yyleng, 1, String_attr); }

<sTEXT,sCODE>{NUMBER}	{ WriteToken(Number_attr); }
<sTEXT,sCODE>{EMAIL}	|
<sTEXT,sCODE>{URL}	{ WriteToken(Ident2_attr); }

<sCODE>{T_}+		{ WriteToken(Code_attr); }
<sCODE>[\r\n]		{ flt_bfr_append(yytext, yyleng); new_state(sTEXT); }

<QUOTES,QUOTED>{EMAIL}	|
<QUOTES,QUOTED>{URL}	{ flt_bfr_embed(yytext, yyleng, Ident2_attr); }

<sTEXT>\(\"\)		|
<sTEXT>\'.\'		{ flt_bfr_append(yytext, yyleng); }

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

<sTEXT>[\r\n]		|
<sTEXT>.		{ flt_bfr_append(yytext, yyleng); }

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
	flt_bfr_embed(value, length, Error_attr);
    } else {
	flt_bfr_embed(value, level, Action_attr);
	flt_bfr_embed(value + level, length - level, String_attr);
    }
}

static int
indent_column(const char *value, int length)
{
    int result = 0;
    int n;

    for (n = 0; n < length; ++n) {
	int ch = value[n];
	if (ch == '\t')
	    result = 1 + (result | 7);
	else if (ch == ' ')
	    ++result;
	else
	    break;
    }
    return result;
}

static int
left_margin(void)
{
    int result = 0;
    if (item_depth > 0)
	result = item_stack[item_depth - 1].c2nd;
    return result;
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
	if (c1st <= 3
	    && (item_depth == 0
		|| item_stack[item_depth - 1].c1st >= c1st)) {
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
    }
    return result;
}

static void
emphasize(const char *value, int length, int margin, const char *attr)
{
    int actual = length;
    int limit = length - margin;
    int n;

    if (reject_links(value, length)) {
	flt_bfr_embed(value, 0, "");
	return;
    }

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
	    flt_bfr_embed(value, margin, "U");
	    actual = margin;
	}
	while (length-- > actual)
	    unput(value[length]);
    }
    if (actual > 0) {
	flt_bfr_embed(value, margin, Action_attr);
	flt_bfr_embed(value + margin, actual - (2 * margin), attr);
	flt_bfr_embed(value, margin, Action_attr);
    }
}

static void
fence_start(const char *value, int length)
{
    int n;
    if ((n = indent_column(value, length)) >= left_margin() + 3) {
	indent_code(value, length);
    } else {
	n += 3;
	flt_bfr_embed(value, n, Action_attr);
	if (length > n)
	    flt_bfr_embed(value + n, length - n, String_attr);
	new_state(FENCE);
    }
}

static void
fence_finish(const char *value, int length)
{
    int n;
    if ((n = indent_column(value, length)) >= left_margin() + 3) {
	indent_code(value, length);
    } else {
	n += 3;
	flt_bfr_embed(value, n, Action_attr);
	if (length > n)
	    flt_bfr_embed(value + n, length - n, Error_attr);
	new_state(sTEXT);
    }
}

static void
indent_code(const char *value, int length)
{
    if (indent_column(value, length) > left_margin() + 3)
	new_state(sCODE);
}

static void
link_link(const char *value, int length)
{
    int n;

    flt_bfr_embed(value, 1, Action_attr);
    for (n = 1; n < length; ++n) {
	if (value[n] == R_BLOCK)
	    break;
    }
    flt_bfr_embed(value + 1, n - 1, String_attr);
    flt_bfr_embed(value + n, 1, Action_attr);
    flt_bfr_embed(value + n + 1, 1, Action_attr);
    flt_bfr_embed(value + n + 2, length - n - 3, Preproc_attr);
    flt_bfr_embed(value + length - 1, 1, Action_attr);
}

static void
link_cite(const char *value, int length)
{
    int n;

    flt_bfr_embed(value, 1, Action_attr);
    for (n = 1; n < length; ++n) {
	if (value[n] == R_BLOCK)
	    break;
    }
    flt_bfr_embed(value + 1, n - 1, String_attr);
    flt_bfr_embed(value + n, 1, Action_attr);
    flt_bfr_embed(value + n + 1, 1, Action_attr);
    flt_bfr_embed(value + n + 2, length - n - 3, Preproc_attr);
    flt_bfr_embed(value + length - 1, 1, Action_attr);
}

/*
 * Delimiters used for emphasis (especially "_") are also valid in link
 * descriptions.  While the beginning of a link is out of reach, recover from
 * this conflict by rejecting the rest of the unintended emphasis.
 */
static int
reject_links(const char *value, int length)
{
    int result = 0;
    int j;

    for (j = 0; j < length; ++j) {
	if (value[j] == R_BLOCK) {
	    int ch = value[j + 1];
	    int k;

	    if (ch == ' ')
		ch = value[j + 2];
	    if (ch == L_BLOCK || ch == L_PAREN) {
		result = 1;
		for (k = length; k > 1; --k) {
		    unput(value[k - 1]);
		}
		flt_bfr_embed(value, 1, "");
		break;
	    }
	}
    }
    return result;
}

static void
scan_html(const char *value, int length)
{
    int n;
    for (n = 0; n < length; ++n) {
	if (value[n] == L_ANGLE) {
	    html_level++;
	} else if (value[n] == R_ANGLE) {
	    --html_level;
	}
    }
}

static void
underline(const char *value, int length, int margin, const char *attr)
{
    if (*value != '_') {
	flt_bfr_embed(value, 1, "");
	underline(value + 1, length - 1, margin, attr);
    } else if (length > 0 && value[length - 1] != '_') {
	underline(value, length - 1, margin, attr);
	flt_bfr_embed(value + length - 1, 1, "");
    } else {
	emphasize(value, length, margin, attr);
    }
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
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident2_attr = class_attr(NAME_IDENT2);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    nbl = pbl = 0;
    item_depth = 0;
    item_level = -1;
    html_level = 0;
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
