/*
 * Program: A simple comment and keyword attributer for vile
 * Author : Jukka Keto, jketo@cs.joensuu.fi
 * Date   : 30.12.1994
 * Modifications:  kevin buettner and paul fox  2/95
 * 		string literal ("Literal") support --  ben stoltz
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_STDLIB_H
# define HAVE_STDLIB_H 0
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

#include <ctype.h>


#define MAX_KEYWORD_LENGTH 80
#define HASH_LENGTH 256
#define MAX_LINELENGTH 256
#define MAX_ATTR_LENGTH 3
static char *keyword_file=".vile.keywords";

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

static	void	inithash		(void);
static	void	removelist		(KEYWORD *k);
static	void	closehash		(void);
static	int	hash_function		(char *id);
static	void	insert_keyword		(char *ident, char *attrib);
static	void	match_identifier	(void);
static	char *	extract_identifier	(char *s);
static	void	read_keywords		(void);
static	int	has_endofcomment	(char *s);
static	int	has_endofliteral	(char *s);
static	char *	skip_white		(char *s);
extern	int	main			(int argc, char **argv);

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
    KEYWORD *new;
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
    new = first = NULL;
    Index = hash_function(ident);
    first = hashtable[Index];
    if ((new = (KEYWORD *)malloc(sizeof(struct _keyword))) != NULL) {
	(void)strcpy(new->kw,ident);
	new->length = strlen(new->kw);
	(void)strcpy(new->attribute,attribute);
	new->next = first;
	hashtable[Index] = new;
#ifdef DEBUG 
    fprintf(stderr,"insert_keyword: new %li, new->kw %s, new->length %i, new->attribute %c, new->next %li\n", new,
    				    new->kw, new->length, new->attribute,new->next);
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
	    while (*s == ' ' || *s == '\t')
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
	} while ((*s == ' ' || *s == '\t') &&
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


static void
read_keywords(void)
{
    char filename[1024];
    char ident[MAX_KEYWORD_LENGTH+1];
    char line[MAX_LINELENGTH+1];
    char attribute[MAX_ATTR_LENGTH+1];
    char *home;
    int  items;
    FILE *kwfile;
    home = getenv("HOME");
    sprintf(filename,"%s/%s",(home == NULL ? "" : home),keyword_file);
    if ((kwfile = fopen(filename,"r")) != NULL) {
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
    char i=0;
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
    char i=0;
    while (*s) {
	if (*s == '\"')
	    return (i);
	if (s[0] == '\\' && s[1] == '\"') {
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
    while(*s && (*s == ' ' || *s == '\t')) putchar(*s++);
    return s;
}

int 
main(int argc, char **argv)
{
    char line[MAX_LINELENGTH+1];
    char *s;
    int comment,c_length,literal;
    comment = 0;
    literal = 0;
    inithash();
    read_keywords();
    while (fgets(line,MAX_LINELENGTH,stdin) != NULL) {
	s = line;
	s = skip_white(s);
	while (*s) {
	    if (*s == '/' && *(s+1) == '*') {
		c_length = has_endofcomment(s);
		if (c_length == 0) { /* Comment continues to the next line */
		    c_length = strlen(s);
		    comment += 1;
		}
		printf("\001%i%s:%.*s",c_length,comment_attr,c_length,s);
		s = s + c_length ;
	    } 
	    if (*s == '/' && *(s+1) == '/') { /* C++ comments */
	        c_length = strlen(s);
		printf("\001%i%s:%.*s",c_length,comment_attr,c_length,s);
	      break;
	    } 
	    if (*s == '#' && s == line ) {
		c_length = strlen(s);
		printf("\001%i%s:%.*s",c_length,cpp_attr,c_length,s);
		break;
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
			c_length = has_endofliteral(s);
			if (c_length == 0)
			    c_length = strlen(s);
			else
			    literal = 0;
			printf("\001%i%s:%.*s",
			    c_length, literal_attr,c_length,s);
			s += c_length;
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
