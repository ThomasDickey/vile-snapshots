# $Id: mk-2nd.awk,v 1.23 2016/08/03 00:47:55 tom Exp $
#
# Generate makefile rules for vile's external and built-in filters.  We will
# build each filter only one way, to avoid conflict with the generated files.
# This script uses these parameters:
#	flex =  yes/no, depending on whether lex is really flex.
#	mode =	one of 'built-in','loadable' or 'external'
#	list =	a list of the filter root-names that are built-in, with 'all'
#		matching all root-names.
BEGIN	{
	    first = 1;
	    count = 0;
	}
	!/^#/ {
	    # command-line parameters aren't available until we're matching
	    if (first == 1) {
		Len = split(list,List,/[, ]/)
		if (mode == "loadable") {
		    suffix = "$(so)";
		} else {
		    suffix = "$x";
		}
	    }
	    found = 0;
	    if ( NF >= 2 ) {
		if ( NF >= 4 && flex != "yes" ) {
		    if ( $4 = flex )
			next;
		}
		for (i = 1; i <= Len; i++) {
		    if ( $1 == List[i] || List[i] == "all" ) {
			found = 1;
			break;
		    }
		}
		if (found) {
		    main[count] = 1;
		    prog[count] = "vile-" $1 "-filt" suffix;
		    name[count] = $1;
		    file[count] = sprintf("%s.%s", $2, $3);
		    root[count] = $2;
		    type[count] = $3;
		    count = count + 1;
		    if (first == 1) {
			printf "# Lists generated by filters/mk-2nd.awk for %s filters\n", mode
			first = 0;
		    }
		}
	    }
	}
END	{
	    if ( first == 1 ) {
		printf "# No lists generated by filters/mk-2nd.awk for %s filters\n", mode
	    } else if ( mode == "external" ) {
		for (i = 0; i < count; i++) {
		    nit = (type[i] == "c") ? "C" : "L";
		    print ""
		    if (main[i]) {
			printf "%s : %s$o $(%sF_DEPS)\n", prog[i], root[i], nit
			printf "\t%s$(LINK) $(LDFLAGS) -o $@ %s$o $(%sF_ARGS)\n", linking, root[i], nit
		    } else {
			printf "%s : %s$o\n", prog[i], root[i]
			printf "\t%s$(LINK) $(LDFLAGS) -o $@ %s$o\n", linking, root[i]
		    }
		}
		print ""
		print "# dependency-rules for install/installdirs (%s)", mode
		for (i = 0; i < count; i++) {
		    src = prog[i];
		    dst = sprintf("$(FILTERS_BINDIR)/%s", src);
		    printf "%s :\t%s\t\t; $(INSTALL_PROGRAM) $? $@\n", dst, src
		}
	    } else {
		if ( mode == "built-in" ) {
		    comp="$(CC) -c $(CPPFLAGS) $(CFLAGS) -Dfilter_def=define_"
		} else {
		    comp="$(CC) -c $(SH_CFLAGS) $(CPPFLAGS) $(CFLAGS) -Dfilter_def=define_"
		}
		for (i = 0; i < count; i++) {
		    dst = sprintf("%s$o", root[i]);
		    print ""
		    printf "%s : %s\n", dst, file[i]
		    if (index(compiling,"#") == 0)
			printf "\t@echo compiling %s\n", file[i]
		    if (type[i] == "l") {
			printf "\t%s echo '#include <flt_defs.h>' > %s.c\n", show, root[i]
			printf "\t%s$(LEX) -P%s_ -t %s/%s >> %s.c\n", show, name[i], from, file[i], root[i]
			printf "\t%s%s%s -Dprivate_yywrap=%s_wrap %s/%s.c\n", show, comp, name[i], name[i], ".", root[i]
			printf "\t%s $(RM) %s.c\n", show, root[i]

			dst = sprintf("%s.c", root[i]);
			print ""
			printf "%s : %s\n", dst, file[i]
			if (index(compiling,"#") == 0)
			    printf "\t@echo processing %s\n", file[i]
			printf "\t%s echo '#include <flt_defs.h>' > %s.c\n", show, root[i]
			printf "\t%s$(LEX) -P%s_ -t %s/%s >> %s.c\n", show, name[i], from, file[i], root[i]
		    } else {
			printf "\t%s%s%s %s/%s.c\n", show, comp, name[i], from, root[i]
		    }
		}
		print ""
		if (mode == "loadable") {
		    for (i = 0; i < count; i++) {
			printf "%s : %s$o\n", prog[i], root[i]
			printf "\t%s$(LINK) $(LDFLAGS) -o $@ $(SH_LDFLAGS) $?\n", show
			print ""
		    }
		    print "# dependency-rules for install/installdirs (%s)", mode
		    for (i = 0; i < count; i++) {
			src = prog[i];
			dst = sprintf("$(FILTERS_BINDIR)/%s", src);
			printf "%s :\t%s\t\t; $(INSTALL_PROGRAM) $? $@\n", dst, src
		    }
		}
	    }
	}
