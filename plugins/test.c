/*
 * $Id: test.c,v 1.2 2004/06/17 23:53:47 tom Exp $
 */
#include "estruct.h"
#include "edef.h"

int
test_vile_plugin_init(void)
{
    static int done = 0;
    if (done)
	mlwarn("test plugin already loaded");
    else
	mlwarn("test plugin loaded");
    done = 1;
    return TRUE;
}
