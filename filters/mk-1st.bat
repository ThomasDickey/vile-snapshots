@echo off
rem $Id: mk-1st.bat,v 1.7 2000/08/20 19:00:48 tom Exp $
rem like mk-1st.awk, used to generate lists from genmake.mak

genmake.exe -o%3 -n "# generated by %0.bat

goto %2

:extern
genmake.exe -o%3 -n "ALL_C	= %%b"
genmake.exe -o%3 -c "	vile-%%i-filt$x %%b" <%1
genmake.exe -o%3 -n ""
if "%LEX%" == "" goto done
genmake.exe -o%3 -n "ALL_LEX	= %%b"
genmake.exe -o%3 -l "	vile-%%i-filt$x %%b" <%1
genmake.exe -o%3 -n ""
goto done

:intern
genmake.exe -o%3 -n "OBJ_C	= %%b"
genmake.exe -o%3 -c "	%%j$o %%b" <%1
genmake.exe -o%3 -n ""
genmake.exe -o%3 -n "OBJ_LEX	= %%b"
genmake.exe -o%3 -l "	%%j$o %%b" <%1
genmake.exe -o%3 -n ""
goto done

:done

genmake.exe -o%3 -n "KEYS	= %%b"
FOR %%i in (*.key) do genmake.exe -o%3 -n "	%%i %%b"
genmake.exe -o%3 -n ""
