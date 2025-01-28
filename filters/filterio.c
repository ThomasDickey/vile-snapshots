/*
 * $Id: filterio.c,v 1.71 2025/01/27 23:23:15 tom Exp $
 *
 * Main program and I/O for external vile syntax/highlighter programs
 */

#include <filters.h>
#include <stdarg.h>

#if defined(_WIN32)
#include "w32vile.h"
#endif

#if defined(__GNUC__) && defined(_FORTIFY_SOURCE)
int ignore_unused;
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
    static char empty[1];
    char *s = *option;
    char *result = ++s;

    if (*result == 0) {
	*inx += 1;
	if (*inx < argc) {
	    result = argv[*inx];
	} else {
	    result = empty;
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

    if (!flag)
	printf("\001M%s", argv[0]);
    memset(flt_options, 0, sizeof(flt_options));
    for (n = 1; n < argc; n++) {
	s = argv[n];
	if (!flag)
	    printf(" %s", s);
	if (*s == '-') {
	    while (*++s) {
		FltOptions(*s) += 1;
		switch (*s) {
		case 'k':
		    value = ParamValue(&s, &n, argc, argv);
		    if (flag) {
			flt_init_table(value);
			flt_setup_symbols(value);
		    }
		    break;
		case 't':
		    value = ParamValue(&s, &n, argc, argv);
		    if ((FltOptions('t') = atoi(value)) <= 0)
			FltOptions('t') = 8;
		    break;
		default:
		    if (((filter_def.options != 0
			  && (strchr) (filter_def.options, *s) == 0)
			 || filter_def.options == 0)
			&& strchr("ivqQ", *s) == 0) {
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
    if (!flag)
	fwrite("\0", sizeof(char), (size_t) 1, stdout);
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
flt_gets(char **ptr, size_t *len)
{
    return readline(my_in, ptr, len);
}

const char *
flt_name(void)
{
    return default_table;
}

char *
flt_put_blanks(char *string)
{
    char *result = skip_blanks(string);
    if (result != string)
	flt_puts(string, (int) (result - string), "");
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
#if 0
    /* markers come out in flt_puts */
    if (ch == CTL_A)
	vl_putc('?', my_out);
#endif
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

int
flt_succeeds(void)
{
    return 1;
}

void
flt_error(const char *fmt, ...)
{
    if (FltOptions('v') || FltOptions('Q')) {
	va_list ap;

	fflush(stdout);

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);

	fflush(stderr);
    }
}

void
flt_message(const char *fmt, ...)
{
    if (FltOptions('v') || FltOptions('Q')) {
	va_list ap;

	fflush(stdout);

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);

	fflush(stderr);
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
mlforce(const char *fmt, ...)
{
    va_list ap;

    fflush(stderr);

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');

    fflush(stdout);
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
#if defined(_WIN32) || defined(_WIN64)
    if (result == 0) {
	static HKEY rootkeys[] =
	{HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};

	int j;
	HKEY hkey;
	DWORD dwSzBuffer;
	char buffer[256];

	for (j = 0; j < (int) TABLESIZE(rootkeys); ++j) {
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

int
vl_check_cmd(const void *cmd GCC_UNUSED, unsigned long flags GCC_UNUSED)
{
    /* added in 9.5s */
    return 0;
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

/* ARGSUSED */
int
vl_is_xcolor(const void *cmd GCC_UNUSED)
{
    /* added in 9.7f */
    return 0;
}

const void *
vl_lookup_cmd(const char *name GCC_UNUSED)
{
    /* added in 9.5s */
    return 0;
}

#if OPT_EXTRA_COLOR
/* ARGSUSED */
int
vl_lookup_color(const char *mode GCC_UNUSED)
{
    /* added in 9.7f */
    return 0;
}
#endif /*  OPT_EXTRA_COLOR */

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

#if OPT_EXTRA_COLOR
/* ARGSUSED */
int
vl_lookup_xcolor(const char *mode GCC_UNUSED)
{
    /* added in 9.7f */
    return 0;
}
#endif /* OPT_EXTRA_COLOR */

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
    my_col = 0;
    my_line = 1;

    /* get verbose option */
    (void) ProcessArgs(argc, argv, 0);

    flt_initialize(filter_def.filter_name);

    filter_def.InitFilter(1);

    flt_read_keywords(MY_NAME);
    if (strcmp(MY_NAME, filter_def.filter_name)) {
	flt_read_keywords(filter_def.filter_name);
    }

    n = ProcessArgs(argc, argv, 1);

    if ((vile_keywords = !FltOptions('k')) != 0) {
	if (strcmp(MY_NAME, default_table)
	    && strcmp(filter_def.filter_name, default_table)) {
	    flt_read_keywords(default_table);
	}
    }
    set_symbol_table(default_table);

    filter_def.InitFilter(0);

    if (FltOptions('q') || FltOptions('Q')) {
	/*
	 * When the filter is called, we want to force it to print out its
	 * class info and then immediately exit.  Easiest way to do this
	 * is to connect the filter's input to /dev/null.
	 *
	 * To date, syntax filtering only works well/reliably on win32 or
	 * Unix hosts.   Handle those hosts now.
	 */

	const char *name;

#if defined(_WIN32)
	name = "NUL";
#else
	name = "/dev/null";
#endif
	if ((my_in = fopen(name, "r")) != 0) {
	    filter_def.DoFilter(my_in);
	    fclose(my_in);
	}
	if (FltOptions('Q'))
	    flt_dump_symtab(NULL);
    } else if (n < argc) {
	while (n < argc) {
	    char *name = argv[n++];
	    if ((my_in = fopen(name, "r")) != 0) {
		filter_def.DoFilter(my_in);
		fclose(my_in);
	    }
	}
    } else {
	filter_def.DoFilter(my_in);
    }
#if NO_LEAKS
    filter_def.FreeFilter();
    filters_leaks();
#endif
    fflush(my_out);
    fclose(my_out);
    exit(GOODEXIT);
}
