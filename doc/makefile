# $Header: /users/source/archives/vile.vcs/doc/RCS/makefile,v 1.11 2009/12/28 16:58:36 tom Exp $
# construct ".doc" and ".hlp" files from the ".html" files

# list the html files in the order needed for table of contents
SOURCES = vile-hlp.html \
	config.html \
	macros.html \
	modes.html \
	filters.html \
	perl.html \
	dir.html \
	Vileserv.html \
	oleauto.html \
	visvile.html

TARGETS	= \
	../vile.hlp \
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
maintainer-clean:
	rm -f $(TARGETS)

vile-toc.html : $(SOURCES)
	rm -f $@
	sh make-toc.sh $(SOURCES) >$@

../vile.hlp : vile-hlp.html
	rm -f $@
	perl make-hlp.pl vile-hlp.html >$@
