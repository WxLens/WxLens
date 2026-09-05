# macOS downloads

GitHub Releases build separate `WxLens-<version>-macos-arm64.dmg` (Apple Silicon)
and `WxLens-<version>-macos-x64.dmg` (Intel) assets for macOS 15 or newer.
Open the disk image and drag WxLens to Applications. These builds are ad-hoc signed,
not Developer ID signed or notarized; macOS may require **System Settings > Privacy
& Security > Open Anyway** after the first launch attempt for a trusted download.

The reusable `.github/workflows/macos.yml` builds and tests both architectures on
native runners, packages Qt/QML/MapLibre and dependent libraries, verifies signatures
and rejects absolute dependencies outside Apple's system libraries. CI uploads the
disk images as workflow artifacts; the release workflow publishes them alongside the
Windows installer only after all package jobs succeed.

To add Mac downloads to an existing release whose tag predates this packaging setup,
manually run `release.yml` with `tag` set to that release, `build_ref` set to the
packaging commit, and `macos_only` set to `true`. Use this override only when the
application source matches the release and the differences are packaging fixes.
The workflow uploads both DMGs automatically after successful builds, preserving
the existing Windows asset. It does not move the release tag. Conan packages are
cached after dependency installation so retries can reuse them.

To reproduce on a Mac, use LLVM 18, Qt 6.11.1 with ShaderTools, Conan 2, CMake and
Ninja. Follow the configure/build commands in `macos.yml`, using the Conan profile
matching the machine architecture, then run from the repository root:

```bash
bash tools/installer/macos/package.sh build v0.1.1 arm64
```

Use `x64` on Intel. Outputs go to `dist/`. The package includes `LICENSE.txt` and
`ACKNOWLEDGEMENTS.md`. The macOS deployment target is deliberately 15.0 in both
Conan and CMake so the package does not promise compatibility below its build host.

Validation still required: successful builds on both GitHub runners and opening the
download on clean Macs, including map/radar rendering. Windows-local syntax checks
cannot establish macOS runtime compatibility.
