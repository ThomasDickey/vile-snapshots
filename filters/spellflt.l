%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/spellflt.l,v 1.15 2005/02/12 13:12:56 tom Exp $
 *
 * Filter to add vile "attribution" sequences to misspelled words.
 */

#ifdef filter_def
#include <estruct.h>
#include <edef.h>
#endif

#include <filters.h>

DefineFilter("spell");

#if defined(ISPELL_PROG)
/* -l	Produce a list of misspelled words */
/* -x	Don't create a backup file. */
#define SPELL_PIPE "ispell -x -l"
#else
#define SPELL_PIPE "spell -l"
#endif

static FILE *
open_tempfile(char *fname)
{
    FILE *fp = 0;
#ifdef HAVE_MKSTEMP
    int fd = mkstemp(strcpy(fname, "vileXXXXXX"));
    if (fd >= 0)
    	fp = fdopen(fd, "w");
#else
    int oldmask = umask(0077);
    strcpy(fname, tmpnam((char *)0));
    fp = fopen(fname, "w");
    umask(oldmask);
#endif
    return fp;
}

static void
init_filter(int before GCC_UNUSED)
{
}

static void
do_filter(FILE *inputs GCC_UNUSED)
{
#ifdef filter_def
    LINE *lp;
#else
    int ch;
#endif
    char name[256];
    FILE *fp;
    char buffer[BUFSIZ];
    char *attr = class_attr(NAME_ERROR);
    char *prog;

    if ((fp = open_tempfile(name)) == 0)
	return;
#ifdef filter_def		/* built-in filter? */
    ffp = fp;
#if OPT_ENCRYPT
    ffstatus = file_is_pipe;
#endif
    for_each_line(lp, curbp) {
	ffputline(lp->l_text, llength(lp), "\n");
    }
#else
    while ((ch = fgetc(inputs)) != EOF && !feof(inputs))
	fputc(ch, fp);
#endif
    fclose(fp);

#ifdef filter_def		/* built-in filter? */
    ffstatus = file_is_closed;
    ffp = 0;
#endif

    if ((prog = getenv("VILE_SPELL_FILT")) == 0)
    	prog = SPELL_PIPE;
    sprintf(buffer, "%s <%s", prog, name);
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
}

%}

ALPHA		[a-zA-Z\240-\377]
DIGIT		[0-9]
WORD		{ALPHA}({ALPHA}|{DIGIT})*

%%

{WORD}		{ char *attr = keyword_attr(yytext);
		  if (attr != 0)
		      flt_error(yytext);
		  WriteToken(attr);
		}
