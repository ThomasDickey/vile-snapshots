Summary: VILE (VI Like Emacs) editor
# $Header: /users/source/archives/vile.vcs/package/RCS/vile.spec,v 1.36 2012/10/16 09:22:33 tom Exp $
Name: vile
%define AppVersion 9.8
Version: %{AppVersion}j
# each patch should update the version
Release: dev
License: GPLv2
Group: Applications/Editors
URL: ftp://invisible-island.net/vile
Source0: vile-9.8.tgz
Patch1: vile-9.8a.patch.gz
Patch2: vile-9.8b.patch.gz
Patch3: vile-9.8c.patch.gz
Patch4: vile-9.8d.patch.gz
Patch5: vile-9.8e.patch.gz
Patch6: vile-9.8f.patch.gz
Patch7: vile-9.8g.patch.gz
Patch8: vile-9.8h.patch.gz
Patch9: vile-9.8i.patch.gz
Patch10: vile-9.8j.patch.gz
# each patch should add itself to this list
Packager: Thomas Dickey <dickey@invisible-island.net>

BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires:	%{name}-common = %{version}-%{release}

%prep

%define desktop_vendor  dickey
%define desktop_utils   %(if which desktop-file-install 2>&1 >/dev/null ; then echo "yes" ; fi)

%define icon_theme  %(test -d /usr/share/icons/hicolor && echo 1 || echo 0)
%define apps_shared %(test -d /usr/share/X11/app-defaults && echo 1 || echo 0)
%define apps_syscnf %(test -d /etc/X11/app-defaults && echo 1 || echo 0)

# loadable filters are mistreated by rpmbuild
%define debug_package %{nil}

%if %{apps_shared}
%define _xresdir    %{_datadir}/X11/app-defaults
%else
%define _xresdir    %{_sysconfdir}/X11/app-defaults
%endif

%define _iconsdir   %{_datadir}/icons
%define _pixmapsdir %{_datadir}/pixmaps
%define _wmcfgdir   %{_sysconfdir}/X11/wmconfig

# rpm5 came too late to be more than a niche.  Use environment variables to
# override configure settings.
%define without_perl %(test -n "$_without_perl" && echo 1 || echo 0)
%if %{without_perl} == 1
%define perl_opt --without-perl
%define with_perl 0
%else
%define perl_opt --with-perl
%define with_perl 1
%endif

%setup -q -n %{name}-%{AppVersion}
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

rpm --version

# help rpmbuild to ignore the maintainer scripts in the doc directory...
rm -f doc/makefile
rm -f doc/*.pl
rm -f doc/*.sh

%package	common
Summary:	The common files needed by any version of VILE (VI Like Emacs)
Group:		Applications/Editors

%package -n	xvile
Summary:	XVILE (VI Like Emacs for X11)
Group:		Applications/Editors

Requires:	%{name}-common = %{version}-%{release}
#Requires:	xorg-x11-fonts-misc

%description	common
vile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%description -n xvile
xvile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%description
vile is a text editor which is extremely compatible with vi in terms of
"finger feel".  In addition, it has extended capabilities in many areas,
notably multi-file editing and viewing, syntax highlighting, key
rebinding, and real X window system support.

%build
%configure --verbose \
	--with-loadable-filters \
	--disable-rpath-hack %{perl_opt}
make vile

%configure --verbose \
	--with-loadable-filters \
	--disable-rpath-hack \
	--with-app-defaults=%{_xresdir} \
	--with-icondir=%{_icondir} \
	--with-pixmapdir=%{_pixmapsdir} \
	--with-screen=Xaw \
%if "%{icon_theme}"
	--with-icon-theme \
	--with-icondir=%{_iconsdir} \
%endif
	--with-xpm %{perl_opt}
make xvile
touch vile

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot} INSTALL='install -p' TARGET='xvile'
make install DESTDIR=%{buildroot} INSTALL='install -p' TARGET='vile'

# There is no possible cross-version check possible for rpm to filter this,
# and it would make the dependencies hard to satisfy - remove it.
rm -f %{buildroot}/%{_datadir}/vile/perl/dict.pm

%if "%{desktop_utils}" == "yes"
make install-desktop            DESKTOP_FLAGS="--vendor='%{desktop_vendor}' --dir %{buildroot}%{_datadir}/applications"
%endif

mkdir -p %{buildroot}/%{_wmcfgdir}
install vile.wmconfig %{buildroot}%{_wmcfgdir}/vile
install xvile.wmconfig %{buildroot}%{_wmcfgdir}/xvile

MY_MANDIR=%{buildroot}%{_mandir}/man1
for alias in uxvile lxvile
do
	for name in $MY_MANDIR/xvile.1*
	do
		ls -l "$name"
		if test -f "$name"
		then
			rename=`basename "$name" | sed -e 's,xvile\.,'$alias'\.,'`
			( cd $MY_MANDIR && ln -s `basename $name` $rename )
		fi
	done
done

%post
%if "%{icon_theme}"
touch --no-create %{_iconsdir}/hicolor
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
  %{_bindir}/gtk-update-icon-cache %{_iconsdir}/hicolor || :
fi
%endif

%postun
%if "%{icon_theme}"
touch --no-create %{_iconsdir}/hicolor
if [ -x %{_bindir}/gtk-update-icon-cache ]; then
  %{_bindir}/gtk-update-icon-cache %{_iconsdir}/hicolor || :
fi
%endif

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_bindir}/vile
%{_bindir}/vile-pager
%{_mandir}/man1/vile.1*

%files common
%defattr(-,root,root,-)
%doc AUTHORS COPYING CHANGES README doc/*doc
%{_datadir}/vile/
%{_libdir}/vile/

%files -n xvile
%defattr(-,root,root,-)
%{_bindir}/lxvile
%{_bindir}/lxvile-fonts
%{_bindir}/uxvile
%{_bindir}/xshell.sh
%{_bindir}/xvile
%{_bindir}/xvile-pager
%if %{with_perl} == 1
%{_bindir}/vileget
%endif
%{_mandir}/man1/xvile.1*
%{_mandir}/man1/lxvile.1*
%{_mandir}/man1/uxvile.1*

%if "%{icon_theme}"
# should have 2 files, but patch will not work for png-file
%{_iconsdir}/hicolor/*/apps/vile.*
%endif
%{_pixmapsdir}/vile.xpm

%{_xresdir}/XVile
%{_xresdir}/UXVile

%config(missingok) %{_wmcfgdir}/vile
%config(missingok) %{_wmcfgdir}/xvile

%if "%{desktop_utils}" == "yes"
%config(missingok) %{_datadir}/applications/%{desktop_vendor}-lxvile.desktop
%config(missingok) %{_datadir}/applications/%{desktop_vendor}-uxvile.desktop
%config(missingok) %{_datadir}/applications/%{desktop_vendor}-xvile.desktop
%endif

%changelog
# each patch should add its ChangeLog entries here

* Tue Oct 16 2012 Thomas Dickey
- added patch for 9.8j

* Mon Aug 20 2012 Thomas Dickey
- added patch for 9.8i

* Sun Mar 12 2012 Thomas Dickey
- added patch for 9.8h

* Sun Dec 11 2011 Thomas Dickey
- added patch for 9.8g

* Sat Nov 26 2011 Thomas Dickey
- modified scheme of sym-linking the uxvile and lxvile manpages to ensure
  relative links, since absolute links may be automatically removed in
  rpmbuild.

* Tue Jul 19 2011 Thomas Dickey
- adapt scheme in Fedora spec-file for providing separate RPMs.

* Mon Jul 18 2011 Thomas Dickey
- add symlink for lxvile manpage; use symlink for xvile manpage.

* Mon Apr 11 2011 Thomas Dickey
- added patch for 9.8f

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

