# ADR 0005: Experience levels — tiered presets over one interface

## Status
Accepted (2026-11-26). Owner decisions on 2026-11-26: tier names Basic / Advanced / Professional;
Advanced allows up to 4 panes; the forecast card is in scope; any number of custom presets can be saved and
shared as a file (§8).

## Context
WxLens is meant to serve two kinds of users (§5.3): someone who just wants to see whether weather is
coming, and an enthusiast or forecaster who wants every product, pane and analysis tool. Until now
the roadmap has handled that split with progressive disclosure: one interface, with advanced
controls hidden behind individual reveals such as **Settings > Appearance > Advanced pane linking**
(2026-09-05). §5.3 also says toolbar customization must not become "a second incompatible UI
persona."

The owner wants more than that (2026-11-26):

- At first-run setup, the user chooses how much control they want, from simple to advanced, with
  **at least three tiers**.
- The tier can be changed quickly from a dropdown in the top bar, so switching back and forth is
  cheap.
- The simple tier leaves out the extras (drawing, analysis tools, multi-pane setup) and shows
  **a radar view that covers storms across radar-site boundaries**, so the user never has to pick
  or switch radar sites.
- WxLens moves toward being a general weather app, not just a radar viewer.
- Beyond the three tiers, a user can save as many **custom presets** as they like (for example "Snow" or "Tornado season") and export any of them as a file.
  A coworker, a new user or an online community can import it and get the same setup, including
  color tables. Locations and other private information must never be in that file.

Turning on individual reveals one by one does not meet this. A new user would have to know which
switches exist, and there would be no single action that brings back the simple view.

## Decision

### 1. An experience level is a preset, not a separate UI
A single setting, `experience.level`, picks one of three levels. Each level is just **a default set
of reveal flags**. Every level uses the same panes, data services, map-object store, QML components
and settings surface. Choosing a level never loads different code paths or a different layout
system. This follows §5.3's rule (one information architecture) and principle 13 (never remove
capability, only layer it).

| | **Basic** | **Advanced** | **Professional** |
|---|---|---|---|
| For | "Is weather coming toward me?" | Hobbyists, spotters, storm chasers | Experienced users and forecasters |
| Radar | Composite radar across sites only (see §6). No site picker | Composite by default, plus picking a single site | All of it: every product, tilt and data level |
| Products | Reflectivity | Reflectivity, velocity, correlation coefficient, storm-relative velocity | Full product browser and palette editor |
| Panes | 1 | Up to 4, camera always linked | Up to 4, all per-channel sync controls |
| Warnings | Plain-language alerts for saved places | Warning polygons plus full text | Plus watches, mesoscale discussions and archived warnings |
| Tools | Tap for distance and bearing from home | Measurement and place markers | Drawing, analysis objects and radar-geometry readouts |
| Time | Play/pause and "Now" | Scrubber and loop length | Archive browsing and frame stepping |
| Other layers | Forecast and current-conditions card (all levels, see §7) | Satellite, once it exists | Model fields and soundings, once they exist |

Advanced and Professional both allow 4 panes. What Professional adds is control over how panes
are linked; at Advanced, all panes move together.

There is no fourth built-in tier. Anything beyond these three is a user-made **custom preset**
(§8), and a user can have as many as they want.

### 2. Features check reveal keys, not the level
Each piece of gated capability has a named **reveal key**, such as `panes.multi`,
`panes.sync_controls`, `tools.drawing`, `radar.site_picker` or `radar.product_browser`. QML and C++
check `isRevealed("tools.drawing")`, never `level >= Professional`. Only one table in C++ maps
levels to key sets. Code must not compare levels anywhere else. Moving a feature to a different
tier then means editing that one table, and per-feature overrides need no special handling.

The existing `advancedPaneLinking` setting becomes the `panes.sync_controls` reveal key. Its
persisted value is migrated as an override, so current users keep what they have. Its settings
label is renamed (for example "Show pane sync controls") so it is not confused with the Advanced
tier.

### 3. Overrides sit on top of the level
A user can turn individual reveal keys on or off. Overrides are stored as differences from the
chosen level's defaults. While any override exists, the dropdown shows
**"Basic (customized)"** and offers **Reset to Basic defaults** and **Save as custom preset…**
(§8). Toolbar customization (§5.3) stays as it is. The level only picks which curated toolbar is
the default.

### 4. Switching level only changes what is shown
Changing level never deletes or rewrites user state:

- Panes hidden by a lower level are kept, using the existing machinery for retaining hidden
  workspace panes. They come back when the level goes up again.
- Analysis objects (drawings, measurements, range rings) are hidden, not removed. Saved places
  (§4.9) stay visible at every level because they are personal context, not analysis.
- Products and sites chosen at a higher level are remembered for each pane. A lower level can
  show its own default product without overwriting them.
- Switching takes effect immediately, with no restart and no reload of loaded data.

The command search and keyboard shortcuts still find hidden capability. They label it, for
example "Drawing — available in Professional", and offer to switch level. They do not run it
silently at the lower level.

### 5. Where it appears
- **First run:** one question, "How do you use weather?", with three cards and an
  **Import a preset** link for someone who was given a `.wxsetup` file (§8). Each card describes
  what the user will see, not what is hidden. The default selection is **Basic**. It sits in the
  same first-run flow as the update-preference prompt (§7 Release prep).
- **Top bar:** one compact level dropdown. §5.3 normally keeps rarely changed modes out of
  persistent chrome. This is a deliberate, owner-approved exception, because switching quickly is
  the point of the feature. It must stay one control and collapse to an icon at narrow widths.
- **Top-bar dropdown contents:** the built-in tiers (Basic, Advanced, Professional), a divider,
  the user's custom presets by name (for example "Snow" and "Tornado season"), another divider,
  then **Import preset…** and **Manage presets…** (rename, duplicate, delete, export).
- **Settings:** an addressable `experience` section holding the level, the per-key overrides, the
  reset action and the custom presets (rename, duplicate, delete, export).

### 6. Radar in Basic: composite first, auto-site as a stopgap
Basic's promise is that the user never picks a radar site. The permanent answer is the
**composite mosaic layer** (Phase 2, MRMS). That makes the start of Phase 2 a dependency of this
feature: the IEM pre-rendered MRMS tile bootstrap (§6) moves forward to ship with or soon after
this work, ahead of the full GRIB2 path.

Until the composite exists, Basic uses **automatic site selection**: the pane follows the
nearest suitable radar to the map center (or to home when there is no interaction), with
hysteresis so it does not switch back and forth near coverage edges. This stopgap reuses the
site-switch scheduling and service-lifecycle work (`5aabf46`, `d521d93`, `c78049a`).

### 7. Forecast and current-conditions card (in scope)
The owner approved this on 2026-11-26. It is the first step toward a general weather app. The
card uses the NWS API forecast and observation endpoints, which are free and need no
authentication, for the home saved place. It is visible at every level (reveal key
`weather.forecast_card`). Its data-source entry, caching and units follow the normal Data Source
rules (§6), and it ships in its own sub-slice. Because it is shown for the home location, it is
private data and never goes into a shared setup file (§8).

### 8. Custom presets and the shareable setup file
**Built-in tiers versus custom presets.** Basic, Advanced and Professional are the **built-in
defaults**. They ship with the app, every user has them, and they cannot be edited, renamed,
deleted or overwritten. Overrides on them can always be undone with "Reset to … defaults".

A **custom preset** is a named, saved arrangement the user
creates. A user can have **as many as they like**, each for a different purpose. Examples from the
owner: a **"Snow"** preset with winter palettes and precipitation-type products, and a
**"Tornado season"** preset with velocity panes and warning detail. Each preset records which
built-in tier it started from, plus everything listed below. It appears in the level dropdown
under the built-in tiers. Choosing it applies the whole preset in one step, using the same
safe-switching rules as §4. Switching from it to Basic, Advanced, Professional or another preset
works the same way and loses nothing.

**Changing a preset.** Each custom preset has one of two editing modes, picked by the user per
preset:

- **Ask before saving** (the default). Any change marks the preset **"Tornado season
  (modified)"**, and the dropdown offers:
  - **Save to "Tornado season"**: write the changes into the existing preset.
  - **Save as new preset…**: keep the original untouched and save the changes under a new name,
    for example to try a variation before committing to it.
  - **Discard changes**: go back to the preset as last saved.

  Switching to another tier or preset while there are unsaved changes asks the same question
  instead of dropping them.
- **Save changes automatically**. For a preset the user knows they want to keep refining, every
  change is written straight into it and no "(modified)" state appears. The toggle is in the
  dropdown's preset menu and in **Manage presets…**. As a safety net, the app keeps the preset as
  it was when it was last switched to, so **Revert to when I opened it** is always available.

The built-in tiers never take either mode. Changes on Basic, Advanced or Professional are always
overrides (§3), which can be reset or saved as a new custom preset.

**What a setup contains** (presentation and configuration only):
- The base level and reveal-key overrides (§2, §3)
- Toolbar actions and their order
- Pane layout, linking presets, and each pane's product, tilt and palette choice
- **Color tables:** the full text of every `.pal` file the setup uses, embedded so the recipient
  needs no separate files
- The chrome theme (embedded, in the same TOML form `ThemeManager` already imports and exports)
- Map style and map-detail visibility, units, measurement and snap preferences, playback length,
  and warning and overlay choices
- Optional author name and description, typed in by the person exporting

**What a setup never contains** (enforced by an allowlist in the exporter, not by trusting a
checkbox):
- Saved places, the home location, and anything derived from them, such as the forecast card's
  location
- The camera position, last-viewed area, and the selected radar site, because these reveal where
  someone is or watches. A recipient's panes open on their own home location, or on the national
  view if they have none.
- Drawings, measurements, markers and other map objects
- Placefile URLs (they can contain personal tokens), file-system paths, cache settings, update
  preferences, logs, and any account or machine identifiers

The exporter writes only keys on the allowlist, so a setting added later stays out of shared files
until someone deliberately adds it to the list. Export shows a plain summary of what is included
before writing.

**File format:** a single human-readable TOML document (ADR 0003) with the extension `.wxsetup`.
It starts with `format = "wxlens-setup"` and `version = 1`, and holds palettes as multi-line
strings and the theme as a table. One self-contained file is easy to email, post on a forum or
attach to a message, and needs no zip library.

**Importing** treats the file as untrusted input:
- Check the format marker and version. Enforce a size limit. Ignore unknown keys and log them.
  Reject the file outright if it fails to parse.
- Validate each value with the same checks `AppSettings` uses. Values out of range fall back to
  defaults.
- Parse embedded palettes and themes with the existing `.pal` and theme readers, then store them
  as the user's own copies, renaming on a name clash. Nothing in the file is ever run.
- **Importing adds a new custom preset and changes nothing else.** It does not replace the
  current setup, change any other preset, or switch away from the active level. The preview
  (name, author, base tier, pane count, palettes included) offers **Add preset** (the default) and
  **Add and switch to it**. If the name is already taken, the new preset becomes "Tornado season
  (2)" rather than overwriting the existing one. Deleting the preset undoes the import, so no
  separate undo is needed.
- A file can never replace or change Basic, Advanced or Professional. A preset that claims a
  built-in name is imported under a different name.
- Any private keys found in a hand-edited file are dropped without being applied.

## Alternatives considered
- **Individual reveals only (the current approach).** Rejected. It gives no single "make it simple"
  action, and a new user cannot tell which switches matter.
- **Separate simple and advanced UIs.** Rejected. Two information architectures to document,
  test and keep accessible, which §5.3 explicitly forbids.
- **Two tiers.** Rejected by the owner, who asked for at least three. A middle tier also gives the
  hobbyist and spotter group a clear home, which they would otherwise lack.
- **Gating by `level >=` comparisons.** Rejected (see §2). It spreads tier logic through the code
  and makes overrides awkward to add.
- **Sharing by copying the config folder.** Rejected. It carries saved places, the camera
  position, file paths and cache settings, which is exactly the private data §8 must keep out.
- **A zip bundle of separate TOML and `.pal` files.** Rejected for now. One TOML file is easier to
  read, paste and inspect before importing, and needs no new dependency. Revisit only if setups
  ever need binary assets.

## Consequences
- New C++ type (likely `wxlens::settings::ExperienceProfile`) owns the level, the level-to-key
  table, the overrides and `isRevealed()`, exposed to QML through `AppSettings`. Every gated
  feature registers one reveal key.
- New `SetupBundle` exporter and importer in `app/source/wxlens/settings/`, with one allowlist that
  defines what may be shared. Tests: a round trip that brings back an identical configuration,
  checks that private data from a heavily populated profile never appears in the exported file,
  rejection of malformed and oversized files, and renaming of palettes on a name clash.
- Every new setting must be marked shareable or private when it is added. The default is
  private.
- §5.3 is amended to point to this ADR: levels are curated defaults over the same information
  architecture, not separate personas.
- Every new feature from now on must state which reveal key it uses and the lowest level it
  appears at. Features with no key are visible at every level.
- Acceptance testing now covers each level: the §4.8 criteria apply to Professional, and Basic gets
  its own first-run usability check ("empty settings to a useful live radar view" without picking
  a radar site).
- Phase 2's mosaic bootstrap becomes higher priority, since Basic depends on it.

## Implementation log

**Slice 1 — level core (2026-11-26).** `settings::ExperienceProfile` holds the level, the one
level-to-key table (`kRevealKeys` in `experience_profile.cpp`, as each key's lowest level) and the
per-key overrides; `AppSettings` persists it to `experience.toml` and exposes it to QML
(`reveals`, `isRevealed()`, `setRevealed()`, `experienceLevel`). Keys gated in this slice:
`panes.multi` (Advanced), `panes.sync_controls` (Professional; replaces `advancedPaneLinking`),
`tools.measurement` and `tools.markers` (Advanced), `tools.drawing` (Professional, covering line
drawing and range rings). Hiding `panes.multi` caps only the *visible* grid at 1x1
(`PaneGridModel::setMultiPaneEnabled`); the stored layout and hidden panes are kept and saved.
The top bar has the level dropdown (built-in tiers, reset, "Customize…"), and Settings has an
addressable `experience` section.

Decisions taken while implementing, open to revision:
- A brand-new install starts at Basic. An install with existing config (a `workspace.toml` or
  `appearance.toml`) starts at **Professional**, so an upgrade hides nothing the user had (§4).
  The legacy `advanced_pane_linking` value is migrated into a `panes.sync_controls` override and
  removed.
- Picking a level from the dropdown clears that level's overrides; re-picking the current level is
  "Reset to <level> defaults". The global **Reset to defaults** clears overrides but keeps the
  level.

Not yet done, deliberately:
- `radar.site_picker` and `radar.product_browser` are not gated: hiding the site picker before
  automatic site selection (§6) exists would leave Basic stuck on one site.
- Placed analysis objects stay visible when their tool is hidden; §4 asks for them to be hidden at
  lower levels, which needs a per-kind filter in `MapObjectsLayer`.
- Dropping from Professional to Advanced still relinks all cameras through the existing
  `setAdvancedPaneLinking(false)` path, which rewrites custom sync groups. §4 asks for that to be
  display-only; it needs an effective-group layer over the stored groups.
- The layout menu still offers 3x3, while §1 says "up to 4 panes" at Advanced and Professional.
  This slice does not take that layout away from existing users without a decision.
- Custom presets, the `.wxsetup` exporter and the first-run question come in later slices.
