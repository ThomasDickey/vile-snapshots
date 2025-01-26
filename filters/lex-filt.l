%pointer
%s RULES RULE1 RULEX RULER STATES ACTIONS ACTION0 ACTION1 ACTION2 ACTION3 CODE CODE1 COMMENT

%a 20000
%n 10000
%o 30000
%p 25000

%{

/*
 * $Id: lex-filt.l,v 1.89 2025/01/26 21:48:59 tom Exp $
 *
 * Filter to add vile "attribution" sequences to selected bits of LEX program.
 */

#include <filters.h>
#include <fltstack.h>

DefineFilter(lex);

#define NAME_LEX_PATTERN "LexPattern"
#define NAME_LEX_SECTION "LexSection"
#define NAME_LEX_STATES  "LexStates"

static char *Action_attr;
static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Keyword_attr;
static char *Number_attr;
static char *Preproc_attr;
static char *String_attr;

static char *Pattern_attr;
static char *Section_attr;
static char *States_attr;

static int section = 0;
static int nesting = 0;
static int bracket = 0;

static void end_action(void);
static void set_rules(void);
static void set_state(void);
static void write_patterns(char *text, int len);
static void write_states(char *text, int len);

%}

SPACE		[[:blank:]]

IDENT		[[:alpha:]_][[:alnum:]_]*

DIRECTIVE	^%([{}+*-]|{IDENT})

INTEGER		[-+]?([[:digit:]]+)

SSTRING		\'(\\.|[^'\\])*\'
DSTRING		\"(\\.|[^"\\]|\\\r?\n)*\"
STRINGS		({SSTRING}|{DSTRING})

ESCAPED		(\\[^\r\n])

UNQUOTED0	([^{}"\\[\][:space:]<>])
UNQUOTED1	([^{}"\\[\][:space:]])

QUOTED		(\"([^"\\\r\n]|{ESCAPED})*\")

CCLASS		("[:"{IDENT}":]")

RANGE0		(\^[^\r\n])
RANGE1		([^\r\n\]]|{CCLASS}|{ESCAPED})
RANGE		("["{RANGE0}?{RANGE1}*"]")

LIMITED		([[:digit:]]+([,][[:digit:]]+)*)
BRACED		("{"({IDENT}|{LIMITED}+)"}")

		/*
		 * Combining all of these pieces makes the lex filter much
		 * larger than the other lex-based filters.
		 */
PATTERN0	({ESCAPED}|{BRACED}|{QUOTED}|{RANGE}|{UNQUOTED0})
PATTERN1	({ESCAPED}|{BRACED}|{QUOTED}|{RANGE}|{UNQUOTED1})
PATTERN		(({PATTERN0}|"("{PATTERN1}")")+|"<<EOF>>")
PATTERNS	(({PATTERN0}|{PATTERN1}|"("{PATTERN1}")")+|"<<EOF>>")

STATES		("<"("*"|({IDENT}|\,)+)">")

%%

		/*
		 * An entirely blank line should not affect the state.
		 */
^{SPACE}+$	{ ECHO; }

		/*
		 * Lines in the patterns section beginning with whitespace
		 * are passed through to the output as-is.  The main use for
		 * that is to allow inline comments.  However, variables also
		 * can be declared.
		 */
<RULES>^{SPACE}+	{
    			ECHO;
			new_state(CODE1);
		}

		/*
		 * Handle comments.  flex actually pays attention only to
		 * newlines and C-style comments.  We handle "//" here to
		 * help with compiling its output.
		 */
<RULES,ACTION0,ACTION3,CODE,CODE1>"//"[^\r\n]* {
			WriteToken(Comment_attr);
		}
<RULES,ACTION0,ACTION3,CODE,CODE1>"/*" {
			PushQuote(COMMENT, Comment_attr);
		}
<RULES,ACTIONS,CODE>^"%%"{SPACE}*[^[:space:]]+[^\r\n]*	{
			WriteToken(Comment_attr);
		}

<RULES,ACTIONS,CODE>^"%%"{SPACE}*	|
<RULES,ACTIONS,CODE>{DIRECTIVE}	{
		    WriteToken(Section_attr);
		    switch(yytext[1]) {
		    case '%':
			section++;
			set_state();
			break;
		    case '{':
			new_state(CODE);
			break;
		    case '}':
			set_state();
			break;
		    case 'S': /* FALLTHRU */
		    case 's': /* FALLTHRU */
		    case 'X': /* FALLTHRU */
		    case 'x':
			new_state(STATES);
			break;
		    case '+':
		    case '-':
		    case '*':
			break;
		    default:
			break;
		    }
		}

<STATES>{IDENT}		{
			  const char *attr = class_attr(yytext);
			  if (attr == NULL) {
			    insert_keyword(yytext, NAME_LEX_STATES, 0);
			    attr = get_keyword_attr(yytext);
			  } else {
			    attr = Error_attr;
			    flt_error("Keyword \"%s\" is already a classname", yytext);
			  }
			  WriteToken(attr);
			}
<STATES>{SPACE}		{ ECHO; }
<STATES>[^\r\n[:blank:]]	{ WriteToken(Error_attr);
			  flt_error("Expected newline or blanks");
			}
<STATES>[\n]		{ ECHO; set_state(); }

<RULES>^{IDENT}		{
			  if (set_symbol_table(NAME_LEX_PATTERN)) {
			      const char *attr = class_attr(yytext);
			      if (attr == NULL) {
				  insert_keyword(yytext, Pattern_attr, 0);
				  attr = get_keyword_attr(yytext);
			      } else {
				  attr = Error_attr;
				  flt_error("Keyword \"%s\" is already a classname", yytext);
			      }
			      WriteToken(attr);
			      set_symbol_table(default_table);
			  } else {
			      WriteToken(Ident_attr);
			  }
			  new_state(RULE1);
			}

<RULE1>{SPACE}+		{ ECHO; new_state(RULEX); }
<RULEX>{PATTERNS}	{ write_patterns(yytext, yyleng); new_state(RULER); }
<RULER>[^\r\n]*		{ WriteToken(Error_attr);
			  flt_error("Expected newline");
			}
<RULER>[\n]		{ ECHO; new_state(RULES); }

<ACTIONS>^{SPACE}+	{ ECHO; new_state(CODE1); }
<ACTIONS>{SPACE}+	{ ECHO; }
<ACTIONS>^{STATES}	{ write_states(yytext, yyleng); new_state(ACTION1); }
<ACTIONS>^{PATTERN}	{ write_patterns(yytext, yyleng); new_state(ACTION2); }

<ACTION0>^{SPACE}+	{ ECHO; }
<ACTION0>{STATES}	{ write_states(yytext, yyleng); new_state(ACTION1); }
<ACTION0>{PATTERN}	{ write_patterns(yytext, yyleng); new_state(ACTION2); }

<ACTION1>"{"		{ WriteToken(Action_attr); ++bracket; new_state(ACTION0); }
<ACTIONS,ACTION0>"}"	{ if (bracket) { --bracket; WriteToken(Action_attr); end_action(); } else ECHO; }
<ACTION1>{PATTERN}	{ write_patterns(yytext, yyleng); new_state(ACTION2); }
<ACTION1>{SPACE}+	{ ECHO; new_state(ACTION3); }
<ACTION1>[\n]		{ ECHO; end_action(); }

<ACTION2>{SPACE}+	{ ECHO; new_state(ACTION3); }
<ACTION2>[\n]		{ ECHO; end_action(); }

<ACTION3>{IDENT}	{ WriteToken(get_keyword_attr(yytext)); new_state(CODE1); }
<ACTION3>\{		{ ECHO; nesting = 1; new_state(CODE); }
<ACTION3>[\n]		{ ECHO; end_action(); }

<CODE,CODE1>{IDENT}	{ WriteToken(get_keyword_attr(yytext)); }
<CODE,CODE1>{STRINGS}	{ WriteToken(String_attr); }
<CODE,CODE1>{INTEGER}	{ WriteToken(Number_attr); }
<CODE1>[\n]		{ ECHO; set_state(); }
<CODE>\{		{ ECHO; nesting++; }
<CODE>\}		{ ECHO; if (--nesting <= 0) { nesting = 0; set_rules(); } }

<COMMENT>[^*]*		{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+[^*/]*	{ flt_bfr_append(yytext, yyleng); }
<COMMENT>"*"+"/"	{ PopQuote(); }

<RULES,CODE>^{SPACE}*#{SPACE}*{IDENT}({SPACE}+(\<[^>]+\>|\"[^"]+\"))? {
			  WriteToken(Preproc_attr);
			  if (FLT_STATE == RULES)
			      new_state(CODE);
			}

%%

#include <fltstack.h>

static void
end_action(void)
{
    if (bracket)
	new_state(ACTION0);
    else
	new_state(ACTIONS);
}

static void
set_state(void)
{
    if (section >= 2) {
	new_state(CODE);
    } else if (section >= 1) {
	end_action();
    } else {
	new_state(RULES);
    }
}

static void
set_rules(void)
{
    if (section >= 1)
	set_state();
}

static void
write_1state(char *text, int len)
{
    const char *attr = get_keyword_attr(text);
    if (attr == NULL) {
	if (len == 1 && *text == '*') {
	    attr = Keyword_attr;
	} else {
	    attr = Error_attr;
	    flt_error("Unknown state name \"%s\"", text);
	}
    }
    flt_bfr_embed(text, len, attr);
}

/*
 * FIXME: do this with lex states
 */
static void
write_states(char *text, int len)
{
    int n;
    char *next;
    char *last;

    if (text[0] == '<') {	/* only happens if we have {STATES} */
	flt_bfr_begin(Keyword_attr);
	flt_bfr_append(text, 1);
	++text;
	--len;

	if ((last = strchr(text, '>')) != NULL)
	    *last = 0;

	while (len >= 0 && (next = strchr(text, ',')) != NULL) {
	    *next = 0;
	    write_1state(text, (int) (next - text));
	    *next = ',';
	    len -= (int) (next - text);
	    text = next;

	    flt_bfr_append(text, 1);
	    ++text;
	    --len;
	}
	if (last != NULL) {	/* ...or is confused with a {PATTERN} */
	    n = (int) (1 + last - text);
	    write_1state(text, n - 1);
	    *last = '>';
	    flt_bfr_append(last, 1);
	}
	flt_bfr_finish();
    }
}

static int
ok_to_embed(char *text, int first, int last, int value)
{
    return ((value != first
	     && value < last
	     && text[value] == R_CURLY)
	    ? (value + 1)
	    : -1);
}

static int
parse_ident(char *text, int first, int last)
{
    int n;
    for (n = first; n < last; ++n) {
	int ch = CharOf(text[n]);
	int ok;
	if (n == first) {
	    ok = isalpha(ch);
	} else {
	    ok = isalnum(ch) || (ch == '_');
	}
	if (!ok)
	    break;
    }
    return ok_to_embed(text, first, last, n);
}

static int
parse_limits(char *text, int first, int last)
{
    int n;
    for (n = first; n < last; ++n) {
	int ch = CharOf(text[n]);
	int ok;
	if (n == first) {
	    ok = isdigit(ch);
	} else {
	    ok = isdigit(ch) || (ch == ',');
	}
	if (!ok)
	    break;
    }
    return ok_to_embed(text, first, last, n);
}

/*
 * FIXME: do this with lex states
 */
static void
write_patterns(char *text, int len)
{
    const char *attr;
    int quoted = 0;
    int escape = 0;
    int ranges = 0;
    int first, last, next;

    set_symbol_table(NAME_LEX_PATTERN);

    flt_bfr_begin(String_attr);
    for (first = last = 0; last < len; ++last) {
	int ch = CharOf(text[last]);

	if (escape) {
	    escape = 0;
	} else if (quoted) {
	    if (ch == DQUOTE)
		quoted = 0;
	} else if (ranges) {
	    if (ch == L_BLOCK) {
		++ranges;
	    } else if (ch == R_BLOCK) {
		--ranges;
	    }
	} else {
	    if (ch == '\\') {
		escape = 1;
	    } else if (ch == DQUOTE) {
		quoted = 1;
	    } else if (ch == L_BLOCK) {
		ranges = 1;
	    } else if (ch == L_CURLY) {
		if ((next = parse_ident(text, last + 1, len)) > 0) {
		    int save = text[next - 1];
		    text[next - 1] = 0;
		    /*
		     * flex accepts forward-references to names, but this
		     * is a one-pass highlighter and cannot tell if a failure
		     * is a forward reference.  But show an error anyway since
		     * it is more likely to be useful.
		     */
		    flt_bfr_append(text + first, last - first);
		    if ((attr = get_keyword_attr(text + last + 1)) == NULL) {
			attr = Error_attr;
			flt_error("Undefined name \"%s\"", text + last + 1);
		    }
		    text[next - 1] = (char) save;
		    flt_bfr_embed(text + last, next - last, attr);
		    first = next;
		} else if ((next = parse_limits(text, last + 1, len)) >= 0) {
		    flt_bfr_append(text + first, last - first);
		    flt_bfr_embed(text + last, next - last, Number_attr);
		    first = next;
		}
	    }
	}
    }
    flt_bfr_append(text + first, len - first);
    flt_bfr_finish();

    set_symbol_table(default_table);
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

    section = 0;
    nesting = 0;

    Action_attr = class_attr(NAME_ACTION);
    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Keyword_attr = class_attr(NAME_KEYWORD);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);
    String_attr = class_attr(NAME_LITERAL);

    if ((Section_attr = class_attr(NAME_LEX_SECTION)) == NULL)
	Section_attr = Keyword_attr;

    if ((States_attr = class_attr(NAME_LEX_STATES)) == NULL)
	insert_keyword(NAME_LEX_STATES, Keyword_attr, 0);

    flt_make_symtab(NAME_LEX_PATTERN);
    set_symbol_table(NAME_LEX_PATTERN);
    if ((Pattern_attr = class_attr(NAME_LEX_PATTERN)) == NULL)
	Pattern_attr = String_attr;

    set_symbol_table(default_table);

    begin_state(RULES);
    RunLEX();
    flt_bfr_error();
    end_state();
}

#if NO_LEAKS
static void
free_filter(void)
{
    USE_LEXFREE;
}
#endif
