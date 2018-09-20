%define date 20180920
%define rev  801e616

Name:       fuse-marcfs
Version:    0.6.0
Release:    1.%{date}git%{rev}%{?dist}
Summary:    FUSE filesystem for Mail.ru Cloud storage

License:    GPLv3+
URL:        https://gitlab.com/Kanedias/MARC-FS
Source0:    fuse-marcfs-%{version}.tar.bz2

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  fuse3-devel
BuildRequires:  libcurl-devel
BuildRequires:  jemalloc-devel

%description
MARC-FS - Mail.ru Cloud filesystem written for FUSE.


%prep
%setup -q


%build
%cmake -DBUILD_SHARED_LIBS:BOOL=OFF
make %{?_smp_mflags}


%install
%make_install


%files
%doc AUTHORS.md LICENSE README.md
%{_bindir}/marcfs


%changelog
* Wed Sep 19 2018 Oleg Chernovskiy <kanedias@xaker.ru> - 0.6.0git801e616
- Update to fuse3

* Mon May 29 2017 Yaroslav Sidlovsky <zawertun@gmail.com> - 0.4-1.20170529git0461ea2
- first spec version

