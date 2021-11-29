@echo off
rem $Id: mk-2nd.bat,v 1.10 2014/02/01 00:35:46 tom Exp $
rem like mk-2nd.awk, used to generate rules from genmake.mak

goto %2

:cmpc
genmake.exe -o%3 -n "%5$o :"
genmake.exe -o%3 -n "	$(cc) -Dfilter_def=define_%4 $(CFLAGS) -c %5.c -Fo$@"
genmake.exe -o%3 -n ""
goto done

:cmpl
rem The odd "LEX.%4_.c" happens to be what flex generates.
genmake.exe -o%3 -n "%5$o :"
genmake.exe -o%3 -n "	$(LEX) -P%4_ %5.l"
genmake.exe -o%3 -n "	$(CC) -Dfilter_def=define_%4 $(CFLAGS) -c LEX.%4_.c -Fo$@"
genmake.exe -o%3 -n "	- erase LEX.%4_.c"
genmake.exe -o%3 -n ""
goto done

:link_c
genmake.exe -o%3 -n "vile-%4-filt$x : %5$o $(CF_DEPS) $(RC_DEPS)"
genmake.exe -o%3 -n "	$(link) -out:$@ %5$o $(CF_ARGS) $(RC_ARGS) $(CON_LDFLAGS)"
genmake.exe -o%3 -n ""
goto done

:link_l
genmake.exe -o%3 -n "vile-%4-filt$x : %5$o $(LF_DEPS) $(RC_DEPS)"
genmake.exe -o%3 -n "	$(link) -out:$@ %5$o $(LF_ARGS) $(RC_ARGS) $(CON_LDFLAGS)"
genmake.exe -o%3 -n ""
goto done

:extern
if exist %3 del %3
genmake.exe -o%3 -n "# generated by %0"
genmake.exe -o%3 -x%0 ". link_%%k %3 %%i %%j %%k" <%1
goto done

:intern
if exist %3 del %3
genmake.exe -o%3 -n "# generated by %0"
rem library rule is in makefile.wnt, since we cannot echo redirection
rem chars needed for "inline" (aka here-document).
genmake.exe -o%3 -x%0 ". cmp%%k %3 %%i %%j %%k" <%1
goto done

:done
