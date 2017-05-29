%define date 20170529
%define rev  0461ea2

Name:       fuse-marcfs
Version:    0.4
Release:    1.%{date}git%{rev}%{?dist}
Summary:    FUSE filesystem for Mail.ru Cloud storage

License:    GPLv3+
URL:        https://gitlab.com/Kanedias/MARC-FS
Source0:    fuse-marcfs-%{version}.tar.bz2

BuildRequires:  cmake
BuildRequires:  gcc-c++
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
%doc AUTHORS.md LICENSE README.md
%{_bindir}/marcfs


%changelog
* Mon May 29 2017 Yaroslav Sidlovsky <zawertun@gmail.com> - 0.4-1.20170529git0461ea2
- first spec version

