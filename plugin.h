#ifndef plugin_h_
#define plugin_h_

/* function implemented by plugin, called at initialization */
/* return: ==0 (FALSE) not ok, !=0 (TRUE) ok */
typedef int (* plugin_init) (void) ;

/* name of shared object file for a plugin */
#define PLUGIN_SO_FMT "vile-%s-plugin.so"
/* name of initialization function of a shared object plugin */
#define PLUGIN_INIT_NAME "%s_vile_plugin_init"

#endif /* plugin_h_ */
