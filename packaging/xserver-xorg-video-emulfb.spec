# >> macros
# << macros

Name:       xserver-xorg-video-emulfb
Summary:    X.Org X server driver for sdk emulation
Version:    0.2.0
Release:    16
#ExclusiveArch:  %arm
Group:      System/X11
License:    MIT
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  prelink
BuildRequires:  xorg-x11-util-macros
BuildRequires:  pkgconfig(xorg-server)
BuildRequires:  pkgconfig(randrproto)
BuildRequires:  pkgconfig(renderproto)
BuildRequires:  pkgconfig(fontsproto)
BuildRequires:  pkgconfig(xproto)
BuildRequires:  pkgconfig(videoproto)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(pixman-1)

%description
This package provides the driver for sdk emulation

%prep
%setup -q

# >> setup
# << setup

%build
# >> build pre
# << build pre

%reconfigure --disable-static \
LDFLAGS="-Wl,--hash-style=both -Wl,--as-needed"
make %{?jobs:-j%jobs}

# >> build post
# << build post
%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%make_install

# >> install post
execstack -c %{buildroot}%{_libdir}/xorg/modules/drivers/emulfb_drv.so
# << install post

%files
%defattr(-,root,root,-)
# >> files s5pc210
%{_libdir}/xorg/modules/drivers/*.so
%{_datadir}/man/man4/*
# << files emulfb
