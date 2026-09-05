#!/usr/bin/env bash
set -euo pipefail

# Run from the repository root after a Release build on the target Linux architecture.
# Mirrors tools/installer/macos/package.sh: stage what the build produced, hand it to the
# platform's deployment tool, verify the result is self-contained, then emit one file to dist/.
build_dir=$(cd "${1:?build directory required}" && pwd)
version=${2:?version required}
version=${version#v}
arch=${3:?architecture required}
[[ "$version" =~ ^[0-9A-Za-z][0-9A-Za-z.+-]*$ ]] || { echo "Invalid version" >&2; exit 1; }
# AppImage's own naming convention (and every desktop integration tool that parses it - appimaged,
# AM, zsync) expects the kernel arch spelling here, so this deliberately reads x86_64 rather than
# the x64 used for the Windows/macOS asset names.
[[ "$arch" == x86_64 || "$arch" == aarch64 ]] || { echo "Invalid architecture" >&2; exit 1; }

repo_root=$PWD
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
appdir="$stage/AppDir"

# The MapLibre QML plugin is loaded by the QML engine at runtime, never linked, so nothing pulls
# it in automatically - the same reason macos/package.sh passes it to macdeployqt explicitly.
plugin_src="$build_dir/Release/bin/qml/MapLibre"
test -f "$plugin_src/libdeclarative_maplibre.so"

# Two copies on purpose. linuxdeploy-plugin-qt writes usr/bin/qt.conf pointing Qml2Imports at
# $APPDIR/usr/qml, which is where Qt will look; but the app also works today (on Windows) purely
# off Qt's <exe-dir>/qml default import path, and which of the two wins is a Qt-internal detail
# not worth betting a release on. Both roots cost about a megabyte and the QML engine takes the
# first match, so a duplicate is inert.
for dest in "$appdir/usr/qml/MapLibre" "$appdir/usr/bin/qml/MapLibre"; do
   mkdir -p "$dest"
   cp -a "$plugin_src/." "$dest/"
done

install -Dm644 LICENSE.txt "$appdir/usr/share/doc/wxlens/LICENSE.txt"
install -Dm644 ACKNOWLEDGEMENTS.md "$appdir/usr/share/doc/wxlens/ACKNOWLEDGEMENTS.md"

# linuxdeploy installs both of these into the AppDir itself and cross-checks that the desktop
# file's Icon= key matches the icon's basename, so they are staged under matching names.
cp app/res/linux/org.wxlens.WxLens.desktop "$stage/org.wxlens.WxLens.desktop"
cp app/res/branding/wxlens-icon.png "$stage/org.wxlens.WxLens.png"

tools_dir=${LINUXDEPLOY_CACHE_DIR:-$stage/tools}
mkdir -p "$tools_dir"
linuxdeploy="$tools_dir/linuxdeploy-$arch.AppImage"
qt_plugin="$tools_dir/linuxdeploy-plugin-qt-$arch.AppImage"

fetch()
{
   local url=$1 dest=$2
   [[ -x "$dest" ]] && return 0
   curl --fail --location --silent --show-error --retry 3 --retry-delay 5 --output "$dest" "$url"
   chmod +x "$dest"
}

fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$arch.AppImage" \
      "$linuxdeploy"
# --plugin qt resolves the plugin by scanning PATH for linuxdeploy-plugin-qt*, so the filename
# above is load-bearing and the directory has to be on PATH before linuxdeploy runs.
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$arch.AppImage" \
      "$qt_plugin"
PATH="$tools_dir:$PATH"
export PATH

# Both tools ship as AppImages themselves and would need FUSE 2 to mount - absent on GitHub's
# runners and on plenty of modern distros. This makes them self-extract instead.
export APPIMAGE_EXTRACT_AND_RUN=1
# linuxdeploy-plugin-qt runs qmlimportscanner over these sources to decide which Qt QML modules
# to bundle. Without it the AppImage ships zero QML and dies at startup.
export QML_SOURCES_PATHS="$repo_root/app/qml"
export LINUXDEPLOY_OUTPUT_APP_NAME=WxLens
export LINUXDEPLOY_OUTPUT_VERSION="$version"
export LDAI_OUTPUT="WxLens-$version-linux-$arch.AppImage"

# Deploy first, verify, then package - splitting the two linuxdeploy passes is what makes room
# for the check below to run against a finished AppDir but before anything is publishable.
"$linuxdeploy" --appdir "$appdir" \
   --executable "$build_dir/Release/bin/wxlens-app" \
   --desktop-file "$stage/org.wxlens.WxLens.desktop" \
   --icon-file "$stage/org.wxlens.WxLens.png" \
   --deploy-deps-only="$appdir/usr/qml/MapLibre" \
   --deploy-deps-only="$appdir/usr/bin/qml/MapLibre" \
   --plugin qt

# Refuse to ship an AppDir that silently deployed nothing. Each of these is a distinct failure
# mode that otherwise only shows up as a crash on the user's machine: no Qt libraries at all, no
# MapLibre core beside the QML plugin that dlopens it, no X11 platform plugin (Qt aborts with
# "could not load the Qt platform plugin"), or no Qt QML modules because qmlimportscanner found
# nothing to scan.
for library in libQt6Quick libQt6Qml libQt6Gui libQMapLibre; do
   find "$appdir/usr/lib" -name "$library*.so*" -print -quit | grep -q . ||
      { echo "Missing $library in the AppDir" >&2; exit 1; }
done
test -f "$appdir/usr/plugins/platforms/libqxcb.so"
test -d "$appdir/usr/qml/QtQuick"

"$linuxdeploy" --appdir "$appdir" --output appimage

mkdir -p "$repo_root/dist"
mv "$LDAI_OUTPUT" "$repo_root/dist/$LDAI_OUTPUT"
echo "Wrote dist/$LDAI_OUTPUT"
