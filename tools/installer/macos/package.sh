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
trap 'rm -rf "$stage"' EXIT
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
    done < <(otool -L "$binary" | tail -n +2 | sed -E 's/^[[:space:]]+//; s/ \(compatibility version.*$//')
  fi
done < <(find "$app" -type f -print0)

# Ad-hoc signatures support Apple Silicon execution; these are not Developer ID signatures.
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
ln -s /Applications "$stage/Applications"
mkdir -p dist
hdiutil create -volname WxLens -srcfolder "$stage" -ov -format UDZO \
  "dist/WxLens-$version-macos-$arch.dmg"
