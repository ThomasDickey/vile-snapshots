Summary: VILE VI Like Emacs editor
# $Header: /users/source/archives/vile.vcs/RCS/vile-9.4.spec,v 1.24 2005/05/23 23:18:06 tom Exp $
Name: vile
Version: 9.4v
# each patch should update the version
Release: 1
Copyright: GPL
Group: Applications/Editors
URL: ftp://invisible-island.net/vile
Source0: vile-9.4.tgz
Patch1: vile-9.4a.patch.gz
Patch2: vile-9.4b.patch.gz
Patch3: vile-9.4c.patch.gz
Patch4: vile-9.4d.patch.gz
Patch5: vile-9.4e.patch.gz
Patch6: vile-9.4f.patch.gz
Patch7: vile-9.4g.patch.gz
Patch8: vile-9.4h.patch.gz
Patch9: vile-9.4i.patch.gz
Patch10: vile-9.4j.patch.gz
Patch11: vile-9.4k.patch.gz
Patch12: vile-9.4l.patch.gz
Patch13: vile-9.4m.patch.gz
Patch14: vile-9.4n.patch.gz
Patch15: vile-9.4o.patch.gz
Patch16: vile-9.4p.patch.gz
Patch17: vile-9.4q.patch.gz
Patch18: vile-9.4r.patch.gz
Patch19: vile-9.4s.patch.gz
Patch20: vile-9.4t.patch.gz
Patch21: vile-9.4u.patch.gz
Patch22: vile-9.4v.patch.gz
# each patch should add itself to this list
Packager: Thomas Dickey <dickey@invisible-island.net>
BuildRoot: %{_tmppath}/%{name}-root

%description
Vile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%prep
%setup -q -n vile-9.4
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
%patch22 -p1
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

* Mon May 23 2005 Thomas Dickey
- added patch for 9.4v

* Wed May 18 2005 Thomas Dickey
- added patch for 9.4u

* Wed May 18 2005 Thomas Dickey
- added patch for 9.4t

* Tue Mar 15 2005 Thomas Dickey
- added patch for 9.4s

* Tue Feb 15 2005 Thomas Dickey
- added patch for 9.4r

* Mon Feb 07 2005 Thomas Dickey
- added patch for 9.4q

* Tue Jan 25 2005 Thomas Dickey
- added patch for 9.4p

* Mon Jan 24 2005 Thomas Dickey
- added patch for 9.4o

* Wed Jan 19 2005 Thomas Dickey
- added patch for 9.4n

* Thu Dec 09 2004 Thomas Dickey
- added patch for 9.4m

* Mon Nov 09 2004 Thomas Dickey
- added patch for 9.4l

* Mon Nov 01 2004 Thomas Dickey
- added patch for 9.4k

* Fri Oct 22 2004 Radek Liboska
- add vile-pager to list of installed files
- install vile.1 in ${mandir}/man1/

* Thu Sep 02 2004 Thomas Dickey
- added patch for 9.4j

* Sun Aug 08 2004 Thomas Dickey
- added patch for 9.4i

* Sat Jun 19 2004 Thomas Dickey
- added patch for 9.4h

* Thu May 13 2004 Thomas Dickey
- added patch for 9.4g

* Mon Mar 22 2004 Thomas Dickey
- added patch for 9.4f

* Mon Mar 22 2004 Thomas Dickey
- added patch for 9.4e

* Tue Nov 12 2003 Thomas Dickey
- added patch for 9.4d

* Tue Nov 04 2003 Thomas Dickey
- added patch for 9.4c

* Sun Nov 02 2003 Thomas Dickey
- added patch for 9.4b

* Tue Sep 16 2003 Thomas Dickey
- added patch for 9.4a

* Mon Aug 04 2003 Thomas Dickey
- 9.4 release

