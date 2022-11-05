%define date 20221105
%define rev  g1667726

Name:       fuse-marcfs
Version:    0.8.1
Release:    1.%{date}git%{rev}%{?dist}
Summary:    FUSE filesystem for Mail.ru Cloud storage

License:    GPLv3+
URL:        https://gitlab.com/Kanedias/MARC-FS
Source0:    fuse-marcfs-%{version}.tar.zst

BuildRequires:  git
BuildRequires:  meson
BuildRequires:  gcc-c++
BuildRequires:  fuse3-devel
BuildRequires:  libcurl-devel
BuildRequires:  jemalloc-devel

Requires: fuse3
Requires: jsoncpp
Requires: libcurl
Requires: jemalloc

%description
MARC-FS - Mail.ru Cloud filesystem written for FUSE.


%prep
%setup -q


%build
%meson
%meson_build


%install
%meson_install --skip-subprojects


%files
%doc AUTHORS.md CONTRIBUTING.md LICENSE README.md
%{_bindir}/marcfs


%changelog
* Sat Nov 05 2022 Oleg Chernovskiy <kanedias@gmx.net> - 0.8.1gitg1667726
- Bump to curlcpp 2.1

* Sun Nov 01 2020 Oleg Chernovskiy <kanedias@xaker.ru> - 0.6.0gitc5a7985
- Fix cloud API

* Wed Sep 19 2018 Oleg Chernovskiy <kanedias@xaker.ru> - 0.6.0git801e616
- Update to fuse3

* Mon May 29 2017 Yaroslav Sidlovsky <zawertun@gmail.com> - 0.4-1.20170529git0461ea2
- first spec version

