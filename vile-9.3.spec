Summary: VILE VI Like Emacs editor
# $Header: /users/source/archives/vile.vcs/RCS/vile-9.3.spec,v 1.15 2003/05/17 14:22:23 tom Exp $
Name: vile
Version: 9.3q
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
Patch6: vile-9.3f.patch.gz
Patch7: vile-9.3g.patch.gz
Patch8: vile-9.3h.patch.gz
Patch9: vile-9.3i.patch.gz
Patch10: vile-9.3j.patch.gz
Patch11: vile-9.3k.patch.gz
Patch12: vile-9.3l.patch.gz
Patch13: vile-9.3m.patch.gz
Patch14: vile-9.3n.patch.gz
Patch15: vile-9.3o.patch.gz
Patch16: vile-9.3p.patch.gz
Patch17: vile-9.3q.patch.gz
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
%patch6 -p1
%patch7 -p1
%patch8 -p1
%patch9 -p1
%patch10 -p1
%patch11 -p1
%patch12 -p1
%patch13 -p1
%patch14 -p1
%patch15 -p1
%patch16 -p1
%patch17 -p1
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

* Sat May 17 2003 Thomas Dickey
- added patch for 9.3q

* Tue May 06 2003 Thomas Dickey
- added patch for 9.3p

* Sat May 03 2003 Thomas Dickey
- added patch for 9.3o

* Fri Mar 18 2003 Thomas Dickey
- added patch for 9.3n

* Fri Mar 07 2003 Thomas Dickey
- added patch for 9.3m

* Wed Feb 26 2003 Thomas Dickey
- added patch for 9.3l

* Mon Feb 17 2003 Thomas Dickey
- added patch for 9.3k

* Thu Jan 02 2003 Thomas Dickey
- added patch for 9.3j

* Wed Dec 03 2002 Thomas Dickey
- added patch for 9.3i

* Wed Oct 29 2002 Thomas Dickey
- added patch for 9.3h

* Wed Oct 26 2002 Thomas Dickey
- added patch for 9.3g

* Wed Oct 20 2002 Thomas Dickey
- added patch for 9.3f

* Wed Oct 16 2002 Thomas Dickey
- added patch for 9.3e

* Sun Aug 25 2002 Thomas Dickey
- added patch for 9.3d

* Sun Aug 11 2002 Mark Milhollan <mlm@mlm.ath.cx>
- Updated spec file to follow the usual conventions (macros and defattr)

* Mon Nov 1 1999 Thomas Dickey
- modified based on spec by Tim Powers <timp@redhat.com>  
