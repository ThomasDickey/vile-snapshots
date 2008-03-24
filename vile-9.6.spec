Summary: VILE VI Like Emacs editor
# $Header: /users/source/archives/vile.vcs/RCS/vile-9.6.spec,v 1.11 2008/03/19 22:00:30 tom Exp $
Name: vile
Version: 9.6j
# each patch should update the version
Release: 1
License: GPLv2
Group: Applications/Editors
URL: ftp://invisible-island.net/vile
Source0: vile-9.6.tgz
Patch1: vile-9.6a.patch.gz
Patch2: vile-9.6b.patch.gz
Patch3: vile-9.6c.patch.gz
Patch4: vile-9.6d.patch.gz
Patch5: vile-9.6e.patch.gz
Patch6: vile-9.6f.patch.gz
Patch7: vile-9.6g.patch.gz
Patch8: vile-9.6h.patch.gz
Patch9: vile-9.6i.patch.gz
Patch10: vile-9.6j.patch.gz
# each patch should add itself to this list
Packager: Thomas Dickey <dickey@invisible-island.net>
BuildRoot: %{_tmppath}/%{name}-root

%description
Vile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%prep
%setup -q -n vile-9.6
%patch1 -p1
%patch2 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
%patch6 -p1
%patch7 -p1
%patch8 -p1
%patch9 -p1
%patch10 -p1
# each patch should add itself to this list

%build
EXTRA_CFLAGS="$RPM_OPT_FLAGS" INSTALL_PROGRAM='${INSTALL} -s' ./configure --target %{_target_platform} --prefix=%{_prefix} --with-locale --with-builtin-filters --with-screen=x11 --with-xpm
make xvile
EXTRA_CFLAGS="$RPM_OPT_FLAGS" INSTALL_PROGRAM='${INSTALL} -s' ./configure --target %{_target_platform} --prefix=%{_prefix} --with-locale --with-builtin-filters --with-screen=ncurses --mandir=%{_mandir}
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
./mkdirs.sh $RPM_BUILD_ROOT/%{_mandir}/man1  
install -m 644 vile.1 $RPM_BUILD_ROOT/%{_mandir}/man1/vile.1  

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
%{_prefix}/X11R6/bin/vile-pager
%{_prefix}/X11R6/man/man1/xvile.*
%{_prefix}/X11R6/share/vile
%{_bindir}/vile
%{_bindir}/vile-pager
%{_mandir}/man1/vile.*
%{_datadir}/vile/
%{_libdir}/vile/

%changelog
# each patch should add its ChangeLog entries here

* Wed Mar 19 2008 Thomas Dickey
- added patch for 9.6j

* Wed Mar 12 2008 Thomas Dickey
- added patch for 9.6i

* Wed Mar 05 2008 Thomas Dickey
- added patch for 9.6h

* Fri Feb 22 2008 Thomas Dickey
- added patch for 9.6g

* Sat Feb 09 2008 Thomas Dickey
- added patch for 9.6f

* Mon Feb 04 2008 Thomas Dickey
- added patch for 9.6e

* Sat Jan 12 2008 Thomas Dickey
- added patch for 9.6d

* Sat Jan 12 2008 Thomas Dickey
- added patch for 9.6c

* Sun Jan 06 2008 Thomas Dickey
- added patch for 9.6b

* Mon Dec 31 2007 Thomas Dickey
- added patch for 9.6a

* Tue Dec 27 2007 Thomas Dickey
- 9.6 release

* Tue Dec 25 2007 Thomas Dickey
- added patch for 9.6
