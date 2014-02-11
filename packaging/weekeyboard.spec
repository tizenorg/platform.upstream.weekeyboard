%bcond_with wayland

Name:           weekeyboard
Version:        0.0.2
Release:        1
License:        Apache-2.0
Summary:        Virtual Keyboard Application
Url:            http://github.com/etrunko/weekeyboard
Group:          Graphics & UI Framework/Libraries
Source0:        weekeyboard-%{version}.tar.bz2
Source1001:     weekeyboard.manifest

BuildRequires:  pkgconfig(eina)
BuildRequires:  pkgconfig(eet)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(ecore-evas)
BuildRequires:  pkgconfig(ecore-wayland)
BuildRequires:  pkgconfig(edje)
BuildRequires:  pkgconfig(eet)
BuildRequires:  pkgconfig(efreet)
BuildRequires:  pkgconfig(eldbus)
BuildRequires:  ibus

Requires:       ibus
Requires:       ibus-hangul
Requires:       ibus-libpinyin

%if !%{with wayland}
ExclusiveArch:
%endif

%description
Weekeyboard is virtual keyboard application written in EFL and
made for Wayland compositors.
%prep
%setup -q
cp %{SOURCE1001} .

%build

%configure --disable-ibus
make %{?_smp_mflags}

%install
%make_install

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%license COPYING
%{_bindir}/weekeyboard
%{_datadir}/weekeyboard/*.edj

%changelog

