/*
 * Main program and I/O for external vile syntax/highlighter programs
 *
 * $Header: /users/source/archives/vile.vcs/RCS/builtflt.c,v 1.3 2000/03/18 01:40:12 tom Exp $
 *
 */

#include <estruct.h>

#include <builtflt.h>
#include <stdarg.h>

#ifdef _builtflt_h

static FILE *my_out;

static FILTER_DEF *current_filter;

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

void
flt_echo(char *string, int length)
{
    fprintf(my_out, "%.*s", length, string);
}

void
flt_failed(const char *msg)
{
    mlforce("[Filter: %s]", msg);
}

int
flt_input(char *buffer, int max_size)
{
    return 0;
}

char *
flt_name(void)
{
    return "";
}

void
flt_putc(int ch)
{
    fputc(ch, my_out);
}

void
flt_puts(char *string, int length, char *marker)
{
    if (length > 0) {
	if (marker != 0 && *marker != 0 && *marker != 'N')
	    fprintf(my_out, "%c%i%s:", CTL_A, length, marker);
	fprintf(my_out, "%.*s", length, string);
    }
}

int
flt_start(char *name)
{
    unsigned n;

    for (n = 0; n < TABLESIZE(builtflt); n++) {
	if (!strcmp(name, builtflt[n]->filter_name)) {
	    current_filter = builtflt[n];
	    return TRUE;
	}
    }
    return FALSE;
}
#else

extern void builtflt(void);
void builtflt(void) { }

#endif /* _builtflt_h */
