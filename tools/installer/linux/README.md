# Linux downloads

GitHub Releases build a single `WxLens-<version>-linux-x86_64.AppImage` asset. Download it,
`chmod +x` it, and run it — no installation, no root, and no distribution packages to add.
The AppImage bundles Qt, the MapLibre renderer and every other shared library the app needs.

The arch in the filename is spelled `x86_64` rather than the `x64` used for the Windows and
macOS assets because AppImage's desktop-integration tooling (appimaged, AM, zsync) parses that
suffix and expects the kernel spelling.

## Compatibility

Built on `ubuntu-24.04`, so the AppImage requires **glibc 2.39 or newer** — Ubuntu 24.04,
Debian 13, Fedora 40 and anything of similar vintage or later. AppImages cannot bundle glibc,
so an older distribution needs a build from source instead. Bumping the runner image forward
raises this floor; bumping it back lowers it.

Qt's Wayland platform plugins are not bundled (the `qtwayland` module is not part of the CI Qt
install), so the app runs under XWayland on a Wayland session. Some AppImage-era distributions
also ship without FUSE 2; on those, either install `libfuse2` or run the AppImage with
`APPIMAGE_EXTRACT_AND_RUN=1`.

## How it is built

`.github/workflows/linux.yml` is a reusable workflow that builds, runs the wxdata test suite,
packages the AppImage, and then **smoke tests it** — the packaged binary is launched under
`xvfb` with software OpenGL and has to survive a timeout with no unresolved Qt platform plugin
or QML module in its output. A build that links is not a build that runs, and that smoke test is
the only thing in this repo that exercises the Linux runtime path.

`ci.yml` calls this workflow on every push and pull request (it replaced the old inline
`linux_gcc-14_x64` matrix entry, so Linux is still built and tested exactly once), and
`release.yml` calls it again for tagged releases. CI uploads the AppImage as a workflow
artifact; the release workflow publishes it alongside the Windows installer and the Mac disk
images, and only after every package job has succeeded.

## Reproducing locally

Needs GCC 14, Qt 6.11.1 with ShaderTools, Conan 2, CMake, Ninja and `curl`. Follow the
configure/build commands in `linux.yml`, then from the repository root:

```bash
bash tools/installer/linux/package.sh build v0.1.1 x86_64
```

Output goes to `dist/`. The script downloads `linuxdeploy` and `linuxdeploy-plugin-qt` on each
run; set `LINUXDEPLOY_CACHE_DIR` to a persistent directory to keep them between runs.

## What it packages

`linuxdeploy` collects the executable's shared-library dependencies into the AppDir and
`linuxdeploy-plugin-qt` adds the Qt platform plugins and the QML modules `qmlimportscanner`
finds in `app/qml`. Two things need explicit handling on top of that:

- **The MapLibre QML plugin** is loaded by the QML engine at runtime and never linked, so
  nothing pulls it in automatically. The script stages it and passes `--deploy-deps-only` so its
  own dependencies (`libQMapLibre`, `libQMapLibreQuickPrivate`) get deployed and its RPATH
  patched. It is staged under **both** `usr/qml/MapLibre` and `usr/bin/qml/MapLibre` — the
  `qt.conf` the Qt plugin writes points Qt at the first, while the app works today on Windows
  purely off Qt's `<exe-dir>/qml` default import path. Which one wins is a Qt-internal detail;
  a duplicate costs about a megabyte and the QML engine takes the first match.
- **`LICENSE.txt` and `ACKNOWLEDGEMENTS.md`** are installed under `usr/share/doc/wxlens/`.
- **The icon** is `app/res/branding/wxlens-icon-256.png`, a committed 256x256 derivative of the
  880x880 branding master. linuxdeploy validates icon dimensions against the freedesktop
  icon-theme sizes and rejects anything else, so the master cannot be used directly. Regenerate
  it after a branding change with:

  ```python
  from PIL import Image
  Image.open("app/res/branding/wxlens-icon.png").convert("RGBA")        .resize((256, 256), Image.LANCZOS)        .save("app/res/branding/wxlens-icon-256.png", "PNG", optimize=True)
  ```

Before packaging, the script fails the build if the AppDir is missing Qt Quick/Qml/Gui, the
MapLibre core library, the `xcb` platform plugin, or the bundled `QtQuick` QML module — each of
which would otherwise only surface as a crash on a user's machine.

## Known open items

- Unsigned, and no AppStream metainfo or zsync update information yet, so `appimageupdate`
  cannot do delta updates.
- x86_64 only. The script already accepts `aarch64`, but no ARM64 Linux runner is wired up.
- No `.deb`, `.rpm` or Flatpak.
