%{

/*
 * $Header: /users/source/archives/vile.vcs/filters/RCS/spellflt.l,v 1.19 2005/03/11 00:23:11 tom Exp $
 *
 * Filter to add vile "attribution" sequences to misspelled words.
 */

#ifdef filter_def
#include <estruct.h>
#include <edef.h>
#endif

#include <filters.h>

DefineFilter("spell");

#define BUFFER_SIZE	BUFSIZ

/* console applications for win32 have popen, GUI applications do not */
#if defined(_WIN32) && !defined(popen)
#define popen _popen
#define pclose fclose
#endif

#if defined(ISPELL_PROG)
/* -l	Produce a list of misspelled words */
/* -x	Don't create a backup file. */
#define SPELL_PIPE "ispell -x -l"
#else
#define SPELL_PIPE "spell -l"
#endif

#ifdef HAVE_POPEN
#define pipe_open(command)	popen(command, "r")
#define	pipe_read(b,s)		fgets(b, sizeof(b), s)
#define pipe_close(s)		pclose(s)
#else				/* platforms with no workable popen */
#ifdef filter_def		/* built-in filter? */
#define HAVE_POPEN		/* we can fake it */
static FILE *
pipe_open(const char *command)
{
    FILE *result = 0;
    char buffer[BUFFER_SIZE + 2];

    sprintf(buffer, "!%s", command);
    if (ffropen(buffer) == FIOSUC)
	result = ffp;

    return result;
}

static char *
pipe_read(char *buffer, FILE *s)
{
    static char lastdata[BUFFER_SIZE];
    static int lastsize;

    char *result = buffer;
    int len;
    int done = 0;

    (void) s;
    do {
	if (lastsize != 0) {
	    int used = 0;
	    for (len = 0; len < lastsize; ++len) {
		if (done) {
		    lastdata[len - used] = lastdata[len];
		} else {
		    if ((buffer + 1 - result) >= BUFFER_SIZE
		    	|| (*buffer++ = lastdata[len]) == '\n') {
			done = 1;
		    }
		    used = len + 1;
		}
	    }
	    if (done) {
		lastsize -= used;
	    } else {
		lastsize = 0;
	    }
	}

	if (!done) {
	    lastsize = ffread(lastdata, sizeof(lastdata));
	    done = (lastsize <= 0);
	}
    } while (!done);

    *buffer = 0;
    if (buffer == result)
    	result = 0;

    return result;
}

static void
pipe_close(FILE *s)
{
    (void) s;
    ffclose();
}
#endif				/* filter_def */
#endif				/* HAVE_POPEN */

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
#ifdef HAVE_POPEN
#ifdef filter_def
    LINE *lp;
#else
    int ch;
#endif
    char name[256];
    FILE *fp;
    char buffer[BUFFER_SIZE + 2];
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
#else				/* external filter */
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
    if ((fp = pipe_open(buffer)) != 0) {
	while (pipe_read(buffer, fp)) {
	    unsigned len  = strlen(buffer);
	    while (len-- && isspace(CharOf(buffer[len])))
		buffer[len] = 0;
	    if (*buffer)
		insert_keyword(buffer, attr, 0);
	}
	pipe_close(fp);
    }
    yyin = fopen(name, "r");

    while (yylex() > 0) {
    }

    fclose(yyin);
    remove(name);
#endif /* HAVE_POPEN */
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
