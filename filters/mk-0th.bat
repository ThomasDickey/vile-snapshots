@echo off
rem $Id: mk-0th.bat,v 1.7 2000/08/20 21:42:06 tom Exp $
rem like mk-0th.awk, used to generate builtflt.h from genmake.mak
genmake.exe -o%2 -n "#ifndef _builtflt_h"
genmake.exe -o%2 -n "#define _builtflt_h 1"
genmake.exe -o%2 -n ""
genmake.exe -o%2 -n "#include \"filters.h\""
genmake.exe -o%2 -n ""
genmake.exe -o%2    "extern FILTER_DEF define_%%i;" <%1
genmake.exe -o%2 -n ""
genmake.exe -o%2 -n "/* fill the array with 0's so we can get the proper size */"
genmake.exe -o%2 -n "static FILTER_DEF *builtflt[] = {"
genmake.exe -o%2    "	0," <%1
genmake.exe -o%2 -n "	0"
genmake.exe -o%2 -n "};"
genmake.exe -o%2 -n "#undef INIT_c"
genmake.exe -o%2 -n "#undef INIT_k"
genmake.exe -o%2 -n "#define INIT_c(name) builtflt[j++] = ADDR(name)"
genmake.exe -o%2 -n "#ifdef OPT_LEX_FILTER"
genmake.exe -o%2 -n "#define INIT_l(name) builtflt[j++] = ADDR(name)"
genmake.exe -o%2 -n "#else"
genmake.exe -o%2 -n "#define INIT_l(name) /*nothing*/"
genmake.exe -o%2 -n "#endif"
genmake.exe -o%2 -n "/* Visual C++ will not initialize the array except via assignment */"
genmake.exe -o%2 -n "void flt_array(void) { int j = 0;"
genmake.exe -o%2    "	INIT_%%k(define_%%i);" <%1
genmake.exe -o%2 -n "}"
genmake.exe -o%2 -n ""
genmake.exe -o%2 -n "#endif /* _builtflt_h */"
