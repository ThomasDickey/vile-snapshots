@echo off
rem $Header: /users/source/archives/vile.vcs/filters/RCS/mk-0th.bat,v 1.5 2000/08/16 09:59:46 tom Exp $
rem like mk-0th.awk, used to generate builtflt.h from genmake.mak
genmake.exe -n "#ifndef _builtflt_h"
genmake.exe -n "#define _builtflt_h 1"
genmake.exe -n ""
genmake.exe -n "#include \"filters.h\""
genmake.exe -n ""
genmake.exe    "extern FILTER_DEF define_%%i;" <%1
genmake.exe -n ""
genmake.exe -n "/* fill the array with 0's so we can get the proper size */"
genmake.exe -n "static FILTER_DEF *builtflt[] = {"
genmake.exe    "	0," <%1
genmake.exe -n "	0"
genmake.exe -n "};"
genmake.exe -n "#undef INIT_c"
genmake.exe -n "#undef INIT_k"
genmake.exe -n "#define INIT_c(name) builtflt[j++] = ADDR(name)"
genmake.exe -n "#ifdef OPT_LEX_FILTER"
genmake.exe -n "#define INIT_l(name) builtflt[j++] = ADDR(name)"
genmake.exe -n "#else"
genmake.exe -n "#define INIT_l(name) /*nothing*/"
genmake.exe -n "#endif"
genmake.exe -n "/* Visual C++ will not initialize the array except via assignment */"
genmake.exe -n "void flt_array(void) { int j = 0;"
genmake.exe    "	INIT_%%k(define_%%i);" <%1
genmake.exe -n "}"
genmake.exe -n ""
genmake.exe -n "#endif /* _builtflt_h */"
