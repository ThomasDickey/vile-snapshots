/*
 * Stub for testing linkage requirements of I/O drivers.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/test_io.c,v 1.2 2006/01/10 23:28:47 tom Exp $
 */

#include <estruct.h>

#define realdef			/* Make global definitions not external */
#include	"edef.h"	/* global declarations */

int
main(int argc, char **argv)
{

    TERM *test = &term;

    return (test != 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
