# ADR 0002: `wxdata` reuse via whole-repo submodule (Option A)

## Status
Accepted (2026-08-21)

## Context
`wxdata` (NEXRAD Level 2/3 parsing, `.pal` color tables, AWIPS text products, network providers)
is the one part of Supercell Wx carried forward into WxLens unchanged — re-deriving ICD binary
parsing from scratch would be months of low-value, high-risk work. `docs/ROADMAP.md` §3.1 lists
two options: (A) submodule the *entire* current Supercell Wx repo and `add_subdirectory` its
`wxdata/`, or (B) extract `wxdata/` into its own standalone repo via `git subtree split` first.

`wxdata/CMakeLists.txt` + `wxdata/wxdata.cmake` show its dependencies split two ways: most are
Conan-managed packages (Boost, BZip2, cpr, LibXml2, libzip, OpenSSL, range-v3, re2, spdlog), but
`wxdata`'s AWS S3 provider code (`source/scwx/provider/aws_*.cpp`) links directly against
`aws-cpp-sdk-core`/`aws-cpp-sdk-s3`/`aws-crt-cpp`, and `common/color_table.cpp`+`gr/color.cpp`+
`gr/placefile.cpp` link `hsluv-c` — both built from source via vendored git submodules
(`external/aws-sdk-cpp`, `external/hsluv-c`), not Conan. `units::units` (`external/units`,
header-only) and, conditionally, `date::date-tz` (`external/date`, only when the toolchain's
`std::chrono` lacks full timezone/calendar support, checked via a `try_compile`) round out the
non-Conan half. It does **not** need `maplibre-native`, `imgui`, `qt6ct`, `glad`,
`imgui-backend-qt`, `stb`, or `textflowcpp` — those are `scwx-qt`-only. WxLens vendors the four
submodules `wxdata` actually needs (`external/aws-sdk-cpp`, `external/date`, `external/hsluv-c`,
`external/units`) directly at its own `external/`, built via `external/*.cmake` glue files mirroring
the legacy repo's pattern, rather than reaching into the legacy submodule's copies — so WxLens's
own `external/CMakeLists.txt` is self-sufficient without depending on the legacy submodule's
internal layout beyond `wxdata/` itself.

## Decision
Option A: `external/legacy-supercell-wx/` is a shallow (`--depth 1`) submodule of
`https://github.com/dpaulat/supercell-wx.git`, and WxLens's top-level `CMakeLists.txt`
`add_subdirectory()`s only its `wxdata/` path. The submodule's own nested submodules
(`aws-sdk-cpp`, `maplibre-native`, `imgui`, etc.) are **not** initialized — WxLens doesn't need
them, and initializing them would multiply the clone by several times for no benefit.

## Follow-up finding (same session): pin `external/units` to the legacy repo's exact commit
Building `wxdata` for real (see `docs/ROADMAP.md`'s Phase 0 status note) surfaced that `wxdata.cmake`
compiles with `/W4 /WX` on MSVC (line ~338), and a newer `nholthaus/units` commit than the legacy
repo pins (`aa4c448`/`v3.6.1`, this repo's initial clone, vs. the legacy repo's pinned `da6dd917`)
introduces a `C4459` warning (`declaration of 'd' hides global declaration` in `time.h`/`velocity.h`)
that becomes a hard error under `/WX`. Fixed by checking out `external/units` to the legacy repo's
exact pinned commit (`da6dd9176e8515323c75030d5e51ee19cf6c9afd`) instead of tracking upstream HEAD
— consistent with "reuse wxdata as-is," this extends to using dependency versions already proven
compatible with it, not whatever's newest. Don't advance this pin casually; if it's ever bumped,
rebuild `wxdata` and its tests first to catch this class of issue again.

## Consequences
- Every fresh clone of WxLens drags in the legacy repo's `scwx-qt`/`external`/history, exactly as
  §3.1 calls out as this option's tradeoff. Acceptable for now; not a blocker.
- Option B (extracting `wxdata` into its own standalone MIT repo via `git subtree split`) stays a
  fast-follow, not blocking — deferred per §3.1's `[OPEN QUESTION]` #2 in `docs/ROADMAP.md` §9,
  since it touches the *live*, currently-shipping Supercell Wx repo and is the user's call, not an
  agent's.
- `wxdata` is linked into WxLens unmodified; any bugfix belongs upstream in the legacy repo first,
  then the submodule pin advances — don't fork/patch `wxdata` in place inside WxLens.
