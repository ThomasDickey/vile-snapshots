Summary: VILE VI Like Emacs editor
Name: vile
Version: 8.4
Release: 1
Copyright: GPL
Group: Applications/Editors
URL: ftp://ftp.clark.net/pub/dickey/vile
Source0: vile-8.4.tgz
Packager: Thomas Dickey <dickey@clark.net>

%description
Vile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%prep
%setup -q -n vile-8.4
#%patch1 -p1

%build
EXTRA_CFLAGS="$RPM_OPT_FLAGS" INSTALL_PROGRAM='${INSTALL} -s' ./configure --with-screen=x11 --with-xpm --prefix=/usr --with-locale
make xvile
EXTRA_CFLAGS="$RPM_OPT_FLAGS" INSTALL_PROGRAM='${INSTALL} -s' ./configure --prefix=/usr --with-screen=ncurses --with-locale
touch x11.o menu.o
make
touch xvile

%install
rm -rf $RPM_BUILD_ROOT
make install prefix=$RPM_BUILD_ROOT/usr
make TARGET=xvile install-xvile prefix=$RPM_BUILD_ROOT/usr/X11R6

strip $RPM_BUILD_ROOT/usr/X11R6/bin/xvile
strip $RPM_BUILD_ROOT/usr/bin/vile
strip $RPM_BUILD_ROOT/usr/lib/vile/vile-*filt

./mkdirs.sh $RPM_BUILD_ROOT/usr/X11R6/man/man1  
install -m 644 vile.1 $RPM_BUILD_ROOT/usr/X11R6/man/man1/xvile.1  

./mkdirs.sh $RPM_BUILD_ROOT/etc/X11/wmconfig  
install vile.wmconfig $RPM_BUILD_ROOT/etc/X11/wmconfig/vile  
install xvile.wmconfig $RPM_BUILD_ROOT/etc/X11/wmconfig/xvile  
  
%clean
rm -rf $RPM_BUILD_ROOT

%files
%doc CHANGES*
%doc COPYING INSTALL MANIFEST README*
%doc doc/*
%doc macros
%config(missingok) /etc/X11/wmconfig/vile
%config(missingok) /etc/X11/wmconfig/xvile
/usr/X11R6/bin/xvile
/usr/X11R6/man/man1/xvile.1
/usr/bin/vile
/usr/man/man1/vile.1
/usr/share/vile
/usr/lib/vile/

%changelog
* Mon Nov 1 1999 Thomas Dickey
- modified based on spec by Tim Powers <timp@redhat.com>  
