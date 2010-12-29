Summary: VILE VI Like Emacs editor
# $Header: /users/source/archives/vile.vcs/package/RCS/vile.spec,v 1.9 2010/12/29 18:56:08 tom Exp $
Name: vile
Version: 9.8e
# each patch should update the version
Release: 1
License: GPLv2
Group: Applications/Editors
URL: ftp://invisible-island.net/vile
Source0: vile-9.8.tgz
Patch1: vile-9.8a.patch.gz
Patch2: vile-9.8b.patch.gz
Patch3: vile-9.8c.patch.gz
Patch4: vile-9.8d.patch.gz
Patch5: vile-9.8e.patch.gz
# each patch should add itself to this list
Packager: Thomas Dickey <dickey@invisible-island.net>
# BuildRoot: %{_tmppath}/%{name}-root

%description
Vile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%prep

%define desktop_vendor  dickey
%define desktop_utils   %(if which desktop-file-install 2>&1 >/dev/null ; then echo "yes" ; fi)

%define apps_shared %(test -d /usr/share/X11/app-defaults && echo 1 || echo 0)
%define apps_syscnf %(test -d /etc/X11/app-defaults && echo 1 || echo 0)

%if %{apps_shared}
%define _xresdir    %{_datadir}/X11/app-defaults
%else
%define _xresdir    %{_sysconfdir}/X11/app-defaults
%endif

%define _iconsdir   %{_datadir}/icons
%define _pixmapsdir %{_datadir}/pixmaps

%setup -q -n vile-9.8
%patch1 -p1
%patch2 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
# each patch should add itself to this list

# help rpmbuild to ignore the maintainer scripts in the doc directory...
rm -f doc/makefile
rm -f doc/*.pl
rm -f doc/*.sh

%build

VILE_LIBDIR_PATH=%{_libdir}/vile \
EXTRA_CFLAGS="$RPM_OPT_FLAGS" \
INSTALL_PROGRAM='${INSTALL}' \
	./configure \
		--disable-rpath-hack \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--bindir=%{_bindir} \
		--libdir=%{_libdir} \
		--mandir=%{_mandir} \
		--with-locale \
		--with-builtin-filters
make vile

VILE_LIBDIR_PATH=%{_libdir}/vile \
EXTRA_CFLAGS="$RPM_OPT_FLAGS" \
INSTALL_PROGRAM='${INSTALL}' \
	./configure \
		--disable-rpath-hack \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--bindir=%{_bindir} \
		--libdir=%{_libdir} \
		--mandir=%{_mandir} \
		--with-app-defaults=%{_xresdir} \
		--with-icondir=%{_pixmapsdir} \
		--with-locale \
		--with-builtin-filters \
		--with-screen=Xaw \
		--with-xpm

touch tcap.o
make
touch vile

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install                    DESTDIR=$RPM_BUILD_ROOT
make install-app                DESTDIR=$RPM_BUILD_ROOT
make install-icon               DESTDIR=$RPM_BUILD_ROOT
make install-bin   TARGET=vile  DESTDIR=$RPM_BUILD_ROOT

%if "%{desktop_utils}" == "yes"
make install-desktop            DESKTOP_FLAGS="--vendor='%{desktop_vendor}' --dir $RPM_BUILD_ROOT%{_datadir}/applications"
%endif

strip $RPM_BUILD_ROOT%{_bindir}/xvile
strip $RPM_BUILD_ROOT%{_bindir}/vile
strip $RPM_BUILD_ROOT%{_libdir}/vile/atr2html
strip $RPM_BUILD_ROOT%{_libdir}/vile/vile-crypt
strip $RPM_BUILD_ROOT%{_libdir}/vile/vile-manfilt
strip $RPM_BUILD_ROOT%{_libdir}/vile/atr2ansi
strip $RPM_BUILD_ROOT%{_libdir}/vile/atr2text

mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1
install -m 644 vile.1 $RPM_BUILD_ROOT%{_mandir}/man1/xvile.1
install -m 644 vile.1 $RPM_BUILD_ROOT%{_mandir}/man1/vile.1

mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/X11/wmconfig
install vile.wmconfig $RPM_BUILD_ROOT%{_sysconfdir}/X11/wmconfig/vile
install xvile.wmconfig $RPM_BUILD_ROOT%{_sysconfdir}/X11/wmconfig/xvile

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
%{_prefix}/bin/xvile
%{_prefix}/bin/uxvile
%{_prefix}/bin/lxvile
%{_prefix}/bin/lxvile-fonts
%{_prefix}/bin/xvile-pager
%{_prefix}/bin/xshell.sh
%{_bindir}/vile
%{_bindir}/vile-pager
%{_mandir}/man1/xvile.*
%{_mandir}/man1/vile.*
%{_datadir}/vile/
%{_pixmapsdir}/vile.xpm
%{_libdir}/vile/
%{_xresdir}/XVile
%{_xresdir}/UXVile

%if "%{desktop_utils}" == "yes"
%config(missingok) %{_datadir}/applications/%{desktop_vendor}-lxvile.desktop
%config(missingok) %{_datadir}/applications/%{desktop_vendor}-uxvile.desktop
%config(missingok) %{_datadir}/applications/%{desktop_vendor}-xvile.desktop
%endif

%changelog
# each patch should add its ChangeLog entries here

* Wed Dec 29 2010 Thomas Dickey
- added patch for 9.8e

* Sun Dec 12 2010 Thomas Dickey
- added patch for 9.8d

* Wed Nov 10 2010 Thomas Dickey
- added patch for 9.8c

* Thu Sep 09 2010 Thomas Dickey
- added patch for 9.8b

* Mon Sep 06 2010 Thomas Dickey
- rpmbuild is confused by doc/makefile, adding dependencies on the relatively
  nonportable Perl scripts used to generate table of contents.  Workaround by
  removing those from the build-tree, to eliminate spurious dependencies that
  interferes with installing the RPM.

* Thu Aug 12 2010 Thomas Dickey
- added patch for 9.8a

* Sun Aug 08 2010 Thomas Dickey
- added patch for 9.8

* Sun Jul 25 2010 Thomas Dickey
- added patch for 9.7zg

* Tue Jul 06 2010 Thomas Dickey
- added patch for 9.7zf

* Fri Jun 11 2010 Thomas Dickey
- add lxvile and lxvile-fonts

* Thu Jun 10 2010 Thomas Dickey
- added patch for 9.7ze

* Sun Apr 11 2010 Thomas Dickey
- added patch for 9.7zd

* Tue Mar 02 2010 Thomas Dickey
- use termcap/terminfo driver rather than ncursesw
  also minor cleanup.

* Fri Feb 12 2010 Thomas Dickey
- added patch for 9.7zc

* Fri Jan 29 2010 Thomas Dickey
- added patch for 9.7zb

* Mon Jan 25 2010 Thomas Dickey
- remove obsolete use of /usr/X11R6 (report by Radek Liboska).

* Sat Jan 09 2010 Thomas Dickey
- added patch for 9.7za

* Tue Dec 29 2009 Thomas Dickey
- added patch for 9.7z

* Tue Dec 08 2009 Thomas Dickey
- added patch for 9.7y

* Sat Oct 31 2009 Thomas Dickey
- added patch for 9.7x

* Wed Oct 14 2009 Thomas Dickey
- added patch for 9.7w

* Wed Sep 02 2009 Thomas Dickey
- added patch for 9.7v

* Sat Jul 04 2009 Thomas Dickey
- added patch for 9.7u

* Fri Jun 19 2009 Thomas Dickey
- added patch for 9.7t

* Tue May 26 2009 Thomas Dickey
- added patch for 9.7s

* Fri May 19 2009 Thomas Dickey
- added patch for 9.7r

* Fri May 01 2009 Thomas Dickey
- added patch for 9.7q

* Wed Apr 29 2009 Thomas Dickey
- added patch for 9.7p

* Sun Apr 19 2009 Thomas Dickey
- added patch for 9.7o

* Mon Mar 30 2009 Thomas Dickey
- added patch for 9.7n

* Wed Mar 11 2009 Thomas Dickey
- added patch for 9.7m

* Thu Feb 19 2009 Thomas Dickey
- added patch for 9.7l

* Sun Dec 21 2008 Thomas Dickey
- added patch for 9.7k

* Wed Dec 03 2008 Thomas Dickey
- added patch for 9.7j

* Sun Nov 16 2008 Thomas Dickey
- added patch for 9.7i

* Thu Nov 06 2008 Thomas Dickey
- added patch for 9.7h

* Mon Oct 21 2008 Thomas Dickey
- added patch for 9.7g

* Mon Sep 29 2008 Thomas Dickey
- added patch for 9.7f

* Tue Aug 19 2008 Thomas Dickey
- added patch for 9.7e

* Wed Jul 29 2008 Thomas Dickey
- added patch for 9.7d

* Wed Jul 16 2008 Thomas Dickey
- added patch for 9.7c

* Wed Jun 26 2008 Thomas Dickey
- added patch for 9.7b

* Sun Jun 22 2008 Thomas Dickey
- added patch for 9.7a

* Sat Jun 14 2008 Thomas Dickey
- release 9.7

* Mon May 26 2008 Thomas Dickey
- added patch for 9.6o

* Sun May 25 2008 Thomas Dickey
- added patch for 9.6n

* Sun Apr 27 2008 Thomas Dickey
- added patch for 9.6m

* Mon Apr 14 2008 Thomas Dickey
- added patch for 9.6l

* Wed Mar 26 2008 Thomas Dickey
- added patch for 9.6k

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

