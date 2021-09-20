%global pgmajorversion 96
%global pgpackageversion 9.6
%global pginstdir /usr/pgsql-%{pgpackageversion}
%global sname	pgexportdoc

Summary:	command line tool for export XML, TEXT and BYTEA documents from PostgreSQL
Name:		pgexportdoc
Version:	0.1.4
Release:	1%{?dist}
License:	BSD
Group:		Applications/Databases
Source0:	https://github.com/okbob/%{name}/archive/v%{version}.tar.gz
Patch0:		%{name}-makefile.patch
URL:		https://github.com/okbob/%{name}
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:	postgresql%{pgmajorversion}-devel, postgresql%{pgmajorversion}
Requires:	postgresql%{pgmajorversion}

%description
pgexportdoc is command line tool for user friendly export XML, TEXT, and
BYTEA documents from PostgreSQL.

%prep
%setup -q
%patch0 -p0

%build
USE_PGXS=1 %{__make} %{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}

install -d %{buildroot}%{_bindir}
USE_PGXS=1 %{__make} %{?_smp_mflags} DESTDIR=%{buildroot} install

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,-)
%if 0%{?rhel} && 0%{?rhel} <= 6
%doc README.md COPYRIGHT
%else
%doc README.md
%license LICENCE
%endif

%{_bindir}/%{name}

%changelog
* Mon Sep 20 2021 - Pavel Stehule <pavel.stehule@gmail.com> 0.1.4-1
- fix build for PostgreSQL 14

* Tue Feb 21 2017 - Pavel Stehule <pavel.stehule@gmail.com> 0.1.1-1
- Initial RPM packaging for PostgreSQL RPM Repository
