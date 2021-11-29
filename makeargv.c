/*
 * makeargv.c:  parse string to argv[]
 *
 * $Id: makeargv.c,v 1.8 2016/07/27 09:10:44 tom Exp $
 */

#include <estruct.h>
#include <makeargv.h>

//--------------------------------------------------------------

int
option_has_param(const char *option)
{
    static const char *table[] =
    {
	"-c",
#if OPT_ENCRYPT
	"-k",
#endif
#if OPT_TAGS
#if DISP_X11			/* because -title is predefined */
	"-T",
#else
	"-t",
#endif
#endif
#if DISP_NTWIN
	"-font",
	"-fn",
	"-geometry",
#endif
    };
    unsigned n;
    int result = 0;
    for (n = 0; n < TABLESIZE(table); ++n) {
	if (!strcmp(option, table[n])) {
	    result = 1;
	    break;
	}
    }
    return result;
}

int
after_options(int first, int argc, char **argv)
{
    int result = first;

    while (result < argc && argv[result] != 0 && is_option(argv[result]))
	result += 1 + option_has_param(argv[result]);
    return result;
}

int
is_option(const char *param)
{
    return (*param == '-'
	    || *param == '+'
	    || *param == '@');
}

int
make_argv(const char *program,
	  const char *cmdline,
	  char ***argvp,
	  int *argcp,
	  char **argend)
{

    int maxargs = 2 + ((int) strlen(cmdline) + 2) / 2;
    char *blob;
    char *ptr;
    char **argv;
    int argc = 0;

    if ((blob = typeallocn(char, strlen(cmdline) + 1)) == 0)
	  return -1;

    if ((argv = typeallocn(char *, maxargs)) == 0) {
	free(blob);
	return -1;
    }

    if (argend != 0)
	*argend = 0;

    strcpy(blob, cmdline);
    if (program != 0)
	argv[argc++] = (char *) program;

    for (ptr = blob; *ptr != '\0';) {
	char *dst;
	char delim = ' ';

	while (*ptr == ' ')
	    ptr++;

	if (*ptr == SQUOTE
	    || *ptr == DQUOTE
	    || *ptr == ' ') {
	    delim = *ptr++;
	}

	/*
	 * Save the beginning of non-options in *argend
	 */
	if (argend != 0
	    && *argend == 0
	    && !is_option(ptr)) {
	    *argend = strdup(ptr);
	}

	argv[argc++] = dst = ptr;
	if (argc + 1 >= maxargs) {
	    break;
	}

	while (*ptr != delim && *ptr != '\0') {
	    if (*ptr == '"') {
		ptr++;
		delim = (char) ((delim == ' ') ? '"' : ' ');
	    } else {
		*dst++ = *ptr++;
	    }
	}

	if (*ptr == '"') {
	    ++ptr;
	}

	if (dst != ptr) {
	    *dst = '\0';
	} else if (*ptr == ' ') {
	    *ptr++ = '\0';
	}

    }

    argv[argc] = 0;
    *argvp = argv;
    *argcp = argc;

    return 0;

}
