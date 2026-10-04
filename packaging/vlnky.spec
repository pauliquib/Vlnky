# SPDX-FileCopyrightText: 2026 Pavel Švec
# SPDX-License-Identifier: GPL-2.0-or-later
Name:           vlnky
Version:        1.5.0
Release:        1%{?dist}
Summary:        Vlnky animated wave wallpaper for KDE Plasma 6

License:        GPL-2.0-or-later
URL:            https://svec-elektro.cz/projekty/psp-xmb-wave/
Source0:        https://svec-elektro.cz/files/%{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtbase-private-devel
BuildRequires:  qt6-qtdeclarative-devel
BuildRequires:  qt6-qtshadertools-devel
BuildRequires:  zlib-devel
Requires:       plasma-workspace
Requires:       qt6-qtbase
Requires:       qt6-qtdeclarative

%description
KDE Plasma 6 wallpaper plugin that renders PSP-style XMB animated waves from
system_plugin_bg.rco files supplied by the user, with XMB month colours,
a PS2 (PSX DESR) style wave and high-resolution post-processing.
No Sony data is included.

%prep
%autosetup

%build
%cmake -DVLNKY_COPY_USER_QML_MODULE=OFF
%cmake_build

%check
%ctest

%install
install -d %{buildroot}%{_datadir}/plasma/wallpapers/org.psvec.vlnky
cp -a %{__cmake_builddir}/plasma-wallpaper/package/* %{buildroot}%{_datadir}/plasma/wallpapers/org.psvec.vlnky/

%files
%license COPYING NOTICE
%doc README.md docs/INSTALL.md docs/NAVOD.cs.md
%{_datadir}/plasma/wallpapers/org.psvec.vlnky

%changelog
* Thu Oct  2 2026 pauliquib - 1.5.0-1
- Svec Studio wave style (Sencurio landing hero waves), custom wave colour

* Tue Sep 29 2026 pauliquib - 1.4.1-1
- Custom XMB colour option, fixed empty preview in settings

* Mon Sep 28 2026 pauliquib - 1.4.0-1
- Post-processing pipeline, XMB colours, PS2 style, original wave import
