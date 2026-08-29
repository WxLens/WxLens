# ADR 0003: TOML for structured config/theme/palette storage

## Status
Accepted (2026-08-21)

## Context
`docs/ROADMAP.md` §3.2 requires moving off `QSettings` (OS-registry-coupled on Windows) to a
portable, plain-text, shareable format for settings, pane-layout persistence, and UI themes
(`.pal` files are unaffected — that format is untouched, see §5.1). The roadmap explicitly says
"pick one" between TOML and JSON and left it open for Phase 0.

## Decision
TOML, via a small typed C++ wrapper in `app/source/wxlens/settings/` that preserves the current
`settings_variable`/`settings_interface` *pattern* (validated defaults, Qt-signal change
notification) but retargets storage to TOML files under `QStandardPaths::AppConfigLocation`.

## Rationale
- TOML supports comments; a hand-editable settings/theme/pane-layout file a user or another agent
  opens in a text editor benefits from being able to see why a value is set, which JSON cannot
  express natively.
- TOML's table/array-of-tables syntax maps cleanly onto the per-category settings split
  (`radar.toml`, `theme.toml`, `panes.toml`, ...) mirroring today's `settings/*_settings.hpp` split
  without needing a schema-heavy nested-object convention the way deeply-nested JSON would.
- A permissively-licensed, header-only C++ TOML library (e.g. `toml++`, MIT) is a trivial Conan
  addition with no footprint concerns, consistent with the "modest laptop" constraint.
- Shareable theme/config files (the Phase 5 stretch-goal rationale in §3.2) work equally well in
  either format; TOML's readability tips the balance since these files are meant to be
  hand-editable and shareable like `.pal` already is.

## Consequences
- `app/source/wxlens/settings/` depends on a TOML library (to be added to `conanfile.py` when the
  first real settings file is implemented — not needed for the Phase 0 empty-shell milestone).
- Pane-layout persistence (§4.6) and theme files (§5.2) both serialize through this same wrapper.
- If a future need arises for config data to interoperate with a JSON-only external tool, add a
  narrow TOML→JSON export path rather than switching the primary format.
