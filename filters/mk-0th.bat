@echo off
rem $Header: /users/source/archives/vile.vcs/filters/RCS/mk-0th.bat,v 1.3 2000/07/27 02:08:28 tom Exp $
rem like mk-0th.awk, used to generate builtflt.h from genmake.mak
echo #ifndef _builtflt_h
echo #define _builtflt_h 1
echo.
echo #include "filters.h"
echo.
rem chew it up into small bites so the "enhanced" shell will see the lines.
sort <%1 >genmake.tmp
FOR /F "eol=# tokens=1,2 delims=	" %%i in (genmake.tmp) do @echo extern FILTER_DEF define_%%i;
echo.
echo /* fill the array with 0's so we can get the proper size */
echo static FILTER_DEF *builtflt[] = {
FOR /F "eol=# tokens=1,2 delims=	" %%i in (genmake.tmp) do @echo 	0,
echo };
echo #undef INIT
echo #define INIT(name) builtflt[j++] = ADDR(name)
echo /* Visual C++ will not initialize the array except via assignment */
echo void flt_array(void) { int j = 0;
FOR /F "eol=# tokens=1,2 delims=	" %%i in (genmake.tmp) do @echo 	INIT(define_%%i);
echo }
echo.
echo #endif /* _builtflt_h */
del genmake.tmp
