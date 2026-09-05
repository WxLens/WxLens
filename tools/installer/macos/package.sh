#!/usr/bin/env bash
set -euo pipefail

# Run from the repository root after a Release build on the target Mac architecture.
build_dir=$(cd "${1:?build directory required}" && pwd)
version=${2:?version required}
version=${version#v}
arch=${3:?architecture required}
[[ "$version" =~ ^[0-9A-Za-z][0-9A-Za-z.+-]*$ ]] || { echo "Invalid version" >&2; exit 1; }
[[ "$arch" == arm64 || "$arch" == x64 ]] || { echo "Invalid architecture" >&2; exit 1; }

stage=$(mktemp -d)

# Qt's deploy tools bundle every plugin in plugins/sqldrivers the moment anything links Qt6Sql -
# and MapLibre's core does, for its offline tile database. Most of those drivers are for databases
# WxLens never touches, and they drag in libraries that simply are not on the machine: macdeployqt
# reported libmimerapi.dylib (the proprietary Mimer client), libiodbc and Postgres' libpq all
# missing, then copied the broken plugins into the bundle anyway, where the portability check
# below correctly rejected them. SQLite is the only driver MapLibre uses. Neither macdeployqt nor
# linuxdeploy-plugin-qt can exclude an individual plugin, so move the rest out of the Qt
# installation for the duration and put them back on the way out - including on failure, which is
# why this is wired into the EXIT trap rather than run at the end.
sqldrivers="$(qmake -query QT_INSTALL_PLUGINS)/sqldrivers"
sqldrivers_stash="$stage/sqldrivers"

cleanup()
{
   if [[ -d "$sqldrivers_stash" ]]; then
      mv "$sqldrivers_stash"/* "$sqldrivers/" 2>/dev/null || true
   fi
   rm -rf "$stage"
}
trap cleanup EXIT

if [[ -d "$sqldrivers" ]]; then
   mkdir -p "$sqldrivers_stash"
   for driver in "$sqldrivers"/*; do
      case "${driver##*/}" in
         *sqlite*) ;;
         *) [[ -e "$driver" ]] && mv "$driver" "$sqldrivers_stash/" ;;
      esac
   done
fi

app="$stage/WxLens.app"
ditto "$build_dir/Release/bin/WxLens.app" "$app"
cp LICENSE.txt ACKNOWLEDGEMENTS.md "$app/Contents/Resources/"
/usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString $version" "$app/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleVersion $version" "$app/Contents/Info.plist"

# Include the custom QML plugin explicitly: it is loaded at runtime, not linked to the app.
plugin=$(find "$app/Contents/Resources/qml/MapLibre" -name '*.dylib' -type f -print -quit)
test -n "$plugin"
macdeployqt "$app" -qmldir="$PWD/app/qml" \
  -qmlimport="$app/Contents/Resources/qml" -executable="$plugin" \
  -libpath="$build_dir/Release/lib" -libpath="$build_dir/Release/bin" -always-overwrite

# Refuse to ship Mach-O files still linked to the builder's Qt, Conan or Homebrew tree.
while IFS= read -r -d '' binary; do
  if file "$binary" | grep -q 'Mach-O'; then
    while IFS= read -r dependency; do
      case "$dependency" in
        /System/Library/*|/usr/lib/*|@*) ;;
        *) echo "Non-portable dependency in $binary: $dependency" >&2; exit 1 ;;
      esac
    done < <(otool -L "$binary" | awk '/compatibility version/ {print $1}')
  fi
done < <(find "$app" -type f -print0)

# Ad-hoc signatures support Apple Silicon execution; these are not Developer ID signatures.
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
ln -s /Applications "$stage/Applications"
mkdir -p dist
hdiutil create -volname WxLens -srcfolder "$stage" -ov -format UDZO \
  "dist/WxLens-$version-macos-$arch.dmg"
