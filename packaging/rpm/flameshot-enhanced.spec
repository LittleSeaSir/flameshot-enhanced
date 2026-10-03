# First supported binary target: Fedora 44, x86_64, KDE Plasma 6 Wayland.
# Other RPM distributions and desktop sessions are not validated by this spec.
%global release_version %{version}-littlesea.1
%global release_tag v%{release_version}
%{!?commit:%global commit -}
# Publish one application RPM and its source RPM for this first release.
%global debug_package %{nil}

Name:           flameshot-enhanced
Version:        14.0.0
Release:        1.littlesea%{?dist}
Summary:        Independently maintained Flameshot fork with enhanced annotations

# Includes statically linked QtColorWidgets and KDSingleApplication, Material
# icons, and CC0 AppStream metadata. Original license texts ship in the RPM.
License:        GPL-3.0-or-later AND LGPL-3.0-or-later AND MIT AND Apache-2.0 AND CC0-1.0
URL:            https://github.com/LittleSeaSir/flameshot-enhanced
Source0:        %{url}/archive/refs/tags/%{release_tag}.tar.gz#/%{name}-%{release_version}.tar.gz
Vendor:         LittleSeaSir
ExclusiveArch:  x86_64

BuildRequires:  gcc-c++
BuildRequires:  cmake >= 3.22
BuildRequires:  ninja-build
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
BuildRequires:  cmake(Qt6Core) >= 6.2.4
BuildRequires:  cmake(Qt6DBus) >= 6.2.4
BuildRequires:  cmake(Qt6Gui) >= 6.2.4
BuildRequires:  cmake(Qt6LinguistTools) >= 6.2.4
BuildRequires:  cmake(Qt6Network) >= 6.2.4
BuildRequires:  cmake(Qt6Svg) >= 6.2.4
BuildRequires:  cmake(Qt6Widgets) >= 6.2.4
BuildRequires:  cmake(KF6GuiAddons) >= 6.7.0

Requires:       hicolor-icon-theme
Requires:       qt6-qtwayland%{?_isa}
Requires:       qt6-qtsvg%{?_isa}
Requires:       xdg-desktop-portal%{?_isa}
Requires:       xdg-desktop-portal-kde%{?_isa}
# These libraries are loaded with QLibrary, so automatic ELF dependency
# generation cannot discover them. They provide native pin positioning.
Requires:       libPlasmaQuick.so.7()(64bit)
Requires:       libLayerShellQtInterface.so.6()(64bit)
Recommends:     qt6-qtimageformats%{?_isa}

# This release keeps /usr/bin/flameshot and the org.flameshot D-Bus/desktop IDs.
# Require users to explicitly remove the official package before installing.
# Do not use Obsoletes or claim to provide the upstream package.
Conflicts:      flameshot

Provides:       bundled(QtColorWidgets) = 2.2.0
Provides:       bundled(KDSingleApplication) = 1.2.1

%description
Flameshot Enhanced is an independent fork of Flameshot, maintained by
LittleSeaSir. It adds smoother drawing, whole-stroke and pixel erasing,
in-place editing of pinned images, movable annotations, and KWin window
snapping. It is not an official Flameshot release.

This RPM is built and tested for Fedora 44 x86_64 with KDE Plasma 6 on
Wayland. Native pin positioning and window snapping depend on KDE/KWin.
The executable, configuration, and desktop/D-Bus identities remain named
flameshot, so this package cannot coexist with the official flameshot RPM.

%prep
%autosetup -n %{name}-%{release_version}
# The release archive must include these pinned dependencies. Never download
# moving sources from the network while building an RPM.
test -f external/Qt-Color-Widgets/CMakeLists.txt
test -f external/KDSingleApplication/CMakeLists.txt

%build
# scripts/build-rpm.sh supplies the commit of the archived source tree.
export GIT_HASH="%{commit}"
%cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_LIBDIR=%{_lib} \
    -DBUILD_SHARED_LIBS=OFF \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DUSE_KDSINGLEAPPLICATION=ON \
    -DUSE_BUNDLED_KDSINGLEAPPLICATION=ON \
    -DUSE_WAYLAND_CLIPBOARD=ON \
    -DUSE_LAUNCHER_ABSOLUTE_PATH=ON \
    -DUSE_PORTABLE_CONFIG=OFF \
    -DDISABLE_UPDATE_CHECKER=ON \
    -DENABLE_IMGUR=OFF \
    -DFLAMESHOT_VERSION_SUFFIX=-littlesea.1 \
    -DFLAMESHOT_BUILD_TESTS=ON
%cmake_build

%install
%cmake_install

# The bundled dependencies are linked statically into flameshot. Their SDK
# files are not application payload and would conflict with system packages.
rm -rf %{buildroot}%{_includedir}/QtColorWidgets
rm -rf %{buildroot}%{_libdir}/cmake/QtColorWidgets
rm -f %{buildroot}%{_libdir}/libQtColorWidgets.*
rm -f %{buildroot}%{_libdir}/pkgconfig/QtColorWidgets.pc
rm -rf %{buildroot}%{_includedir}/kdsingleapplication-qt6
rm -rf %{buildroot}%{_libdir}/cmake/KDSingleApplication-qt6
rm -f %{buildroot}%{_libdir}/libkdsingleapplication-qt6.*

# Keep dependency licenses in separate directories so identical basenames
# cannot overwrite one another in the installed license directory.
install -Dpm0644 LICENSE %{buildroot}%{_licensedir}/%{name}/LICENSE
install -Dpm0644 external/Qt-Color-Widgets/COPYING %{buildroot}%{_licensedir}/%{name}/QtColorWidgets/COPYING
install -Dpm0644 external/Qt-Color-Widgets/LICENSE-EXCEPTION %{buildroot}%{_licensedir}/%{name}/QtColorWidgets/LICENSE-EXCEPTION
install -Dpm0644 external/Qt-Color-Widgets/LICENSES/CC0-1.0.txt %{buildroot}%{_licensedir}/%{name}/CC0-1.0.txt
install -Dpm0644 external/KDSingleApplication/LICENSE.txt %{buildroot}%{_licensedir}/%{name}/KDSingleApplication/LICENSE.txt
install -Dpm0644 external/KDSingleApplication/LICENSES/MIT.txt %{buildroot}%{_licensedir}/%{name}/KDSingleApplication/MIT.txt
install -Dpm0644 data/img/material/LICENSE.txt %{buildroot}%{_licensedir}/%{name}/MaterialIcons/LICENSE.txt

%check
export QT_QPA_PLATFORM=offscreen
# Keep tests away from the builder's real application configuration.
export XDG_CONFIG_HOME="${PWD}/rpm-test-config"
mkdir -p "$XDG_CONFIG_HOME"
%ctest --timeout 60
desktop-file-validate %{buildroot}%{_datadir}/applications/org.flameshot.Flameshot.desktop
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/org.flameshot.Flameshot.metainfo.xml

%files
%doc README.md
%license %{_licensedir}/%{name}
%{_bindir}/flameshot
%{_datadir}/flameshot/
%{_datadir}/applications/org.flameshot.Flameshot.desktop
%{_datadir}/metainfo/org.flameshot.Flameshot.metainfo.xml
%{_datadir}/bash-completion/completions/flameshot
%{_datadir}/zsh/site-functions/_flameshot
%{_datadir}/fish/vendor_completions.d/flameshot.fish
%{_datadir}/dbus-1/interfaces/org.flameshot.Flameshot.xml
%{_datadir}/dbus-1/services/org.flameshot.Flameshot.service
%{_datadir}/icons/hicolor/48x48/apps/flameshot.png
%{_datadir}/icons/hicolor/48x48/apps/org.flameshot.Flameshot.png
%{_datadir}/icons/hicolor/128x128/apps/flameshot.png
%{_datadir}/icons/hicolor/128x128/apps/org.flameshot.Flameshot.png
%{_datadir}/icons/hicolor/scalable/apps/flameshot.svg
%{_datadir}/icons/hicolor/scalable/apps/org.flameshot.Flameshot.svg
%{_mandir}/man1/flameshot.1*

%changelog
* Sat Oct 03 2026 LittleSeaSir - 14.0.0-1.littlesea
- First independent fork release for Fedora 44 KDE Plasma 6 Wayland.
- Bundle pinned source dependencies and retain their license texts.
- Require native pin-positioning libraries and reject official-package conflicts.
- Build and run screenshot, annotation, pin-layout, and window-snapping tests.
