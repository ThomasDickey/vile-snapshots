/*
 * Generate fragments of filters/makefile (for platforms without useful
 * shell scripting tools)
 *
 * $Id: genmake.c,v 1.5 2002/05/01 19:43:39 tom Exp $
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

static char *program = "";

static int
parse(char *buffer, const char *list[MAXFIELD])
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

static void
substitute(char *output, const char *data, const char **list)
{
    char *d = output;
    const char *s = data;

    if (*program != 0) {
	sprintf(d, "%s ", program);
	d += strlen(d);
    }
    while (*s != 0) {
	if (*s == '%') {
	    switch (*++s) {
	    case '%':
		*d++ = *s;
		break;
	    case 'b':
		*d++ = '\\';
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
	}
	*d = 0;
	s++;
    }
    *d = 0;
}

int
main(int argc, char *argv[])
{
    int n;
    char input[BUFSIZ];
    char output[BUFSIZ];
    char *list[MAXFIELD];
    char *data[BUFSIZ];
    FILE *fp = stdout;
    int use_c = 0;
    int use_l = 0;
    int count = 0;
    int filter = 1;
    int verbose = 0;
    char *redir = 0;
    char *s;
    char *d;

    (void) parse(strcpy(input, ""), list);
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
		case 'n':
		    filter = 0;	/* don't substitute */
		    break;
		case 'o':
		    if (*++s == 0 || (fp = fopen(s, "a")) == 0) {
			fprintf(stderr, "Cannot open output %s\n", s);
			exit(EXIT_FAILURE);
		    }
		    redir = s;
		    s += strlen(s) - 1;
		    break;
		case 'x':
		    if ((program = ++s) == 0)
			program = "";
		    s += strlen(s) - 1;
		    break;
		case 'v':
		    verbose = 1;
		    break;
		}
	    }
	} else {
	    data[count++] = s;
	    if (!filter) {
		substitute(output, s, list);
		fprintf(fp, "%s\n", output);
	    } else if (verbose)
		fprintf(fp, "$ %s\n", s);
	}
    }

    if (filter) {
	if (!use_c && !use_l) {
	    use_c = 1;
	    use_l = 1;
	}

	while (fgets(input, sizeof(input), stdin) != 0) {
	    if (verbose)
		fprintf(fp, ">%s", input);
	    if (parse(input, list)) {
		if (verbose)
		    for (n = 0; n < MAXFIELD; n++)
			fprintf(fp, "\t%d '%s'\n", n, list[n]);
		if (!use_c && !strcmp(list[SELFIELD], "c"))
		    continue;
		if (!use_l && !strcmp(list[SELFIELD], "l"))
		    continue;
		for (n = 0; n < count; n++) {
		    substitute(output, data[n], list);
		    if (*program != 0) {
			if (redir != 0)
			    fclose(fp);
			fprintf(stderr, "%% %s\n", output);
			system(output);		/* won't work on VMS */
			fflush(stdout);
			if (redir != 0)
			    fp = fopen(redir, "a");
		    } else {
			fprintf(fp, "%s\n", output);
		    }
		}
	    }
	}
    }
    fflush(fp);
    fclose(fp);
    exit(EXIT_SUCCESS);
}
