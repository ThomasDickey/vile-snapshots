Summary: VILE VI Like Emacs editor
# $Header: /users/source/archives/vile.vcs/RCS/vile-9.3.spec,v 1.4 2002/10/17 00:41:40 tom Exp $
Name: vile
Version: 9.3e
# each patch should update the version
Release: 1
Copyright: GPL
Group: Applications/Editors
URL: ftp://invisible-island.net/vile
Source0: vile-9.3.tgz
Patch1: vile-9.3a.patch.gz
Patch2: vile-9.3b.patch.gz
Patch3: vile-9.3c.patch.gz
Patch4: vile-9.3d.patch.gz
Patch5: vile-9.3e.patch.gz
# each patch should add itself to this list
Packager: Thomas Dickey <dickey@herndon4.his.com>
BuildRoot: %{_tmppath}/%{name}-root

%description
Vile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%prep
%setup -q -n vile-9.3
%patch1 -p1
%patch2 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
# each patch should add itself to this list

%build
EXTRA_CFLAGS="$RPM_OPT_FLAGS" INSTALL_PROGRAM='${INSTALL} -s' ./configure --target %{_target_platform} --prefix=%{_prefix} --with-locale --with-builtin-filters --with-screen=x11 --with-xpm
make xvile
EXTRA_CFLAGS="$RPM_OPT_FLAGS" INSTALL_PROGRAM='${INSTALL} -s' ./configure --target %{_target_platform} --prefix=%{_prefix} --with-locale --with-builtin-filters --with-screen=ncurses
touch x11.o menu.o
make
touch xvile

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
make install prefix=$RPM_BUILD_ROOT/%{_prefix}
make TARGET=xvile install-xvile prefix=$RPM_BUILD_ROOT/%{_prefix}/X11R6

strip $RPM_BUILD_ROOT/%{_prefix}/X11R6/bin/xvile
strip $RPM_BUILD_ROOT/%{_bindir}/vile
strip $RPM_BUILD_ROOT/%{_libdir}/vile/vile-*filt

./mkdirs.sh $RPM_BUILD_ROOT/%{_prefix}/X11R6/man/man1  
install -m 644 vile.1 $RPM_BUILD_ROOT/%{_prefix}/X11R6/man/man1/xvile.1  

./mkdirs.sh $RPM_BUILD_ROOT/%{_sysconfdir}/X11/wmconfig  
install vile.wmconfig $RPM_BUILD_ROOT/%{_sysconfdir}/X11/wmconfig/vile  
install xvile.wmconfig $RPM_BUILD_ROOT/%{_sysconfdir}/X11/wmconfig/xvile  
  
%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc CHANGES*
%doc COPYING INSTALL MANIFEST README*
%doc doc/*
%doc macros
%config(missingok) %{_sysconfdir}/X11/wmconfig/vile
%config(missingok) %{_sysconfdir}/X11/wmconfig/xvile
%{_prefix}/X11R6/bin/xvile
%{_prefix}/X11R6/man/man1/xvile.*
%{_bindir}/vile
%{_mandir}/man1/vile.*
%{_datadir}/vile
%{_libdir}/vile/

%changelog
# each patch should add its ChangeLog entries here

* Wed Oct 16 2002 Thomas Dickey
- added patch for 9.3e

* Sun Aug 25 2002 Thomas Dickey
- added patch for 9.3d

* Sun Aug 11 2002 Mark Milhollan <mlm@mlm.ath.cx>
- Updated spec file to follow the usual conventions (macros and defattr)

* Mon Nov 1 1999 Thomas Dickey
- modified based on spec by Tim Powers <timp@redhat.com>  
