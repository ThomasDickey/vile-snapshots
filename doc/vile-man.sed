# $Header: /users/source/archives/vile.vcs/doc/RCS/vile-man.sed,v 1.10 2013/12/27 20:56:36 tom Exp $
s/<h1\([^>]*\)>\([^<]*\)/<h1 id="toplevel-toc"\1><a name="toplevel">\2<\/a>/
s/<h2>\([^<]*\)/<h2 id="\1-toc">\1/
:ids
	s/id=\("[^ "]*\) /id=\1_/g
	t ids
:hrefs
	s/href=\("[^ "]*\) /href=\1_/g
	t hrefs
:names
	s/name=\("[^ "]*\) /name=\1_/g
	t names
s/<i>\([^<.]*\)\.doc<\/i>/<a href="\1.html"><i>\1.doc<\/i><\/a>/g
s/<i>\([^<.]*\)\.hlp<\/i>/<a href="\1-hlp.html"><i>\1.hlp<\/i><\/a>/g
s%\([^"]\)\(http[s]\?:[[:alnum:]/._-]*\)\([^"]\)%\1<a href="\2">\2</a>\3%g
