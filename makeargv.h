/*
 * $Header: /users/source/archives/vile.vcs/RCS/makeargv.h,v 1.3 2007/06/03 14:26:59 tom Exp $
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int after_options(int /* argc */, char ** /* argv */);
extern int is_option(const char * /* param */);
extern int option_has_param(const char * /* option */);
extern int make_argv(const char * /* program */, const char * /*cmdline */, char *** /*argvp */, int * /*argcp */, char ** /*argend*/);

#ifdef __cplusplus
}
#endif
