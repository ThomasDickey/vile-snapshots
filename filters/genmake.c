/*
 * Generate fragments of filters/makefile (for platforms without useful
 * shell scripting tools)
 *
 * $Header: /users/source/archives/vile.vcs/filters/RCS/genmake.c,v 1.1 2000/08/08 22:00:44 tom Exp $
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

#define MAXFIELD 3
#define SELFIELD 2

static int
parse(char *buffer, char *list[MAXFIELD])
{
    unsigned len = strlen(buffer);
    int n = 0;
    char *s = buffer;

    while (len != 0 && isspace(buffer[len - 1]))
	buffer[--len] = 0;
    while (*s != 0 && isspace(*s))
	s++;
    if (*s != 0 && *s != '#') {
	while (n < MAXFIELD) {
	    if (*s == 0) {
		list[n++] = "";
	    } else {
		list[n++] = s;
		while (*s != 0 && !isspace(*s))
		    s++;
		if (isspace(*s)) {
		    while (*s != 0 && isspace(*s))
			*s++ = 0;
		}
	    }
	}
	return 1;
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    int n;
    char input[BUFSIZ];
    char output[BUFSIZ];
    char *list[MAXFIELD];
    char *data[BUFSIZ];
    int use_c = 0;
    int use_l = 0;
    int count = 0;
    int verbose = 0;
    char *s;
    char *d;
    char *program = 0;

    for (n = 1; n < argc; n++) {
	s = argv[n];
	if (*s == '-') {
	    while (*++s) {
		switch (*s) {
		case 'c':
		    use_c = 1;
		    break;
		case 'l':
		    use_l = 1;
		    break;
		case 'x':
		    program = ++s;
		    s += strlen(s) - 1;
		    break;
		case 'v':
		    verbose = 1;
		    break;
		}
	    }
	} else {
	    data[count++] = s;
	    if (verbose)
		printf("$ %s\n", s);
	}
    }
    if (!use_c && !use_l) {
	use_c = 1;
	use_l = 1;
    }

    while (fgets(input, sizeof(input), stdin) != 0) {
	if (verbose)
	    printf(">%s", input);
	if (parse(input, list)) {
	    if (verbose)
		for (n = 0; n < MAXFIELD; n++)
		    printf("\t%d '%s'\n", n, list[n]);
	    if (!use_c && !strcmp(list[SELFIELD], "c"))
		continue;
	    if (!use_l && !strcmp(list[SELFIELD], "l"))
		continue;
	    for (n = 0; n < count; n++) {
		d = output;
		if (program != 0) {
		    sprintf(d, "%s ", program);
		    d += strlen(d);
		}
		for (s = data[n]; *s != 0; s++) {
		    if (*s == '%') {
			switch (*++s) {
			case '%':
			    *d++ = *s;
			    *d = 0;
			    break;
			case 'b':
			    *d++ = '\\';
			    *d = 0;
			    break;
			case 'i':
			case 'j':
			case 'k':
			    strcpy(d, list[*s - 'i']);
			    d += strlen(d);
			    break;
			default:
			    s--;
			    break;
			}
		    } else {
			*d++ = *s;
			*d = 0;
		    }
		}
		if (program != 0) {
		    system(output);	/* won't work on VMS */
		    fflush(stdout);
		} else {
		    puts(output);
		}
	    }
	}
    }
    fflush(stdout);
    exit(EXIT_SUCCESS);
}
