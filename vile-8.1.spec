Summary: VILE VI Like Emacs editor
Name: vile
Version: 8.1
Release: 1
Copyright: Freely Distributable, copyright statements must be maintained
Group: Applications/Editors
Source: ftp.clark.net:/pub/dickey/vile/vile-8.1.tgz
Packager: Radek Liboska <liboska@uochb.cas.cz>
#Patch1 : vile-8.1.patch

%description
Vile is a text editor which is extremely compatible with vi in terms
of "finger feel".  In addition, it has extended capabilities in many
areas,
notably multi-file editing and viewing, key rebinding, and real X window
system support.

%prep
%setup
#%patch1 -p1
%build
%install
./configure --prefix=/usr
make install
./configure --prefix=/usr --with-screen=ncurses --with-screen=Xaw --with-locale
make clean
make install
mv /usr/bin/xvile /usr/X11R6/bin/xvile
mv /usr/man/man1/xvile.1 /usr/X11R6/man/man1/xvile.1
strip /usr/X11R6/bin/xvile
strip /usr/bin/vile
strip /usr/bin/vile-manfilt
strip /usr/bin/vile-c-filt
strip /usr/bin/vile-pas-filt
strip /usr/bin/vile-crypt

%files
%doc CHANGES CHANGES.R3 CHANGES.R4 CHANGES.R5 CHANGES.R6 CHANGES.R7
%doc COPYING INSTALL MANIFEST
%doc README README.PC
%doc doc/*
/usr/X11R6/bin/xvile
/usr/X11R6/man/man1/xvile.1
/usr/bin/vile
/usr/bin/vile-manfilt
/usr/bin/vile-c-filt
/usr/bin/vile-pas-filt
/usr/bin/vile-crypt
/usr/man/man1/vile.1
/usr/share/vile
