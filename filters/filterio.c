/*
 * Main program and I/O for external vile syntax/highlighter programs
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/filterio.c,v 1.12 2000/08/28 23:53:17 tom Exp $
 *
 */

#include <filters.h>
#include <stdarg.h>

static FILE *my_out;
static FILE *my_in;
static int quit;
static int k_used;

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

static int
ProcessArgs(int argc, char *argv[], int flag)
{
    int n;
    char *s, *value;

    for (n = 1; n < argc; n++) {
	s = argv[n];
	if (*s == '-') {
	    while (*++s) {
		switch (*s) {
		case 'k':
		    value = s[1] ? s + 1 : ((n < argc) ? argv[++n] : "");
		    if (flag) {
			flt_read_keywords(value);
			k_used++;
		    }
		    break;
		case 'v':
		    if (!flag)
			verbose_flt++;
		    break;
		case 'q':
		    quit = 1;	/* 
				 * quit before filter parses data.  useful
				 * in conjunction with -v or -vv.
				 */
		    break;
		default:
		    fprintf(stderr, "unknown option %c\n", *s);
		    exit(BADEXIT);
		}
	    }
	} else {
	    break;
	}
    }
    return n;
}

/******************************************************************************
 * Public functions                                                           *
 ******************************************************************************/

char *
flt_gets(char **ptr, unsigned *len)
{
    return readline(my_in, ptr, len);
}

char *
flt_name(void)
{
    return filter_def.filter_name;
}

void
flt_putc(int ch)
{
    fputc(ch, my_out);
    if (ch == CTL_A)
	fputc('?', my_out);
}

void
flt_puts(char *string, int length, char *marker)
{
    if (length > 0) {
	if (marker != 0 && *marker != 0 && *marker != 'N')
	    fprintf(my_out, "%c%i%s:", CTL_A, length, marker);
	while (length-- > 0)
	    flt_putc(*string++);
    }
}

char *
home_dir(void)
{
    char *result;
#if defined(VMS)
    if ((result = getenv("SYS$LOGIN")) == 0)
	result = getenv("HOME");
#else
    result = getenv("HOME");
#if defined(_WIN32)
    if (result != 0 && strchr(result, ':') == 0) {
	static char desktop[256];
	char *drive = getenv("HOMEDRIVE");
	if (drive != 0) {
	    sprintf(desktop, "%s%s", drive, result);
	    result = desktop;
	}
    }
#endif
#endif
    return result;
}

void
mlforce(const char *fmt,...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

char *
skip_blanks(char *src)
{
    while (isspace(*src))
	src++;
    return (src);
}

char *
strmalloc(const char *src)
{
    register char *ns = (char *) malloc(strlen(src) + 1);
    if (ns != 0)
	(void) strcpy(ns, src);
    return ns;
}

/******************************************************************************
 * Stubs for vile-vile-filt                                                   *
 ******************************************************************************/
/*
 * The builtin-filter version of vilemode can check for unknown keywords.
 * These stubs correspond to the pre-9.1t behavior, which assumes that
 * we'll highlight anything that looks like a directive or a function, but
 * leave other keywords alone.
 */
/* ARGSUSED */
DIRECTIVE dname_to_dirnum(const char *s GCC_UNUSED, size_t len GCC_UNUSED)
{
    return 0;
}

/* ARGSUSED */
const CMDFUNC * engl2fnc(const char *fname GCC_UNUSED)
{
    return 0;
}

/* ARGSUSED */
int vl_lookup_func(const char *name GCC_UNUSED)
{
	return 0;
}

/******************************************************************************
 * Standalone main-program                                                    *
 ******************************************************************************/
int
main(int argc, char **argv)
{
    int n;

#if OPT_LOCALE
    setlocale(LC_CTYPE, "");
#endif

    my_in = stdin;
    my_out = stdout;

    /* get verbose option */
    (void) ProcessArgs(argc, argv, 0);

    flt_make_symtab(filter_def.filter_name);

    default_attr = strmalloc(NAME_KEYWORD);

    filter_def.InitFilter(1);

    flt_read_keywords(MY_NAME);
    n = ProcessArgs(argc, argv, 1);

    vile_keywords = !k_used;
    if (!k_used) {
	if (strcmp(MY_NAME, filter_def.filter_name)) {
	    flt_read_keywords(filter_def.filter_name);
	}
    }
    set_symbol_table(filter_def.filter_name);

    filter_def.InitFilter(0);

    if (quit) {
	/* 
	 * When the filter is called, we want to force it to print out its
	 * class info and then immediately exit.  Easiest way to do this
	 * is to connect the filter's input to /dev/null.
	 *
	 * To date, syntax filtering only works well/reliably on win32 or
	 * Unix hosts.   Handle those hosts now.
	 */

	char *name;

#if defined(_WIN32)
	name = "NUL";
#else
	name = "/dev/null";
#endif
	if ((my_in = fopen(name, "r")) != 0) {
	    filter_def.DoFilter(my_in);
	    fclose(my_in);
	}
    } else if (n < argc) {
	char *name = argv[n++];
	if ((my_in = fopen(name, "r")) != 0) {
	    filter_def.DoFilter(my_in);
	    fclose(my_in);
	}
    } else {
	filter_def.DoFilter(my_in);
    }
    fflush(my_out);
    fclose(my_out);
    exit(GOODEXIT);
}
