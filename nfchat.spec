%define name nfchat
%define version 1.3.5
%define release 1
%define prefix /usr

Summary: Gtk+ IRC client for special client boxes

Name: %{name}
Version: %{version}
Release: %{release}
Group: Applications/Internet
Copyright: GPL

Url: http://www.netforce.be

Source: http://www.netforce.be/files/source/1.3/nfchat-%{version}.tar.gz
Buildroot: /var/tmp/%{name}-%{version}-%{release}-root

%description
NF-Chat is an IRC client based upon X-Chat. It 
shares the look and feel of the original, but it
is a cut down version with only 1 channel. It has
been custom made to be usable on kb only boxes in
public places.

%prep

%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix}
make

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT%{prefix}
make prefix=$RPM_BUILD_ROOT%{prefix} install-strip

%files
%defattr(-,root,root)
%attr(755,root,root) %{prefix}/bin/nfchat

%clean
rm -r $RPM_BUILD_ROOT
