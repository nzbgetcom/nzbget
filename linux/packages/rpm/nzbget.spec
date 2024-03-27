Name: nzbget
Version: $RPM_VERSION
Release: 1
Group: Applications/Internet
Summary: NZBGet is a command-line based binary newsgrabber for nzb files, written in C++.
License: GPLv2

%description
NZBGet is a command-line based binary newsgrabber for nzb files, written in C++. It supports client/server mode, automatic par-check/-repair, unpack and web-interface. NZBGet requires low system resources.

%define __spec_install_pre /bin/true

%prep

%build

%install

%files
/usr/local/bin/nzbget
/usr/local/share/doc/nzbget/
/usr/local/share/nzbget/
