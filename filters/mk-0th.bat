@echo off
rem $Header: /users/source/archives/vile.vcs/filters/RCS/mk-0th.bat,v 1.4 2000/08/09 00:54:42 tom Exp $
rem like mk-0th.awk, used to generate builtflt.h from genmake.mak
echo #ifndef _builtflt_h
echo #define _builtflt_h 1
echo.
echo #include "filters.h"
echo.
genmake.exe "extern FILTER_DEF define_%%i;" <%1
echo.
echo /* fill the array with 0's so we can get the proper size */
echo static FILTER_DEF *builtflt[] = {
genmake.exe "	0," <%1
echo 	0
echo };
echo #undef INIT_c
echo #undef INIT_k
echo #define INIT_c(name) builtflt[j++] = ADDR(name)
echo #ifdef OPT_LEX_FILTER
echo #define INIT_l(name) builtflt[j++] = ADDR(name)
echo #else
echo #define INIT_l(name) /*nothing*/
echo #endif
echo /* Visual C++ will not initialize the array except via assignment */
echo void flt_array(void) { int j = 0;
genmake.exe "	INIT_%%k(define_%%i);" <%1
echo }
echo.
echo #endif /* _builtflt_h */
