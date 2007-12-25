Summary: VILE VI Like Emacs editor
# $Header: /users/source/archives/vile.vcs/RCS/vile-9.5.spec,v 1.23 2007/12/18 00:22:57 tom Exp $
Name: vile
Version: 9.5u
# each patch should update the version
Release: 1
Copyright: GPL
Group: Applications/Editors
URL: ftp://invisible-island.net/vile
Source0: vile-9.5.tgz
Patch1: vile-9.5a.patch.gz
Patch2: vile-9.5b.patch.gz
Patch3: vile-9.5c.patch.gz
Patch4: vile-9.5d.patch.gz
Patch5: vile-9.5e.patch.gz
Patch6: vile-9.5f.patch.gz
Patch7: vile-9.5g.patch.gz
Patch8: vile-9.5h.patch.gz
Patch9: vile-9.5i.patch.gz
Patch10: vile-9.5j.patch.gz
Patch11: vile-9.5k.patch.gz
Patch12: vile-9.5l.patch.gz
Patch13: vile-9.5m.patch.gz
Patch14: vile-9.5n.patch.gz
Patch15: vile-9.5o.patch.gz
Patch16: vile-9.5p.patch.gz
Patch17: vile-9.5q.patch.gz
Patch18: vile-9.5r.patch.gz
Patch19: vile-9.5s.patch.gz
Patch20: vile-9.5t.patch.gz
Patch21: vile-9.5u.patch.gz
# each patch should add itself to this list
Packager: Thomas Dickey <dickey@invisible-island.net>
BuildRoot: %{_tmppath}/%{name}-root

%description
Vile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%prep
%setup -q -n vile-9.5
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
%patch18 -p1
%patch19 -p1
%patch20 -p1
%patch21 -p1
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

* Mon Dec 17 2007 Thomas Dickey
- added patch for 9.5u

* Mon Nov 26 2007 Thomas Dickey
- added patch for 9.5t

* Sat Jun 16 2007 Thomas Dickey
- added patch for 9.5s

* Sat Jun 02 2007 Thomas Dickey
- added patch for 9.5r

* Fri May 04 2007 Thomas Dickey
- added patch for 9.5q

* Sun Feb 11 2007 Thomas Dickey
- added patch for 9.5p

* Sun Jan 14 2007 Thomas Dickey
- added patch for 9.5o

* Thu Dev 15 2006 Thomas Dickey
- added patch for 9.5n

* Thu Nov 23 2006 Thomas Dickey
- added patch for 9.5m

* Mon Oct 02 2006 Thomas Dickey
- added patch for 9.5l

* Sun Aug 13 2006 Thomas Dickey
- added patch for 9.5k

* Sat Jun 03 2006 Thomas Dickey
- added patch for 9.5j

* Mon May 22 2006 Thomas Dickey
- added patch for 9.5i

* Wed Apr 05 2006 Thomas Dickey
- added patch for 9.5h

* Tue Mar 14 2006 Thomas Dickey
- added patch for 9.5g

* Tue Jan 10 2006 Thomas Dickey
- added patch for 9.5f

* Tue Jan 10 2006 Thomas Dickey
- added patch for 9.5e

* Thu Nov 24 2005 Thomas Dickey
- added patch for 9.5d

* Thu Nov 24 2005 Thomas Dickey
- added patch for 9.5c

* Thu Sep 22 2005 Thomas Dickey
- added patch for 9.5b

* Sun Aug 29 2005 Thomas Dickey
- added patch for 9.5a

* Sun Jul 24 2005 Thomas Dickey
- 9.5 release

