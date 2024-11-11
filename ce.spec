#
# spec file for package arpus_ce
#
# Copyright  (c)  2007  Robert Styma
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# please send bugfixes or comments to stymar@cox.net
#

%define is_mandrake %(test -e /etc/mandrake-release && echo 1 || echo 0)
%define is_suse %(test -e /etc/SuSE-release && echo 1 || echo 0)
%define is_fedora %(test -e /etc/fedora-release && echo 1 || echo 0)

%define dist redhat
%define disttag rh

%if %is_mandrake
%define dist mandrake
%define disttag mdk
%endif
%if %is_suse
%define dist suse
%define disttag suse
%define kde_path /opt/kde3
%endif
%if %is_fedora
%define dist fedora
%define disttag rhfc
%endif

%define _bindir		%kde_path/bin
%define _datadir	%kde_path/share
%define _iconsdir	%_datadir/icons
%define _docdir		%_datadir/doc
%define _localedir	%_datadir/locale
%define qt_path		/usr/lib/qt3

%define distver %(release="`rpm -q --queryformat='%{VERSION}' %{dist}-release 2> /dev/null | tr . : | sed s/://g`" ; if test $? != 0 ; then release="" ; fi ; echo "$release")
%define distlibsuffix %(%_bindir/kde-config --libsuffix 2>/dev/null)
%define _lib lib%distlibsuffix
%define packer %(finger -lp `echo "$USER"` | head -n 1 | cut -d: -f 3)

Name:      arpus_ce
Icon:      kmymoney.xpm
Summary:   Arpus Ce text editor and  Ceterm terminal emulator
Version:   2.6.2
Release:   1.%{disttag}%{distver}
License:   GPL
Vendor:    Robert Styma <stymar@cox.net>
Packager:  %packer
Group:     TerminalEmulation/TextEditing
Source0:   %{name}2-%version.tar.bz2
BuildRoot: %{_tmppath}/%{name}2-%{version}-%{release}-build
BuildRequires: libX11-devel
Prereq: /sbin/ldconfig

%description
ARPUS/Ce is an integrated ascii text editor and X based terminal emulator modeled after the editor and terminal
emulator on Apollo Domain systems. The ce tool is a robust and extensible programmer's editor. The ceterm tool is
meant to be a replacement for vendor-supplied X-based terminal window programs such as IBM's "aixterm", HP's
"hpterm", DEC's "decterm", and Sun's "Shell Tool".   It is also suitable for use as a PAGER, replacing the
UNIX "more" command and supplying an interface which models the Apollo Domain systems.
The general reaction of former Apollo users to ARPUS/Ce is "Yes, this is what I was looking for."

For the most up-to-date information and sources please
visit the project web-site at http://styma.org/ce/ce/ce.html

%package devel
#Requires:
Summary: Ce development files
Group: Productivity/Office/Finance
Provides: kmymoney-devel

%description devel
This package contains necessary header files for KMyMoney development.

This package is necessary to compile plugins for KMyMoney.

%package ofx
Requires: kmymoney
Summary: KMyMoney OFX plugin
Group: Productivity/Office/Finance
Provides: kmymoney-ofx

%description ofx
This package contains necessary files for the KMyMoney OFX plugin.

 
%prep
#echo %_target
#echo %_target_alias
#echo %_target_cpu
#echo %_target_os
#echo %_target_vendor
echo Building %{name}-%{version}-%{release}

%setup -q -n %{name}2-%{version}

%build
CFLAGS="%optflags" CXXFLAGS="%{optflags}" \
        ./configure --mandir=%{_mandir}\
	            --disable-rpath \
		    --with-xinerama \
		    --without-gl \
		    --disable-debug \
		    --disable-cppunit \
		    --enable-final

make

%install
make DESTDIR=%buildroot install

%clean
[ ${RPM_BUILD_ROOT} != "/" ] && rm -rf ${RPM_BUILD_ROOT}

%post
cd %_docdir/HTML/*/%{name}2
ln -s ../common common
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-,root,root)

%dir %_docdir/HTML/en/%{name}2/
%doc %_docdir/HTML/*/%{name}2/*.docbook
%doc %_docdir/HTML/*/%{name}2/*.png
%doc %_docdir/HTML/*/%{name}2/index.cache.bz2

# the binary files
%{_bindir}/%{name}
%{_bindir}/%{name}2

# the shared libraries
%kde_path/%_lib/*.so.*.*.*

#
%dir %_datadir/apps/
%dir %_datadir/apps/%{name}2/
%dir %_datadir/apps/%{name}2/html
%dir %_datadir/apps/%{name}2/templates
%dir %_datadir/apps/%{name}2/templates/C
%dir %_datadir/apps/%{name}2/templates/de_DE
%dir %_datadir/apps/%{name}2/templates/en_GB
%dir %_datadir/apps/%{name}2/templates/en_US
%dir %_datadir/apps/%{name}2/templates/fr_FR
%dir %_datadir/apps/%{name}2/templates/pt_BR
%dir %_datadir/apps/%{name}2/templates/ru_SU
%_datadir/apps/%{name}2/templates/README
%_datadir/apps/%{name}2/templates/*/*.kmt

%_datadir/apps/%{name}2/*rc
%_datadir/apps/%{name}2/html/*
%_datadir/apps/%{name}2/tips

%dir %_datadir/apps/%{name}2/pics/
%_datadir/apps/%{name}2/pics/*.png

%dir %_datadir/apps/%{name}2/icons/
%dir %_datadir/apps/%{name}2/icons/hicolor/
%dir %_datadir/apps/%{name}2/icons/hicolor/16x16/
%dir %_datadir/apps/%{name}2/icons/hicolor/16x16/actions/
%dir %_datadir/apps/%{name}2/icons/hicolor/22x22/
%dir %_datadir/apps/%{name}2/icons/hicolor/22x22/actions/
%dir %_datadir/apps/%{name}2/icons/hicolor/32x32/
%dir %_datadir/apps/%{name}2/icons/hicolor/32x32/apps
%dir %_datadir/apps/%{name}2/icons/hicolor/48x48/
%dir %_datadir/apps/%{name}2/icons/hicolor/48x48/apps
%dir %_datadir/apps/%{name}2/icons/hicolor/64x64/
%dir %_datadir/apps/%{name}2/icons/hicolor/64x64/apps
%_datadir/apps/%{name}2/icons/hicolor/*/*/*.png

#
#
%_datadir/applications/kde/kmymoney2.desktop
%_datadir/mimelnk/application/x-kmymoney2.desktop
%_datadir/servicetypes/*

#
#
%_iconsdir/*/*/*/*.png

#
#
%doc %_mandir/man1/kmymoney2.1.gz

#
#
%_localedir/*/*/*.mo


# AqBanking plugin related files
# %dir %_datadir/apps/kmm_kbanking/
# %_datadir/apps/kmm_kbanking/*rc

# plugin related files
# %kde_path/%_lib/kde3/*.so




%files ofx
%_datadir/services/kmm_ofximport.desktop
%kde_path/%_lib/kde3/kmm_ofximport.so


%files devel
%kde_path/include/kmymoney/*
%kde_path/%_lib/*.la

# plugin related files
%kde_path/%_lib/kde3/*.la

%changelog
* Mon May 26 2005 - ipwizard (at) users.sourceforge.net
- Added kmymoney-ofx package

* Tue Mar 22 2005 - ipwizard (at) users.sourceforge.net
- Added more template functionality to provide more 
  distributions
- Added kmymoney-devel package

* Mon Jan 30 2005 - ipwizard (at) users.sourceforge.net
- Started adding distro independant layout

* Sat Oct 30 2004 - ipwizard (at) users.sourceforge.net
  * Preparations for 0.6.3

* Thu Sep 23 2004 - ipwizard (at) users.sourceforge.net
  * Preparations for 0.6.2

* Sat Jun  5 2004 - ipwizard (at) users.sourceforge.net
- Preparations for 0.6

* Thu Apr 22 2004 - ipwizard (at) users.sourceforge.net
- Preparations for 0.6rc4

* Fri Feb 20 2004 - ipwizard (at) users.sourceforge.net
- Removed the standard directories
- Uninstall the default account files also
- Preparations for 0.6rc3

* Thu Feb 5  2004 - ipwizard (at) users.sourceforge.net
- Remove CVS directories from SRPMS
- Preparations for 0.6rc2

* Mon Dec 29 2003 - ipwizard (at) users.sourceforge.net
- Preparations for 0.6rc1
- Incorporated some changes from SuSE distro
- added man page file

* Thu Jan 9 2003 - ipwizard (at) users.sourcforge.net
- Added missing files home.html and kmymoney2.css

* Mon Dec 16 2002 - ipwizard (at) users.sourcforge.net
- Removed make command only required for CVS download
- Update version to match filename

* Sun Dec 15 2002 - ipwizard (at) users.sourcforge.net
- Updated for version 0.51

* Tue Jan 15 2002 - ipwizard (at) users.sourcforge.net
- Initial implementation



