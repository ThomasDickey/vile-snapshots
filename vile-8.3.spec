Summary: VILE VI Like Emacs editor
Name: vile
Version: 8.3
Release: 1
Copyright: Freely Distributable, copyright statements must be maintained
Group: Applications/Editors
Source: ftp.clark.net:/pub/dickey/vile/vile-8.3.tgz
Packager: Thomas Dickey <dickey@clark.net>
#Patch1 : vile-8.2.patch

%description
Vile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%prep
%setup
#%patch1 -p1
%build
%install
./configure --prefix=/usr
make install
./configure --prefix=/usr --with-screen=ncurses --with-screen=Xaw --with-locale
make clean
make install-xvile prefix=/usr/X11R6
strip /usr/X11R6/bin/xvile
strip /usr/bin/vile
strip /usr/lib/vile/vile-*
%clean
test -f makefile && make distclean
rm -f vile

%files
%doc CHANGES CHANGES.R3 CHANGES.R4 CHANGES.R5 CHANGES.R6 CHANGES.R7
%doc COPYING INSTALL MANIFEST
%doc README README.PC README.VMS
%doc doc/*
/usr/X11R6/bin/xvile
/usr/X11R6/man/man1/xvile.1
/usr/bin/vile
/usr/man/man1/vile.1
/usr/share/vile
/usr/lib/vile/
