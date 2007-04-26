/*
 * makeargv.c:  parse string to argv[]
 *
 * $Header: /users/source/archives/vile.vcs/RCS/makeargv.c,v 1.1 2007/04/21 00:25:34 tom Exp $
 */

#include "w32vile.h"

#include <stdlib.h>
#include <string.h>

#include "vl_alloc.h"
#include "makeargv.h"

#define DQUOTE '"'
#define SQUOTE '\''

//--------------------------------------------------------------

int
make_argv(const char *program,
	  const char *cmdline,
	  char ***argvp,
	  int *argcp)
{

    int maxargs = 2 + (strlen(cmdline) + 2) / 2;
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
