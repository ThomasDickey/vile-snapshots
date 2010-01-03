# $Header: /users/source/archives/vile.vcs/doc/RCS/vile-man.sed,v 1.3 2010/01/03 14:23:40 tom Exp $
s/<h2>\([^<]*\)/<h2 id="\1-id">\1/
s/id=\("[^ "]*\) /id=\1_/g
s/id=\("[^ "]*\) /id=\1_/g
s/href=\("[^ "]*\) /href=\1_/g
s/href=\("[^ "]*\) /href=\1_/g
s/name=\("[^ "]*\) /name=\1_/g
s/name=\("[^ "]*\) /name=\1_/g
s/<i>\([^<.]*\)\.doc<\/i>/<a href="\1.html"><i>\1.doc<\/i><\/a>/g
s/<i>\([^<.]*\)\.hlp<\/i>/<a href="\1-hlp.html"><i>\1.hlp<\/i><\/a>/g
