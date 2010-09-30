Name:		sodium
Version:	001
Release:	1%{?dist}
Summary:	GUI DVD cover art browser and movie launcher

Group:		Applications/Multimedia
License:	GPLv2
URL:		http://github.com/ac000/sodium
Source0:	sodium-%{version}.tar
BuildRoot:	%(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

BuildRequires:	clutter-devel glib2-devel
Requires:	clutter glib2

%description
sodium is Clutter based GUI DVD cover art browser and movie launcher

%prep
%setup -q


%build
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
install -Dp -m0755 sodium $RPM_BUILD_ROOT/%{_bindir}/sodium
install -Dp -m0644 sodium.1.gz $RPM_BUILD_ROOT/%{_mandir}/man1/sodium.1.gz


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
%doc README COPYING
%{_bindir}/sodium
%{_mandir}/man1/sodium.1.gz


%changelog
* Thu Sep 30 2010 Andrew Clayton <andrew@digital-domain.net> - 001-1
- Initial version
