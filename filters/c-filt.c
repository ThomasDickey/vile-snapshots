/*
 * Program: A simple comment and keyword attributer for vile
 * Author : Jukka Keto, jketo@cs.joensuu.fi
 * Date   : 30.12.1994
 * Modifications:  kevin buettner and paul fox  2/95
 * 		string literal ("Literal") support --  ben stoltz
 *		factor-out hashing and file I/O - tom dickey
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/c-filt.c,v 1.40 1999/12/14 02:53:05 tom Exp $
 *
 * Features:
 * 	- Reads the keyword file ".vile.keywords" from the home directory.
 *	  Keyword file consists lines "keyword:attribute" where
 *	  keyword is any alphanumeric string [#a-zA-Z0-9_] followed
 *	  by colon ":" and attribute character; "I" for italic,
 *	  "U" for underline, "B" for bold, "R" for reverse or
 *	  "C#" for color (where # is a single hexadecimal digit representing
 *	  one of 16 colors).
 *	- Attributes the file read from stdin using vile attribute sequences
 *	  and outputs the file to stdout with keywords and comments
 *	  attributed.
 *	- Normal C-Comments are handled by the pseudo-keyword "Comments".
 *	- "String literals" are handled by the pseudo-keyword "Literals".
 *	- #if, #include, etc. are handled by the pseudo-keyword "Cpp".
 *	- Here is a macro one might use to invoke the colorizer:
 *	    30 store-macro
 *		~local $curcol $curline
 *		~hidden goto-beginning-of-file
 *		~hidden attribute-from-filter end-of-file "vile-c-filt"
 *	    ~endm
 *	    bind-key execute-macro-30 ^X-q
 *
 * example .vile.keywords files:
 * (first, for color use)
 *	Comments:C1
 *	Literal:C1
 *	Cpp:C2
 *	if:C3
 *	else:C3
 *	for:C3
 *	return:C3
 *	while:C3
 *	switch:C3
 *	case:C3
 *	do:C3
 *	goto:C3
 *	break:C3
 *
 * (for non-color use)
 *	Comments:U
 *	Literal:U
 *	Cpp:I
 *	if:B
 *	else:B
 *	for:B
 *	return:B
 *	while:B
 *	switch:B
 *	case:B
 *	do:B
 *	goto:B
 *	break:B
 *
 * Note:
 *	- I use this to get the coloring effects in XVile, or when
 *	  using a terminal emulator which supports color.  some, for
 *	  instance, allow the mapping of bold and italic attributes to
 *	  color.
 *
 * Win32 Notes:
 *    1) Keywords are read from either $HOME\vile.keywords or
 *       .\vile.keywords .
 *
 *    2) The console and GUI versions of vile both support full use of
 *       16 colors.  The default color mapping (palette) is as follows:
 *
 *       C0:black       C1:red            C2:green        C3:brown
 *       C4:blue        C5:magenta        C6:cyan         C7:lightgray
 *       C8:gray        C9:brightred      CA:brightgreen  CB:yellow
 *       CC:brightblue  CD:brightmagenta  CE:brightcyan   CF:white
 *
 *    3) Note also that the user may specify the editor's foreground and
 *       background colors (:se fcolor, :se bcolor) as well as a
 *       foreground color for search matches (:se visual-matches).
 *
 *    Pulling 1-3 together, here is an example vile.rc file that
 *    sets the foreground color to white, background color to (dark) blue,
 *    and visual matches color to bright red:
 *
 *    vile.rc
 *    =======
      set bcolor=blue
      set fcolor=white
      set visual-matches=brightred

 *    And here is an example vile.keywords file that colors comments in
 *    yellow, C keywords in brightcyan, preprocesor directives in
 *    brightmagenta, and string constants in brightgreen.
 *
 *    vile.keywords
 *    =============
      Comments:CB
      Cpp:CD
      Literal:CA
      if:CE
      else:CE
      for:CE
      return:CE
      while:CE
      switch:CE
      case:CE
      do:CE
      goto:CE
      break:CE
 */

#include <filters.h>

char *filter_name = "c";

#define ESCAPE '\\'
#define DQUOTE '"'
#define SQUOTE '\''

#define isIdent(c)  (isalpha(c) || (c) == '_')
#define isNamex(c)  (isalnum(c) || (c) == '_')

#define isBlank(c)  ((c) == ' ' || (c) == '\t')
#define isQuote(c)  ((c) == DQUOTE || (c) == SQUOTE)

static char *Comment_attr;
static char *Error_attr;
static char *Ident_attr;
static char *Literal_attr;
static char *Number_attr;
static char *Preproc_attr;

static char *
extract_identifier(FILE * fp, char *s)
{
    static char *name;
    static unsigned have;

    unsigned need;
    char *attr;
    char *base = s;

    while (isNamex(*s))
	s++;
    if (base != s) {
	need = s - base;
	name = do_alloc(name, need, &have);
	strncpy(name, base, need);
	name[need] = 0;
	if ((attr = keyword_attr(name)) != 0) {
	    write_token(fp, base, need, attr);
	} else {
	    write_token(fp, base, need, Ident_attr);
	}
    }
    return s;
}

static int
has_endofcomment(char *s)
{
    int i = 0;
    while (*s) {
	if (*s == '*' && *(s + 1) == '/') {
	    return (i + 2);
	}
	i += 1;
	s += 1;
    }
    return (0);
}

static int
has_endofliteral(char *s, int delim)
{				/* points to '"' */
    int i = 0;
    while (*s) {
	if (*s == delim)
	    return (i);
	if (s[0] == ESCAPE && (s[1] == delim || s[1] == ESCAPE)) {
	    ++i;
	    ++s;
	}
	++i;
	++s;
    }
    return (-1);
}

static char *
skip_white(FILE * fp, char *s)
{
    while (*s && isBlank(*s))
	fputc(*s++, fp);
    return s;
}

static int
firstnonblank(char *tst, char *cmp)
{
    while (*cmp && isBlank(*cmp))
	cmp++;
    return (tst == cmp);
}

static char *
write_escape(FILE * fp, char *s, char *attr)
{
    char *base = s++;
    int want = (*s == '0' || *s == 'x') ? 3 : 1;
    while (want && *s != 0) {
	s++;
	want--;
    }
    write_string(fp, base, s - base, attr);
    return s;
}

static char *
write_number(FILE * fp, char *s)
{
    char *base = s;
    int radix = (*s == '0')
		? ((s[1] == 'x' || s[1] == 'X') ? 16
		: (!isdigit(s[1])) ? 10 : 8) : 10;
    int state = 0;
    int done = 0;
    int found = isdigit(*s) && (radix != 16);
    int num_f = 0;
    int num_u = 0;
    int num_l = 0;

    if (radix == 16 || (*s == '.'))
	s++;
    while (!done) {
	s++;
	switch (radix) {
	case 8:
	    done = !isdigit(*s) || (*s == '8') || (*s == '9');
	    break;
	case 10:
	    done = !isdigit(*s);
	    break;
	case 16:
	    done = !isxdigit(*s);
	    break;
	}
	found += !done;
    }
    if (radix == 10) {
	while (state >= 0) {
	    int ch = isalpha(*s) ? toupper(*s) : *s;
	    switch (state) {
	    case 0:
		if (ch == '.') {
		    state = 1;
		} else if (ch == 'E') {
		    state = 2;
		} else if (ch == 'F') {
		    num_f++;
		    state = 3;
		} else if (ch == 'L') {
		    num_l++;
		    state = 4;
		} else if (ch == 'U') {
		    num_u++;
		    state = 4;
		} else {
		    state = -1;
		}
		break;
	    case 1:		/* after decimal point */
		while (isdigit(*s))
		    s++;
		state = -1;
		if (ch == 'E') {
		    state = 2;
		} else if (ch == 'F') {
		    num_f++;
		    state = 3;
		} else if (ch == 'L') {
		    num_l++;
		    state = 3;
		}
		break;
	    case 2:		/* after exponent letter */
		if (ch == '+' || ch == '-')
		    s++;
		while (isdigit(*s))
		    s++;
		/* FALLTHRU */
	    case 3:
		if (ch == 'F') {
		    if (++num_f > 1)
			state = -1;
		} else if (ch == 'L') {
		    if (++num_l > 1)
			state = -1;
		} else {
		    state = -1;
		}
		break;
	    case 4:
		if (ch == 'L') {
		    if (++num_l > 2)
			state = -1;
		} else if (ch == 'U') {
		    if (++num_u > 1)
			state = -1;
		} else {
		    state = -1;
		}
		break;
	    }
	    if (state >= 0)
		s++;
	}
    } else {
	for (;;) {
	    int ch = isalpha(*s) ? toupper(*s) : *s;
	    if (ch == 'L') {
		if (++num_l > 2)
		    break;
	    } else if (ch == 'U') {
		if (++num_u > 1)
		    break;
	    } else {
		break;
	    }
	    s++;
	}
    }
    if (!found || isNamex(*s)) {	/* something is run-on to a number */
	while (isNamex(*s))
	    s++;
	write_string(fp, base, s - base, Error_attr);
    } else {
	write_token(fp, base, s - base, Number_attr);
    }
    return s;
}

static char *
write_literal(FILE * fp, char *s, int *literal)
{
    int c_length = has_endofliteral(s, *literal);
    if (c_length < 0)
	c_length = strlen(s);
    else
	*literal = 0;
    write_token(fp, s, c_length, Literal_attr);
    s += c_length;
    if (!*literal)
	fputc(*s++, fp);
    return s;
}

static char *
parse_prepro(FILE * fp, char *s)
{
    char *attr;
    char *ss = s + 1;
    char *tt;
    int c_length;
    int isinclude = 0;
    int save;

    while (isBlank(*ss))
	ss++;
    for (tt = ss; isNamex(*tt); tt++)
	/*LOOP */ ;
    save = *tt;
    *tt = 0;
    isinclude = !strcmp(ss, "include");
    if ((attr = keyword_attr(ss)) == 0) {
	write_string(fp, s, tt - s, Error_attr);
	s = tt;
    }
    *tt = save;

    if (isinclude) {		/* eat filename as well */
	while (isBlank(*tt))
	    tt++;
	ss = tt;
	while (!isspace(*tt) && *tt != 0) {
	    int ch = *tt++;
	    if (*ss == '<') {
		if (ch == '>')
		    break;
	    } else if (*ss == '"') {
		if (ch == '"' && (tt != ss + 1))
		    break;
	    } else {
		if (!isNamex(ch)) {	/* FIXME: should allow any macro */
		    tt--;
		    break;
		}
	    }
	}
    }
    c_length = tt - s;
    write_string(fp, s, c_length, Preproc_attr);
    return tt;
}

void
init_filter(int before GCC_UNUSED)
{
}

void
do_filter(FILE * input, FILE * output)
{
    static unsigned used;
    static char *line;

    char *s;
    int comment, c_length, literal;

    Comment_attr = class_attr(NAME_COMMENT);
    Error_attr = class_attr(NAME_ERROR);
    Ident_attr = class_attr(NAME_IDENT);
    Literal_attr = class_attr(NAME_LITERAL);
    Number_attr = class_attr(NAME_NUMBER);
    Preproc_attr = class_attr(NAME_PREPROC);

    comment = 0;
    literal = 0;

    while (readline(input, &line, &used) != NULL) {
	s = line;
	if (literal)
	    s = write_literal(output, s, &literal);
	s = skip_white(output, s);
	while (*s) {
	    if (!comment && *s == '/' && *(s + 1) == '*') {
		c_length = has_endofcomment(s + 2);
		if (c_length == 0) {	/* Comment continues to the next line */
		    c_length = strlen(s);
		    comment += 1;
		} else {
		    c_length += 2;
		}
		write_token(output, s, c_length, Comment_attr);
		s = s + c_length;
		continue;
	    } else if (!comment && *s == '/' && *(s + 1) == '/') {
		/* C++ comments */
		c_length = strlen(s);
		write_token(output, s, c_length, Comment_attr);
		break;
	    } else if (!comment && *s == '#' && firstnonblank(s, line)
		&& set_symbol_table("cpre")) {
		s = parse_prepro(output, s);
		set_symbol_table(filter_name);
	    } else if (comment && *s) {
		if ((c_length = has_endofcomment(s)) > 0) {
		    write_token(output, s, c_length, Comment_attr);
		    s = s + c_length;
		    comment -= 1;
		    if (comment < 0)
			comment = 0;
		} else {	/* Whole line belongs to comment */
		    c_length = strlen(s);
		    write_token(output, s, c_length, Comment_attr);
		    s = s + c_length;
		}
	    } else if (*s == ESCAPE) {
		if (s[1] != '\n') {
		    s = write_escape(output, s, Error_attr);
		} else {
		    /* escaped newline - ok */
		    fputc(*s++, output);
		}
	    } else if (isQuote(*s)) {
		literal = (literal == 0) ? *s : 0;
		fputc(*s++, output);
		if (literal) {
		    s = write_literal(output, s, &literal);
		}
	    } else if (isIdent(*s)) {
		s = extract_identifier(output, s);
	    } else if (isdigit(*s) || (*s == '.' && isdigit(s[1]))) {
		s = write_number(output, s);
	    } else if (*s == '#') {
		char *t = s;
		while (*s == '#')
		    s++;
		write_string(output, t, s - t, ((s - t) > 2) ?
		    Error_attr : Preproc_attr);
	    } else {
		fputc(*s++, output);
	    }
	}
    }
}
