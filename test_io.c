/*
 * Stub for testing linkage requirements of I/O drivers.
 *
 * $Header: /users/source/archives/vile.vcs/RCS/test_io.c,v 1.1 2006/01/10 01:18:48 tom Exp $
 */

#include <estruct.h>

extern TERM term;		/* Terminal information.        */

int
main(int argc, char **argv)
{

    TERM *test = &term;

    return (test != 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
