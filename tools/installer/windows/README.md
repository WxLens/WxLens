# Windows installer

Builds the Windows installer decided in `docs/ROADMAP.md`'s Release prep section: Inno Setup,
producing one `.exe` installer (a portable ZIP of the same `windeployqt` output can be made
alongside it with any zip tool - there is no dedicated script for that yet).

## Prerequisites

1. A normal Release build, so `build-release-vs2026/Release/bin/` has already been populated by
   `windeployqt` (see the root [AGENTS.md](../../../AGENTS.md) build instructions).
2. [Inno Setup 6](https://jrsoftware.org/isinfo.php) (`winget install JRSoftware.InnoSetup`).

## Build

```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" tools\installer\windows\wxlens.iss
```

Output: `dist\WxLens-<version>-windows-x64-setup.exe` (gitignored - rebuilt per release, not
committed).

Override the version or a non-default build directory without editing the script:

```powershell
ISCC.exe /DMyAppVersion=0.2.0 /DBuildDir=..\build-somewhere\Release tools\installer\windows\wxlens.iss
```

## What it packages

Everything under `Release\bin\` (the full `windeployqt` output: `wxlens-app.exe`, Qt/MapLibre/AWS
runtime DLLs, and the `platforms/`, `imageformats/`, `qml/`, etc. plugin directories), excluding:

- `*.pdb` - debug symbols; hundreds of MB combined (e.g. `wxlens-app.pdb` alone is over 100 MB
  in a typical build) and never loaded at runtime.
- `*-test.exe`, `test_mln_quick.exe` - the GTest binaries that build into the same output
  directory as the app; not part of the shipped product.

Uninstalling removes only the installed program files; a user's settings, saved places and
edited palettes under `%LOCALAPPDATA%\WxLens` are left in place.

## Known open items

- Unsigned (matches the roadmap's "ship unsigned for now" decision) - Windows SmartScreen will
  show an unknown-publisher prompt on first run.
- `ci.yml` builds and tests but does not run this script; only `.github/workflows/release.yml`
  does, on a `v*.*.*` tag. Unlike the Linux and macOS packages, the installer therefore gets no
  per-commit packaging coverage - a change that breaks it surfaces at release time.
- No portable-ZIP-building script yet, only the installer.
