/*
 * Main program and I/O for external vile syntax/highlighter programs
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/filterio.c,v 1.36 2007/09/19 22:44:15 tom Exp $
 *
 */

#include <filters.h>
#include <stdarg.h>

#if defined(_WIN32)
#include "w32vile.h"
#endif

static FILE *my_out;
static FILE *my_in;
static int my_line;
static int my_col;

/******************************************************************************
 * Private functions                                                          *
 ******************************************************************************/

static char *
ParamValue(char **option, int *inx, int argc, char *argv[])
{
    char *s = *option;
    char *result = ++s;

    if (*result == 0) {
	*inx += 1;
	if (*inx < argc) {
	    result = argv[*inx];
	} else {
	    result = "";
	}
    } else {
	*option += strlen(*option) - 1;
    }
    return result;
}

static int
ProcessArgs(int argc, char *argv[], int flag)
{
    int n;
    char *s, *value;

    for (n = 1; n < argc; n++) {
	s = argv[n];
	if (*s == '-') {
	    while (*++s) {
		FltOptions(*s) += 1;
		switch (*s) {
		case 'k':
		    value = ParamValue(&s, &n, argc, argv);
		    if (flag) {
			flt_read_keywords(value);
		    }
		    break;
		case 't':
		    value = ParamValue(&s, &n, argc, argv);
		    if ((FltOptions('t') = atoi(value)) <= 0)
			FltOptions('t') = 8;
		    break;
		default:
		    if (((filter_def.options != 0
			  && strchr(filter_def.options, *s) == 0)
			 || filter_def.options == 0)
			&& strchr("vq", *s) == 0) {
			fprintf(stderr, "unknown option %c\n", *s);
			exit(BADEXIT);
		    }
		    break;
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

/*
 * Trim newline from the string, returning true if it was found.
 */
int
chop_newline(char *s)
{
    size_t len = strlen(s);

    if (len != 0 && s[len - 1] == '\n') {
	s[--len] = '\0';
	return 1;
    }
    return 0;
}

char *
flt_gets(char **ptr, unsigned *len)
{
    return readline(my_in, ptr, len);
}

const char *
flt_name(void)
{
    return filter_def.filter_name;
}

char *
flt_put_blanks(char *string)
{
    char *result = skip_blanks(string);
    if (result != string)
	flt_puts(string, result - string, "");
    return result;
}

void
flt_putc(int ch)
{
    vl_putc(ch, my_out);

    if (ch == '\n') {
	++my_line;
	my_col = 0;
    } else {
	++my_col;
    }

    /* markers come out in flt_puts */
    if (ch == CTL_A)
	vl_putc('?', my_out);
}

void
flt_puts(const char *string, int length, const char *marker)
{
    if (length > 0) {
	int ch;

	if (marker != 0 && *marker != 0 && *marker != 'N')
	    fprintf(my_out, "%c%i%s:", CTL_A, length, marker);

	while (length-- > 0) {
	    if (string != 0) {
		ch = *string++;
	    } else {
		ch = '?';
	    }
	    flt_putc(ch);
	}
    }
}

int
flt_get_line(void)
{
    return my_line;
}

int
flt_get_col(void)
{
    return my_col;
}

void
flt_error(const char *fmt,...)
{
    (void) fmt;
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
    while (isspace(CharOf(*src)))
	src++;
    return (src);
}

char *
strmalloc(const char *src)
{
    char *ns = typeallocn(char, strlen(src) + 1);
    if (ns != 0)
	(void) strcpy(ns, src);
    return ns;
}

char *
vile_getenv(const char *name)
{
    char *result = getenv(name);
#if defined(_WIN32)
    if (result == 0) {
	static HKEY rootkeys[] =
	{HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};

	int j;
	HKEY hkey;
	DWORD dwSzBuffer;
	char buffer[256];

	for (j = 0; j < TABLESIZE(rootkeys); ++j) {
	    if (RegOpenKeyEx(rootkeys[j],
			     VILE_SUBKEY "\\Environment",
			     0,
			     KEY_READ,
			     &hkey) == ERROR_SUCCESS) {
		dwSzBuffer = sizeof(buffer);
		if (RegQueryValueEx(hkey,
				    name,
				    NULL,
				    NULL,
				    (LPBYTE) buffer,
				    &dwSzBuffer) == ERROR_SUCCESS
		    && dwSzBuffer != 0) {

		    buffer[dwSzBuffer - 1] = 0;
		    result = strmalloc(buffer);
		    (void) RegCloseKey(hkey);
		    break;
		}

		(void) RegCloseKey(hkey);
	    }
	}
    }
#endif
    return result;
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
DIRECTIVE
dname_to_dirnum(char **s GCC_UNUSED, size_t length GCC_UNUSED)
{
    return D_ENDM;
}

/* ARGSUSED */
int
vl_is_majormode(const void *cmd GCC_UNUSED)
{
    /* added in 9.5s */
    return 0;
}

/* ARGSUSED */
int
vl_is_setting(const void *cmd GCC_UNUSED)
{
    /* added in 9.5s */
    return 0;
}

/* ARGSUSED */
int
vl_is_submode(const void *cmd GCC_UNUSED)
{
    /* added in 9.5s */
    return 0;
}

const void *
vl_lookup_cmd(const char *name GCC_UNUSED)
{
    /* added in 9.5s */
    return 0;
}

/* ARGSUSED */
int
vl_lookup_func(const char *name GCC_UNUSED)
{
    return 0;
}

/* ARGSUSED */
int
vl_lookup_mode(const char *mode GCC_UNUSED)
{
    /* added in 9.5s */
    return 0;
}

/* ARGSUSED */
int
vl_lookup_var(const char *mode GCC_UNUSED)
{
    /* added in 9.5s */
    return 0;
}

/******************************************************************************
 * Standalone main-program                                                    *
 ******************************************************************************/
int
main(int argc, char **argv)
{
    int n;
    int verbose;

#if OPT_LOCALE
    setlocale(LC_CTYPE, "");
#endif

    my_in = stdin;
    my_out = stdout;
    my_col = 0;
    my_line = 1;

    memset(flt_options, 0, sizeof(flt_options));

    /* get verbose option */
    (void) ProcessArgs(argc, argv, 0);
    verbose = FltOptions('v');

    flt_make_symtab(filter_def.filter_name);

    default_attr = strmalloc(NAME_KEYWORD);

    filter_def.InitFilter(1);

    flt_read_keywords(MY_NAME);
    n = ProcessArgs(argc, argv, 1);
    FltOptions('v') = verbose;

    if ((vile_keywords = !FltOptions('k')) != 0) {
	if (strcmp(MY_NAME, filter_def.filter_name)) {
	    flt_read_keywords(filter_def.filter_name);
	}
    }
    set_symbol_table(filter_def.filter_name);

    filter_def.InitFilter(0);

    if (FltOptions('q')) {
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
#if NO_LEAKS
    free(default_attr);
    filters_leaks();
#endif
    fflush(my_out);
    fclose(my_out);
    exit(GOODEXIT);
}
