%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/spellflt.l,v 1.6 2000/11/04 22:12:27 tom Exp $
 *
 * Filter to add vile "attribution" sequences to misspelled words.
 */

#include <filters.h>

DefineFilter("spell");

#if defined(ISPELL_PROG)
/* -l	Produce a list of misspelled words */
/* -x	Don't create a backup file. */
#define SPELL_PIPE "ispell -x -l"
#elif defined(SPELL_PROG)
#define SPELL_PIPE "spell -x -l"
#endif

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *input)
{
#ifdef SPELL_PIPE
    FILE *fp;
    char *name = tmpnam((char *)0);
    char buffer[BUFSIZ];
    int ch;
    char *attr = class_attr(NAME_KEYWORD);

    if ((fp = fopen(name, "w")) == 0)
	return;
    while ((ch = fgetc(input)) != EOF && !feof(input))
	fputc(ch, fp);
    fclose(fp);

    sprintf(buffer, "%s <%s", SPELL_PIPE, name);
    if ((fp = popen(buffer, "r")) != 0) {
	while (fgets(buffer, sizeof(buffer), fp)) {
	    unsigned len  = strlen(buffer);
	    while (len-- && isspace(CharOf(buffer[len])))
		buffer[len] = 0;
	    if (*buffer)
		insert_keyword(buffer, attr, 0);
	}
	pclose(fp);
    }
    yyin = fopen(name, "r");

    while (yylex() > 0) {
    }

    remove(name);	/* works on Unix, to remove file before closing */
#endif
}

%}

ALPHA		[a-zA-Z\240-\377]
DIGIT		[0-9]
WORD		{ALPHA}({ALPHA}|{DIGIT})*

%%

{WORD}		{ WriteToken(keyword_attr(yytext)); }
