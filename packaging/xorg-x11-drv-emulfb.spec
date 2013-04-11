# >> macros
# << macros

Name:       xorg-x11-drv-emulfb
Summary:    X.Org X server driver for sdk emulation
Version:    0.5.7
Release:    1
#ExclusiveArch:  %arm
Group:      System/X Hardware Support
License:    Samsung
Source0:    %{name}-%{version}.tar.gz

BuildRequires:  prelink
BuildRequires:  xorg-x11-xutils-dev
BuildRequires:  pkgconfig(xorg-server)
BuildRequires:  pkgconfig(randrproto)
BuildRequires:  pkgconfig(renderproto)
BuildRequires:  pkgconfig(fontsproto)
BuildRequires:  pkgconfig(xproto)
BuildRequires:  pkgconfig(videoproto)
BuildRequires:  pkgconfig(resourceproto)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(libdrm)
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(xdbg)

%description
This package provides the driver for sdk emulation

%prep
%setup -q

# >> setup
# << setup

%build
# >> build pre
# << build pre

%ifarch %{arm}
%define ENABLE_ARM --enable-arm
%else
%define ENABLE_ARM --disable-arm
%endif

%reconfigure --disable-static %{ENABLE_ARM} \
    CFLAGS="-Wall -Werror ${CFLAGS}" LDFLAGS="${LDFLAGS} -Wl,--hash-style=both -Wl,--as-needed"

make %{?jobs:-j%jobs}

# >> build post
# << build post
%install
rm -rf %{buildroot}
# >> install pre
# << install pre
mkdir -p %{buildroot}/usr/share/license
cp -af COPYING %{buildroot}/usr/share/license/%{name}
%make_install

# >> install post
execstack -c %{buildroot}%{_libdir}/xorg/modules/drivers/emulfb_drv.so
# << install post

%files
%defattr(-,root,root,-)
# >> files emulfb
%{_libdir}/xorg/modules/drivers/*.so
%{_datadir}/man/man4/*
# << files emulfb
/usr/share/license/%{name}
