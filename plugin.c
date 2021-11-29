/*
 * $Id: plugin.c,v 1.2 2004/06/17 23:48:26 tom Exp $
 */
#include "estruct.h"
#include "edef.h"
#include "plugin.h"

#include <dlfcn.h>

#if defined(RTLD_NOW)
#define my_RTLD RTLD_NOW
#elif defined(RTLD_LAZY)
#define my_RTLD RTLD_LAZY
#else
#define my_RTLD 0
#endif

int
loadplugin(int f GCC_UNUSED, int n GCC_UNUSED)
{
    int ret = FALSE;
    int status;
    static char name[NSTRING - sizeof(PLUGIN_INIT_NAME)] = "";
    char leafname[NSTRING];
    char symname[NSTRING];
    const char *cp = libdir_path;
    void *found = 0;
    plugin_init init;

    /* obtain name from user/rcfile */
    status = mlreply("Plugin name: ", name, (sizeof name) - 1);
    if (status == ABORT)
	return FALSE;

    sprintf(leafname, PLUGIN_SO_FMT, name);
    sprintf(symname, PLUGIN_INIT_NAME, name);

    TRACE(("leafname %s\n", leafname));
    TRACE(("symname  %s\n", symname));

    if ((cp = cfg_locate(leafname, LOCATE_EXEC)) != 0) {
	TRACE(("try dlopen(%s)\n", cp));
	found = dlopen(cp, my_RTLD);
    }

    if (found == 0)
	mlwarn("Error: plugin '%s' not found", name);
    else if ((init = (plugin_init) dlsym(found, symname)) == 0)
	mlwarn("Error: plugin '%s' initialization function not found", name);
    else if (init() == 0)
	mlwarn("Error: failed to initialize plugin '%s'", name);
    else
	ret = TRUE;

    /* TODO: on success, remember in loaded plugin list */

    return ret;
}
