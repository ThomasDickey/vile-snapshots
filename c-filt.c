/*
 * Program: A simple comment and keyword attributer for vile
 * Author : Jukka Keto, jketo@cs.joensuu.fi
 * Date   : 30.12.1994
 * Modifications:  kevin buettner and paul fox  2/95
 * 		string literal ("Literal") support --  ben stoltz
 *
 * $Header: /users/source/archives/vile.vcs/RCS/c-filt.c,v 1.12 1998/09/22 10:50:30 Gary.Ross Exp $
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
 *		    write-message "[Attaching C/C++ attributes...]"
 *		    set-variable %savcol $curcol
 *		    set-variable %savline $curline
 *		    set-variable %modified $modified
 *		    goto-beginning-of-file
 *		    filter-til end-of-file "c-filt"
 *		    goto-beginning-of-file
 *		    attribute-cntl_a-sequences-til end-of-file
 *		    ~if &not %modified
 *			    unmark-buffer
 *		    ~endif
 *		    %savline goto-line
 *		    %savcol goto-column
 *		    write-message "[Attaching C/C++ attributes...done ]"
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
 * Known Bugs (other features):
 *	- The keyword lists should be ordered for optimal operation.
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

#if MISSING_EXTERN_FPUTS
extern	int	fputs	( const char *s, FILE *fp );
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
#define MAX_ATTR_LENGTH 6

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

#define isBlank(c)  ((c) == ' ' || (c) == '\t')

typedef struct _keyword KEYWORD;

struct _keyword {
    char kw[MAX_KEYWORD_LENGTH+1];
    char attribute[MAX_ATTR_LENGTH+1];
    int  length;
    KEYWORD *next;
};

static KEYWORD *hashtable[HASH_LENGTH];
static KEYWORD identifier;
static char comment_attr[MAX_ATTR_LENGTH+1] = "C1"; /* color 1 */
static char literal_attr[MAX_ATTR_LENGTH+1] = "C2"; /* color 1 */
static char cpp_attr[MAX_ATTR_LENGTH+1]     = "C3"; /* color 3 */

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
    if (!strcmp(ident,"Cpp")) {
      strcpy(cpp_attr,attribute);
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
	} else if (identifier.kw[0] == '#' && hash_id->kw[0] == '#') {
	    char *s = &identifier.kw[1];
	    while (isBlank(*s))
	    	s++;

	    if (strcmp(&hash_id->kw[1],s) == 0) {
		match = 1;
		break;
	    }
	}
	hash_id = hash_id->next;
    }
    if (match)
	printf("\001%i%s:%s",identifier.length, hash_id->attribute,
				   identifier.kw);
    else
        printf("%s",identifier.kw);
}


static char *
extract_identifier(char *s)
{
    register char *kwp = identifier.kw;
    identifier.kw[0] = '\0';
    identifier.length = 0;
    if (*s == '#') {
	do {
		identifier.length += 1;
		*kwp++ = *s++;
	} while (isBlank(*s) &&
		(identifier.length < MAX_KEYWORD_LENGTH));
    }
    while ((isalpha(*s) || *s == '_' || isdigit(*s)) &&
           identifier.length < MAX_KEYWORD_LENGTH) {
	identifier.length += 1;
	*kwp++ = *s++;
    }
    *kwp = '\0';
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
	items = sscanf(line,"%[#a-zA-Z0-9_]:%[IURC0-9A-F]",ident,attribute);
	while (! feof(kwfile) ) {
#ifdef DEBUG
	    fprintf(stderr,"read_keywords: Items %i, kw = %s, attr = %s\n",items,ident,attribute);
#endif
	    if (items == 2)
		insert_keyword(ident,attribute);
	    fgets(line,MAX_LINELENGTH,kwfile);
	    items = sscanf(line,"%[#a-zA-Z0-9_]:%[IURC0-9A-F]",ident,attribute);
	}
	fclose(kwfile);
    }
}

static int
has_endofcomment(char *s)
{
    int i=0;
    while (*s) {
	if (*s == '*' && *(s+1) == '/') {
	    return(i+2);
	}
	i += 1;
	s += 1;
    }
    return(0);
}

static int
has_endofliteral(char *s)	/* points to '"' */
{
    int i=0;
    while (*s) {
	if (*s == '\"')
	    return (i);
	if (s[0] == '\\' && (s[1] == '\"' || s[1] == '\\')) {
		++i;
		++s;
	}
	++i;
	++s;
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

static int
firstnonblank(char *tst, char *cmp)
{
    while (*cmp && isBlank(*cmp))
	cmp++;
    return (tst == cmp);
}

static char *
write_literal(char *s, int *literal)
{
    int c_length = has_endofliteral(s);
    if (c_length == 0)
	c_length = strlen(s);
    else
	*literal = 0;
    printf("\001%i%s:%.*s", c_length, literal_attr, c_length, s);
    s += c_length;
    if (!*literal)
    	putchar(*s++);
    return s;
}

int
main(int argc, char **argv)
{
    char line[MAX_LINELENGTH+1];
    char *s;
    int comment,c_length,literal;

#if OPT_LOCALE
    setlocale(LC_CTYPE, "");
#endif

    comment = 0;
    literal = 0;
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
	if (literal)
	    s = write_literal(s, &literal);
	s = skip_white(s);
	while (*s) {
	    if (!comment && *s == '/' && *(s+1) == '*') {
		c_length = has_endofcomment(s+2);
		if (c_length == 0) { /* Comment continues to the next line */
		    c_length = strlen(s);
		    comment += 1;
		} else {
		    c_length += 2;
		}
		printf("\001%i%s:%.*s",c_length,comment_attr,c_length,s);
		s = s + c_length ;
	    }
	    if (!comment && *s == '/' && *(s+1) == '/') { /* C++ comments */
	        c_length = strlen(s);
		printf("\001%i%s:%.*s",c_length,comment_attr,c_length,s);
	      break;
	    }
	    if (!comment && *s == '#' && firstnonblank(s, line) ) {
		char *ss = s+1;
		int isinclude;
		while (isspace(*ss))
		    ss++;
		isinclude = !strncmp(ss, "include", 7);
		while (isalpha(*ss))
		    ss++;
		if (isinclude) {  /* eat filename as well */
		    while (isspace(*ss))
			ss++;
		    while (!isspace(*ss))
			ss++;
		}
		c_length = ss - s;
		printf("\001%i%s:%.*s",c_length,cpp_attr,c_length,s);
		s = s + c_length;
	    }
	    if (comment && *s) {
		if ((c_length = has_endofcomment(s)) > 0) {
		    printf("\001%i%s:%.*s",c_length,comment_attr,c_length,s);
		    s = s + c_length ;
		    comment -= 1;
		    if (comment < 0) comment = 0;
		} else { /* Whole line belongs to comment */
		    c_length = strlen(s);
		    printf("\001%i%s:%.*s",c_length,comment_attr,c_length,s);
		    s = s + c_length;
		}
	    } else if (*s) {
	        if (*s == '\\' && *(s+1) == '\"') {/* Skip literal single character */
		    putchar(*s++);
		    putchar(*s++);
		} else if (!literal && *s == '\"' && s[1] == '\"') {
		    putchar(*s++);
		    putchar(*s++);
		} else if (!literal && *s == '\'' && s[1] == '"' && s[2] == '\'') {
		    putchar(*s++);
		    putchar(*s++);
		    putchar(*s++);
		}
		if (*s == '\"')  {
		    literal = literal == 0 ? 1 : 0;
		    putchar(*s++);
		    if (literal) {
			s = write_literal(s, &literal);
		    }
		}

		if (*s) {
			if ( (isalpha(*s) || *s == '_' || *s == '#')
				&& ! literal) {
			    s = extract_identifier(s);
			    match_identifier();
			} else {
			    putchar(*s++);
			}
		}
	    }
	}
    }
    closehash();

    exit(0);
}
