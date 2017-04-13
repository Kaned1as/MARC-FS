Name:		fuse-marcfs
Version:	0.3
Release:	1%{?dist}
Summary:	FUSE filesystem for Mail.ru Cloud storage

License:	GPLv3+
URL:		https://gitlab.com/Kanedias/MARC-FS
Source0:	fuse-marcfs-%{version}.tar.bz2

BuildRequires:  cmake
BuildRequires:	gcc-c++
BuildRequires:  fuse-devel
BuildRequires:  libcurl-devel
BuildRequires:  jemalloc-devel

%description
MARC-FS - Mail.ru Cloud filesystem written for FUSE.


%prep
%setup -q


%build
%cmake
make %{?_smp_mflags}


%install
%make_install


%files
%doc



%changelog

