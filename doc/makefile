# $Header: /users/source/archives/vile.vcs/doc/RCS/makefile,v 1.16 2010/08/08 18:05:39 tom Exp $
# construct ".doc" and ".hlp" files from the ".html" files

# NOTE: the perl script uses the version from the running copy of vile rather
# than the source, to apply the version number to .doc files.

# list the html files in the order needed for table of contents
SOURCES = \
	vile-man.html \
	vile-hlp.html \
	config.html \
	macros.html \
	modes.html \
	filters.html \
	menus.html \
	perl.html \
	dir.html \
	Vileserv.html \
	oleauto.html \
	visvile.html

TARGETS	= \
	../vile.hlp \
	vile-man.html \
	vile-toc.html \
	Vileserv.doc \
	config.doc \
	dir.doc \
	filters.doc \
	macros.doc \
	menus.doc \
	modes.doc \
	oleauto.doc \
	perl.doc \
	visvile.doc

.SUFFIXES: .html .doc .hlp

.html.doc :
	rm -f $@
	perl make-hlp.pl $< >$@

.html.hlp :
	rm -f $@
	perl make-hlp.pl $< >$@

all: $(TARGETS)
clean:
mostlyclean:
realclean:
distclean:
	:
maintainer-clean:
	rm -f $(TARGETS)

vile-toc.html : $(SOURCES)
	rm -f $@
	sh make-toc.sh $(SOURCES) >$@

vile-man.html : ../vile.1
	GROFF_NO_SGR=stupid $(SHELL) -c "tbl ../vile.1 | groff -Thtml -man" | sed -f vile-man.sed >$@

../vile.hlp : vile-hlp.html
	rm -f $@
	perl make-hlp.pl vile-hlp.html >$@
