/*
 * $Id: makeargv.h,v 1.5 2016/07/27 09:11:10 tom Exp $
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int after_options(int /* first */, int /* argc */, char ** /* argv */);
extern int is_option(const char * /* param */);
extern int option_has_param(const char * /* option */);
extern int make_argv(const char * /* program */, const char * /*cmdline */, char *** /*argvp */, int * /*argcp */, char ** /*argend*/);

#ifdef __cplusplus
}
#endif
