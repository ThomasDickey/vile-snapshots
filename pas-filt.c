/*
 * Program: A simple comment and keyword attributer for vile
 * Author : Jukka Keto, jketo@cs.joensuu.fi
 * Date   : 30.12.1994
 * Modifications:  kevin buettner and paul fox  2/95
 * 		string literal ("Literal") support --  ben stoltz
 *
 * $Header: /users/source/archives/vile.vcs/RCS/pas-filt.c,v 1.2 1998/05/19 19:24:11 tom Exp $
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
 *		    write-message "[Attaching Pascal attributes...]"
 *		    set-variable %savcol $curcol
 *		    set-variable %savline $curline
 *		    set-variable %modified $modified
 *		    goto-beginning-of-file
 *		    filter-til end-of-file "pas-filt"
 *		    goto-beginning-of-file
 *		    attribute-cntl_a-sequences-til end-of-file
 *		    ~if &not %modified
 *			    unmark-buffer
 *		    ~endif
 *		    %savline goto-line
 *		    %savcol goto-column
 *		    write-message "[Attaching Pascal attributes...done ]"
 *	    ~endm
 *	    bind-key execute-macro-30 ^X-q
 *
 * Here's the .vile.keywords file which I used to test:
Comments:C2
Literal:U
goto:C2
and:B
begin:B
case:B
case:B
constructor:B
do:B
else:B
end:B
function:B
if:B
not:B
of:B
or:B
procedure:B
program:B
record:B
repeat:B
then:B
type:B
unit:B
until:B
uses:B
while:B
with:B
array:C4
array:C4
boolean:C4
byte:C4
char:C4
const:C4
const:C4
file:C4
integer:C4
packed:C4
pointer:C4
real:C4
set:C4
var:C4
word:C4
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
/* assume ANSI C */
# define HAVE_STDLIB_H 1
# define HAVE_STRING_H 1
#endif

#include <sys/types.h>		/* sometimes needed to get size_t */

#if HAVE_STDLIB_H
#include <stdlib.h>
#else
# if !defined(HAVE_CONFIG_H) || MISSING_EXTERN_MALLOC
extern	char *	malloc	( size_t len );
# endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#if MISSING_EXTERN__FILBUF
extern	int	_filbuf	( FILE *fp );
#endif

#if MISSING_EXTERN__FLSBUF
extern	int	_flsbuf	( int len, FILE *fp );
#endif

#if MISSING_EXTERN_FCLOSE
extern	int	fclose	( FILE *fp );
#endif

#if MISSING_EXTERN_FPRINTF
extern	int	fprintf	( FILE *fp, const char *fmt, ... );
#endif

#if MISSING_EXTERN_PRINTF
extern	int	printf	( const char *fmt, ... );
#endif

#if MISSING_EXTERN_SSCANF
extern	int	sscanf	( const char *src, const char *fmt, ... );
#endif

#if OPT_LOCALE
#include <locale.h>
#endif
#include <ctype.h>

#define MAX_KEYWORD_LENGTH 80
#define HASH_LENGTH 256
#define MAX_LINELENGTH 256
#define MAX_ATTR_LENGTH 3

#ifdef _WIN32
static char *keyword_file="vile.keywords";
#define PATHSEP '\\'
#else
# if __GO32__
#define PATHSEP '\\'
static char *keyword_file="vile.key";
# else
#define PATHSEP '/'
static char *keyword_file=".vile.keywords";
# endif
#endif

#define isNameBegin(c)   (isalpha(c) || (c) == '_')
#define isNameExtra(c)   (isalnum(c) || (c) == '_')
#define isBlank(c)  ((c) == ' ' || (c) == '\t')

#define L_CURL '{'
#define R_CURL '}'
#define QUOTE  '\''

typedef struct _keyword KEYWORD;

struct _keyword {
    char kw[MAX_KEYWORD_LENGTH+1];	/* stores lowercase keyword */
    char ow[MAX_KEYWORD_LENGTH+1];	/* stores original keyword */
    char attribute[MAX_ATTR_LENGTH+1];
    int  length;
    KEYWORD *next;
};

static KEYWORD *hashtable[HASH_LENGTH];
static KEYWORD identifier;
static char comment_attr[MAX_ATTR_LENGTH+1] = "C1"; /* color 1 */
static char literal_attr[MAX_ATTR_LENGTH+1] = "C2"; /* color 1 */

static void
inithash(void)
{
    int i;
    for (i=0;i<HASH_LENGTH;i++) hashtable[i] = NULL;
}

static void
removelist(KEYWORD *k)
{
    if (k != NULL) {
	if (k->next != NULL) removelist(k->next);
	free((char *)k);
    }
}

static void
closehash(void)
{
    int i;
    for (i=0;i<HASH_LENGTH;i++) {
	removelist(hashtable[i]);
	hashtable[i] = NULL; /* For unseen future i do this */
    }
}

static int
hash_function(char *id)
{
    /*
     * Build more elaborate hashing scheme. If you want one.
     */
    return ( (int) *id );
}

static void
insert_keyword(
    char *ident,
    char *attribute)
{
    KEYWORD *first;
    KEYWORD *nxt;
    int Index;
    if (!strcmp(ident,"Comments")) {
	(void)strcpy(comment_attr,attribute);
	return;
    }
    if (!strcmp(ident,"Literal")) {
	strcpy(literal_attr,attribute);
	return;
    }
    nxt = first = NULL;
    Index = hash_function(ident);
    first = hashtable[Index];
    if ((nxt = (KEYWORD *)malloc(sizeof(struct _keyword))) != NULL) {
	(void)strcpy(nxt->kw,ident);
	nxt->length = strlen(nxt->kw);
	(void)strcpy(nxt->attribute,attribute);
	nxt->next = first;
	hashtable[Index] = nxt;
#ifdef DEBUG
    fprintf(stderr,"insert_keyword: new %li, new->kw %s, new->length %i, new->attribute %c, new->next %li\n", new,
    				    nxt->kw, nxt->length, nxt->attribute,nxt->next);
#endif
    }
}


static void
match_identifier(void)
{
    KEYWORD *hash_id;
    int Index, match = 0;

    Index = hash_function(identifier.kw);
    hash_id = hashtable[Index];

    while (hash_id != NULL) {
	if (hash_id->length == identifier.length) { /* Possible match */
	    if (strcmp(hash_id->kw,identifier.kw) == 0) {
		match = 1;
		break;
	    }
	}
	hash_id = hash_id->next;
    }
    if (match)
	printf("\001%i%s:", identifier.length, hash_id->attribute);
    printf("%s",identifier.ow);
}


static char *
extract_identifier(char *s)
{
    register char *kwp = identifier.kw;
    register char *owp = identifier.ow;

    identifier.length = 0;

    while ((isNameExtra(*s)) &&
           identifier.length < MAX_KEYWORD_LENGTH) {
	identifier.length += 1;
	if (isalpha(*s) && isupper(*s))
	    *kwp++ = tolower(*s);
	else
	    *kwp++ = *s;
	*owp++ = *s++;
    }
    *kwp = '\0';
    *owp = '\0';
    return(s);
}

static FILE *
open_keywords(char *filename)
{
    char *home = getenv("HOME");
    char fullname[1024];

    if (home == 0)
    	home = "";
    sprintf(fullname, "%s%c%s", home, PATHSEP, filename);
    return fopen(fullname, "r");
}

static void
read_keywords(char *filename)
{
    char ident[MAX_KEYWORD_LENGTH+1];
    char line[MAX_LINELENGTH+1];
    char attribute[MAX_ATTR_LENGTH+1];
    int  items;
    FILE *kwfile;

    if ((kwfile = open_keywords(filename)) != NULL) {
	fgets(line,MAX_LINELENGTH,kwfile);
	items = sscanf(line,"%[#a-zA-Z0-9_]:%[IUBR]",ident,attribute);
	if (items != 2)
	    items = sscanf(line,"%[#a-zA-Z0-9_]:%[C0-9ABCDEF]",ident,attribute);
	while (! feof(kwfile) ) {
#ifdef DEBUG
	    fprintf(stderr,"read_keywords: Items %i, kw = %s, attr = %s\n",items,ident,attribute);
#endif
	    if (items == 2)
		insert_keyword(ident,attribute);
	    fgets(line,MAX_LINELENGTH,kwfile);
	    items = sscanf(line,"%[#a-zA-Z0-9_]:%[IUBR]",ident,attribute);
	    if (items != 2)
		items = sscanf(line,"%[#a-zA-Z0-9_]:%[C0-9ABCDEF]",ident,attribute);
	}
	fclose(kwfile);
    }
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
skip_white(char *s)
{
    while(*s && isBlank(*s))
    	putchar(*s++);
    return s;
}

static char *
write_literal(char *s)
{
    int c_length = has_endofliteral(s);
    if (c_length == 0)
	c_length = strlen(s);
    printf("\001%i%s:%.*s", c_length, literal_attr, c_length, s);
    s += c_length;
    if (*s == QUOTE)
    	putchar(*s++);
    return s;
}

int
main(int argc, char **argv)
{
    char line[MAX_LINELENGTH+1];
    char *s;
    int comment,c_length;

#if OPT_LOCALE
    setlocale(LC_CTYPE, "");
#endif

    comment = 0;
    inithash();

    if (argc > 1) {
	int n;
	for (n = 1; n < argc; n++)
	    read_keywords(argv[n]);
    } else {
	read_keywords(keyword_file);
    }

    while (fgets(line,MAX_LINELENGTH,stdin) != NULL) {
	s = line;
	s = skip_white(s);
	while (*s) {
	    if (!comment && *s == L_CURL) {
		c_length = has_endofcomment(s);
		if (c_length == 0) { /* Comment continues to the next line */
		    c_length = strlen(s);
		    comment = 1;
		}
		printf("\001%i%s:%.*s",c_length,comment_attr,c_length,s);
		s = s + c_length ;
	    }
	    if (comment && *s) {
		if ((c_length = has_endofcomment(s)) > 0) {
		    printf("\001%i%s:%.*s",c_length,comment_attr,c_length,s);
		    s = s + c_length ;
		    comment = 0;
		} else { /* Whole line belongs to comment */
		    c_length = strlen(s);
		    printf("\001%i%s:%.*s",c_length,comment_attr,c_length,s);
		    s = s + c_length;
		}
	    } else if (*s == QUOTE)  {
		putchar(*s++);
		s = write_literal(s);
	    } else if (*s) {
		if ( isNameBegin(*s) ) {
		    s = extract_identifier(s);
		    match_identifier();
		} else {
		    putchar(*s++);
		}
	    }
	}
    }
    closehash();

    exit(0);
}
