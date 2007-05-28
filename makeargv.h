/*
 * $Header: /users/source/archives/vile.vcs/RCS/makeargv.h,v 1.2 2007/05/04 20:52:07 tom Exp $
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int is_option(const char * /* param */);
extern int make_argv(const char * /* program */, const char * /*cmdline */, char *** /*argvp */, int * /*argcp */, char ** /*argend*/);

#ifdef __cplusplus
}
#endif
