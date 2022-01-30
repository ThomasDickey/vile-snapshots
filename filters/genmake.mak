# $Id: genmake.mak,v 1.53 2022/01/25 01:29:41 tom Exp $
# This is a list of filter root names and whether .c or .l files define the
# filter.  Except for vile-crypt and vile-manfilt (which do not correspond to
# majormodes), the filter names are constructed as vile-{root}-filt.

c	c-filt		c
key	key-filt	c
m4	m4-filt		c
perl	pl-filt		c
raku	rakufilt	c
ruby	rubyfilt	c
sed	sed-filt	c
tags	tagsfilt	c

ada	ada-filt	l
as	as-filt		l
asm	asm-filt	l
au3	au3-filt	l
awk	awk-filt	l
basic	bas-filt	l
bat	bat-filt	l
bnf	bnf-filt	l
cfg	cfg-filt	l
conf	conffilt	l
css	css-filt	l
cweb	cwebfilt	l
dcl	dcl-filt	l
def	def-filt	l
diff	difffilt	l
ecl	ecl-filt	l
erl	erl-filt	l
esql	esqlfilt	l
est	est-filt	l
fdl	fdl-filt	l
hs	hs-filt		l
html	htmlfilt	l	flex
imake	imakeflt	l
info	infofilt	l
ini	ini-filt	l
iss	iss-filt	l
json	jsonfilt	l
latex	latexflt	l
lex	lex-filt	l	flex
lisp	lispfilt	l
lua	lua-filt	l
mail	mailfilt	l
mailcap	mc-filt		l
make	makefilt	l
mcrl	mcrlfilt	l
md	md-filt		l
midl	midlfilt	l
mms	mms-filt	l
nmake	nmakeflt	l
nr	nr-filt		l
pas	pas-filt	l
php	php-filt	l
pot	pot-filt	l
ps	ps-filt		l
ps1	ps1-filt	l
py	py-filt		l
rc	rc-filt		l
rcs	rcs-filt	l
rexx	rexxfilt	l
rpm	rpm-filt	l
rtf	rtf-filt	l
rust	rustfilt	l
sccs	sccsfilt	l
scheme	scm-filt	l
sh	sh-filt		l
sml	sml-filt	l
spell	spellflt	l
sql	sql-filt	l
tbl	tbl-filt	l
tc	tc-filt		l
tcl	tcl-filt	l
texi	texifilt	l
ti	ti-filt		l
tpu	tpu-filt	l
txt	txt-filt	l
vile	vilefilt	l
vlog	vl-filt		l
wbt	wbt-filt	l
xml	xml-filt	l
xq	xq-filt		l
xres	xresfilt	l
xs	xs-filt		l
yacc	yaccfilt	l
yaml	yamlfilt	l
