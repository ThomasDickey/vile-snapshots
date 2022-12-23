# $Id: makefile,v 1.24 2022/12/23 00:55:56 tom Exp $
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
	sh make-toc.sh $(SOURCES)

vile-man.html : ../vile.1
	rm -f $@
	MAN2HTML_OPTS=-toc ../man2html.tmp ../vile 1 man | sed -e 's,\.\./vile,vile', >$@

../vile.hlp : vile-hlp.html
	rm -f $@
	perl make-hlp.pl vile-hlp.html >$@
