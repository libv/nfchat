%define name xchat
%define version 1.4.2
%define release 1
%define prefix /usr

Summary: Gtk+ IRC client

Name: %{name}
Version: %{version}
Release: %{release}
Group: Applications/Internet
Copyright: GPL

Url: http://xchat.org

Source: http://xchat.org/files/source/1.4/xchat-%{version}.tar.gz
Buildroot: /var/tmp/%{name}-%{version}-%{release}-root

%description
X-Chat is yet another IRC client for the X Window
System, using the Gtk+ toolkit. It is pretty easy
to use compared to the other Gtk+ IRC clients and
the interface is quite nicely designed.

%prep

%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix} --disable-panel --disable-textfe
make

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT%{prefix}
make prefix=$RPM_BUILD_ROOT%{prefix} install-strip

%files
%defattr(-,root,root)
%doc README ChangeLog doc/xchat.sgml
%attr(755,root,root) %{prefix}/bin/xchat
%{prefix}/share/gnome/apps/Internet/xchat.desktop
%{prefix}/share/pixmaps/xchat.png
%{prefix}/share/locale/*/*/*

%clean
rm -r $RPM_BUILD_ROOT
