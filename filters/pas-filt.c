/*
 * Program: A simple comment and keyword attributer for vile
 * Author : Jukka Keto, jketo@cs.joensuu.fi
 * Date   : 30.12.1994
 * Modifications:  kevin buettner and paul fox  2/95
 * 		string literal ("Literal") support --  ben stoltz
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/pas-filt.c,v 1.7 1998/12/22 02:45:28 tom Exp $
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
 *	- Comments are handled by the pseudo-keyword "Comments".
 *	- "String literals" are handled by the pseudo-keyword "Literals".
 *	- Here is a macro one might use to invoke the colorizer:
 *	    30 store-macro
 *		write-message "[Attaching Pascal attributes...]"
 *		~local $curcol $curline
 *		~hidden goto-beginning-of-file
 *		~hidden attribute-from-filter end-of-file "vile-pas-filt"
 *	    ~endm
 *	    bind-key execute-macro-30 ^X-q
 */

#include <filters.h>

char *filter_name = "pas";

#define isNameBegin(c)   (isalpha(c) || (c) == '_')
#define isNameExtra(c)   (isalnum(c) || (c) == '_')
#define isBlank(c)  ((c) == ' ' || (c) == '\t')

#define L_CURL '{'
#define R_CURL '}'
#define QUOTE  '\''

static char *comment_attr;
static char *literal_attr;

static char *
extract_identifier(FILE *fp, char *s)
{
    static char *name;
    static unsigned have;

    unsigned need;
    char *base;

    base = s;
    while (isNameExtra(*s))
	s++;

    if (base != s) {
	need = (s - base);
	name = do_alloc(name, need, &have);
	for (s = base; isNameExtra(*s); s++) {
	    name[s-base] = (isalpha(*s) && isupper(*s))
			    ? tolower(*s)
			    : *s;
	}
	name[s - base] = 0;
	write_token(fp, base, s - base, keyword_attr(name));
    }
    return s;
}

static int
has_endofcomment(char *s)
{
    int i=0;
    while (*s) {
	if (*s == R_CURL) {
	    return(i+1);
	}
	i += 1;
	s += 1;
    }
    return(0);
}

static int
has_endofliteral(char *s)	/* points past beginning QUOTE */
{
    int i = 0;

    while (s[i]) {
	if (s[i] == QUOTE) {
	    if (s[i+1] == QUOTE) {
		i += 2;
	    } else {
	        return (i);	/* points before ending QUOTE */
	    }
	}
	++i;
    }
    return(0);
}

static char *
skip_white(FILE *fp, char *s)
{
    while(*s && isBlank(*s))
    	fputc(*s++, fp);
    return s;
}

static char *
write_literal(FILE *fp, char *s)
{
    int c_length = has_endofliteral(s);
    if (c_length == 0)
	c_length = strlen(s);
    write_token(fp, s, c_length, literal_attr);
    s += c_length;
    if (*s == QUOTE)
    	fputc(*s++, fp);
    return s;
}

void
init_filter(void)
{
}

void
do_filter(FILE *input, FILE *output)
{
    static unsigned used;
    static char *line;
    char *s;
    int comment,c_length;

    comment_attr = keyword_attr(NAME_COMMENT);
    literal_attr = keyword_attr(NAME_LITERAL);
    comment = 0;

    while (readline(input, &line, &used) != NULL) {
	s = line;
	s = skip_white(output, s);
	while (*s) {
	    if (!comment && *s == L_CURL) {
		c_length = has_endofcomment(s);
		if (c_length == 0) { /* Comment continues to the next line */
		    c_length = strlen(s);
		    comment = 1;
		}
		write_token(output, s, c_length, comment_attr);
		s = s + c_length ;
	    }
	    if (comment && *s) {
		if ((c_length = has_endofcomment(s)) > 0) {
		    write_token(output, s, c_length, comment_attr);
		    s = s + c_length ;
		    comment = 0;
		} else { /* Whole line belongs to comment */
		    c_length = strlen(s);
		    write_token(output, s, c_length, comment_attr);
		    s = s + c_length;
		}
	    } else if (*s == QUOTE)  {
		fputc(*s++, output);
		s = write_literal(output, s);
	    } else if (*s) {
		if ( isNameBegin(*s) ) {
		    s = extract_identifier(output, s);
		} else {
		    fputc(*s++, output);
		}
	    }
	}
    }
}
