# $Header: /users/source/archives/vile.vcs/filters/RCS/mk-0th.awk,v 1.2 2000/03/17 10:56:48 tom Exp $
BEGIN	{
		print "/* Generated by filters/mk-0th.awk */"
		count = 0;
		first = 1;
	}
	!/^#/ {
		# command-line parameters aren't available until we're matching
		if (first == 1) {
			Len = split(list,List,/,/)
			first = 0;
		}
		if ( NF >= 2 ) {
			for (i = 1; i <= Len; i++) {
				if ( $1 == List[i] || List[i] == "all" ) {
					table[count++] = $1;
					break;
				}
			}
		}
	}
END	{
		if (count != 0) {
			print "#ifndef _builtflt_h"
			print "#define _builtflt_h 1"
			print ""
			print "#include <filters.h>"
			print ""
			for (i = 0; i < count; i++) {
				printf "extern FILTER_DEF define_%s;\n", table[i]
			}
			print ""
			print "static FILTER_DEF *builtflt[] = {"
			for (i = 0; i < count; i++) {
				printf "\t%s&define_%s\n", i != 0 ? ", " : "", table[i]
			}
			print "};"
			print ""
			print "#endif /* _builtflt_h */"
		}
	}
