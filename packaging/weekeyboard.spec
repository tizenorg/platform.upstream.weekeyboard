Name:           weekeyboard
Version:        0.0.1
Release:        2
License:        Apache-2.0
Summary:        Virtual Keyboard Application
Url:            http://www.enlightenment.org/
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


%post

%postun


%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%license COPYING
%{_bindir}/weekeyboard
%{_datadir}/weekeyboard/*.edj

%changelog

