# Phase 1 packaged UX feedback — 2026-08-31

This record consolidates user feedback from an interactive packaged Release session. It is an
execution and retest checklist, not a replacement for `docs/ROADMAP.md`. Architectural changes
must continue to follow the roadmap; items below remain open until implementation and the stated
packaged-app retest both pass.

## Recommended execution order

1. Fix correctness and input-routing defects that can produce misleading radar output or alter
   the map unexpectedly.
2. Fix accessibility and direct pane targeting so the existing capabilities are usable.
3. Simplify and customize persistent chrome without removing capability.
4. Add progressive-disclosure enhancements to site, product, measurement and interrogation
   workflows.

This order produces a trustworthy build for the next user test before spending time polishing
features whose underlying behavior is still wrong.

## P0 — correctness and interaction defects

### Top bar secondary action row disappears after maximizing (found 2026-09-02)

- [x] **Resolved 2026-09-02: not reproducible — the defect was in the measurement, not the app.**
  Retested the packaged Release build interactively: launched windowed, maximized via
  `ShowWindow(SW_MAXIMIZE)`, then restored and re-maximized again. The Tools/Help/Settings action
  row rendered correctly in every state (DPI-aware full-window captures of all three states).
- The original report's evidence was flawed in two ways. First,
  `build-release-vs2026/acceptance-maximized.png` (2026-08-30) shows the pre-redesign top bar and
  is itself a bad capture: it is also missing the OS caption buttons (minimize/maximize/close) in
  its title bar, which the app cannot affect — so the right-hand portion of that screenshot never
  reflected the screen. Second, the "raw screen-pixel sampling" was almost certainly done in
  logical coordinates against a physical-resolution capture (this machine runs 125 % display
  scaling; the maximized window rect is 2066x1238 physical at (-9,-9)), so the sampled "button
  region" fell on empty map left of the real buttons.
- Lesson for future packaged-app verification on this machine: make the capturing process
  DPI-aware (`SetProcessDPIAware`) and compute regions from `GetWindowRect` physical coordinates,
  and treat "zero pixels found" as a capture-pipeline suspect before a rendering bug.

Retest: passed 2026-09-02 (windowed, maximized, and restore→re-maximize all show the
Tools/Help/Settings icons in the top-right of the top bar).

### Product-aware palette ownership and defaults

- [x] Remove the process-wide active palette from Level 2 renderer state. Editing or selecting a
  velocity palette must never recolor reflectivity, and vice versa.
  *2026-09-03:* the editor's active palette is no longer observed by panes at all
  (`PaneController` listens only to `paletteApplied` / `familyDefaultsChanged`), and every pane
  bakes its own LUT from its resolved palette (`PaneController::Impl::ApplyPalette`). Fixing this
  exposed a second defect: a pane with no explicit override used to hand rendering back to the
  shared `RadarSweepProduct`'s factory-baked LUT, so **"Apply to product" on an edited
  reflectivity palette was a silent no-op on every Level 2 pane** — now covered by
  `PanePaletteTest.AppliedEditToAFamilyDefaultReachesPanesWithoutAnOverride`.
- [x] Assign the correct bundled default palette by canonical Level 2 moment and Level 3 product
  identity. Velocity should open with the conventional red/green velocity palette; reflectivity
  should open with its reflectivity palette.
  *2026-09-03:* packaged retest, 2×2 KEAX: switching pane 2 to Velocity opened it with the
  red/green DV ramp while the three reflectivity panes kept DR.
- [x] Define the distinction between a product-family default and an explicit per-pane override.
  Changing a velocity-family default should update open velocity panes without touching other
  product families; an explicit pane override remains local unless the Palette synchronization
  channel intentionally links it.
  *2026-09-03:* `PaletteManager` now owns palette **families** (`SRV`→`DV`, `KDP2`→`KDP`, every
  other bundled palette its own family) and a persisted **family default** (`palettes.toml`,
  `family_default_<FAMILY>`, validated on load so a hand-edited reflectivity ramp can never become
  the velocity default). "Apply to product" publishes the edited text *and* makes that palette its
  family's default; panes resolve explicit override → family default → bundled product default
  (`Impl::ResolvedPaletteName`, shared by the Level 2 and Level 3 paths). Apply no longer writes to
  any pane's `descriptor_.palette`, so an explicit override survives it
  (`PanePaletteTest.ExplicitPaneOverrideSurvivesAFamilyDefaultChange`). The palette chips mark each
  family's current default (●) and the apply notice names the family.
- [x] Applying an incoming Palette synchronization change must rebuild the receiving pane's LUT
  and repaint immediately.
  *2026-09-03:* `PanePaletteTest.PaletteSyncChannelRebuildsTheReceivingPanesLut` compares the
  linked pane's baked LUT colours, not its palette name, and checks the unlinked case stays local.
- [x] Add renderer-level tests that compare effective LUT/default identity across simultaneous
  reflectivity and velocity panes. Tests that assert only palette-name strings are insufficient.
  *2026-09-03:* `test/source/wxlens/panes/pane_palette.test.cpp` (7 tests) installs synthetic
  reflectivity (offset 66) and velocity (offset 129, m/s) sweeps on real `PaneController`s through
  the new `PaneController::layerBinding()` seam and compares the `ColorTableLut` colours the radar
  layer would upload; `palette_manager.test.cpp` gained 5 tests for families, defaults, persistence
  and validation, and full-factory "Reset all". Suite: 146/146 pass.

Retest: open KICX reflectivity and velocity in separate panes; verify their defaults; change and
edit the velocity palette; confirm every intended velocity pane changes and no reflectivity pane
does; repeat with Palette synchronization both disabled and enabled.

Retest 2026-09-03 (packaged Release, 2×2 KEAX, pane 2 Velocity): edited DV's −40 kt stop to
`#0000ff` and pressed Apply → pane 2's "toward" region turned blue, all three reflectivity panes
unchanged; discarding the editor draft afterwards left the applied palette in place;
`palettes.toml` gained `family_default_DV`. The Palette-sync-enabled half of the retest is covered
by the automated LUT test above and was **not** driven through the packaged sync UI in this pass —
include it in the next manual test session. Follow-up (P2, not blocking): after Apply the editor
still reports the draft as unsaved, so closing asks "Save changes to DV?" even though the change is
already applied — the prompt should distinguish "applied but not saved to a .pal" from "unapplied".

### Product-browser wheel isolation

- [x] Consume wheel/touchpad scrolling while the pointer is over the product browser so browsing
  products never zooms or moves the underlying map.
  *Implemented 2026-08-31* (`ProductBrowser.qml` root `WheelHandler` consumes what the list
  declines at either end); *verified 2026-09-03*, see below.
- [x] Test rapid wheel input at both ends of the product list, where event leakage is easiest to
  expose.

Retest: record the pane camera, rapidly scroll the complete product list, and verify the camera is
pixel-for-pixel unchanged until the pointer leaves the browser.

Retest 2026-09-03 (packaged Release, KEAX Level 3 catalog loaded, `tools/retest/ui-drive.ps1`):
ten synthetic wheel bursts of 15–40 notches at 25 ms and 40 ms spacing, both directions, driven
through both ends of the list; plus single notches and 100–200 ms bursts at each boundary. In every
run where the cursor stayed over the browser the map region (1000×850 px right of the browser) was
**pixel-identical** before and after (`tools/retest/compare-region.ps1`), while the list moved only
when it had room to. Caveat for whoever repeats this: an early pass showed apparent "leaks" that
turned out to be the physical mouse being nudged onto the map mid-burst, and one capture that was
of a window covering the app — the driver now verifies the foreground window and the window under
the cursor before and after every burst and reports displaced runs as invalid rather than as leaks.

## Retest session 2026-09-03 (second pass) — new defects found

Driven with `tools/retest/ui-drive.ps1` against the packaged Release build. Four new defects and
one blocked retest came out of it; each is filed in the appropriate priority section below.

### P0 — Palette synchronization has no user interface

- [ ] The Palette sync channel exists in the model (`SyncChannel::Palette`, per-pane groups,
  covered by `PanePaletteTest.PaletteSyncChannelRebuildsTheReceivingPanesLut`) but **nothing in
  the QML calls `setSyncGroup` at all**. The only sync control shipped is the per-pane
  "Unlinked / Link A / Link B" button, which calls `setCameraSyncGroup` and therefore links only
  Location, Zoom, Bearing and Pitch. Product, Palette, Time, Animation and SelectedStorm cannot be
  linked from the packaged app.
- This makes the P0 palette-ownership retest's "repeat with Palette synchronization both disabled
  and enabled" step **impossible to perform as written**, and it means §4.1's per-channel sync
  model - the thing the pane architecture is built around - is ~80 % unreachable by users.
- Decide whether Phase 1 ships the full per-channel link surface (a menu on the link button, per
  §4.1's "link everything / link camera / link palette" presets) or explicitly defers it with the
  limitation named. Right now it is neither shipped nor recorded as deferred.

Implemented 2026-09-03: the pane link indicator now opens effect-named presets for **Map view
only**, **Map view + radar site**, **Palette only**, **Everything**, and **Independent**, in either
group A or B. Presets remain thin UI sugar over per-channel membership; joining adopts the
existing group's current values, including an immediate palette LUT rebuild. Automated preset
coverage passes. The box remains open until the packaged Palette-only retest below passes.

Retested 2026-09-03 (packaged Release, 2x2 KEAX, two velocity panes both set to **Palette only ·
A**): the menu and presets work, and changing the velocity family default to `SRV` repainted both
velocity panes while leaving both reflectivity panes untouched. Two findings, neither a regression:

1. **The link had no observable effect.** Nothing in the QML exposes a per-pane palette override
   (`PaneController::setPaletteName` is not reachable from any QML file), so the only palette
   change a user can make is the family default - which already reaches every pane of that field,
   linked or not. "Palette only" therefore cannot yet be distinguished from "Independent" by
   observation. The channel is real and unit-tested; what is missing is the per-pane override UI
   that would give it something to carry. Until that ships, this preset is a control that appears
   to do nothing.
2. **The group letter was wrong**, showing "Palette @" instead of "Palette A". The new `group`
   binding read `paneGridModel.syncGroupForPreset(...)` without also reading `syncRevision`, so
   QML could not know when to re-evaluate it; it kept its initial `0` and
   `String.fromCharCode(64 + 0)` rendered "@". This is the exact staleness the neighbouring
   `preset` binding's comment warns about. Fixed in `PaneHost.qml`; needs a visual re-check.

Retest: link two velocity panes on Palette only, change one pane's palette, confirm the other
repaints and that an unlinked third pane does not; confirm camera stays independent when only
Palette is linked.

### P1 — Bottom control bar is unreachable at increased text scaling

- [ ] Launched with `QT_SCALE_FACTOR=1.25` (app-local simulation of a 150 %-scaled desktop; the
  machine's own scaling was not changed) the window opens 2018x1297 and the floating bottom
  control bar is **clipped below the bottom of the screen, behind the taskbar** - LIVE/Archive,
  the time field, site, product, tilt and the layout selector are all unreachable. The top bar and
  pane headers scale correctly, so this is specifically the window sizing / floating-bar anchoring
  at scale, not general scaling breakage.
- Docked mode may or may not be affected - not yet checked.

Implemented 2026-09-03: initial window dimensions are capped to the active screen's available
desktop area, and the bottom toolbar is now exposed as an accessible toolbar whose reusable
buttons are keyboard-operable and named.

Retested 2026-09-03 (packaged Release, `QT_SCALE_FACTOR`) — the first cap **did not work**: the
window still opened 2018x1297 at 1.25 and 2418x1427 at 1.5 on a 2066x1238 screen, byte-identical
to the pre-fix measurement. `Screen.desktopAvailableWidth/Height` in QML was not comparable with
the window's own size in the units that size is expressed in, so `Math.min` silently chose the
unclamped value while the same function's reposition branch still ran (the window moved but never
shrank). Replaced with `FitWindowToScreen()` in `main/main.cpp`, which does the arithmetic in
`QWindow`/`QScreen` device-independent pixels, accounts for the frame margins, logs what it
changed, and re-fits when the window moves to another screen.

- [x] Verified 2026-09-03: 1.25 -> 2018x1230, 1.5 -> 2066x1230 (both inside the 2066x1238
  available area; previously 1297 and 1427 tall), 1.0 unchanged at 1618x1047. Floating/docked and
  a visual confirmation of every bottom-bar control at scale remain for the next session - the
  measurement above is window geometry, not a click-through of each control.

Retest: run at 125 % and 150 % system scaling (and via `QT_SCALE_FACTOR`), floating and docked,
and confirm every bottom-bar control is on screen and clickable.

### P1 — Clipped status text at 1280x800

- [ ] At 1280x800 in a 2x2 layout an orange status string (observed: "Calculating") is drawn at
  the very bottom edge of the window and is cut in half. The rest of the layout is legible at that
  size - pane headers, both product labels and the floating bottom bar all fit - so this is one
  mis-anchored status element, not a general density failure.

Implemented 2026-09-03: per-pane product/status details reserve the floating control zone instead
of anchoring eight pixels from the window edge, and the obsolete process-wide single-site status
bridge was removed from application startup. Packaged 1280x800 retesting remains.

Retest: 1280x800, 2x2, while a sweep is being computed; confirm the status text is fully visible
and does not overlap the attribution or the control bar.

### P1 — Accessible names and roles are absent almost everywhere

- [ ] A static audit of `app/qml` found **89 `MouseArea`-based controls across 17 files with no
  `Accessible.role` or `Accessible.name`**; the only accessible metadata in the entire UI is the
  three entries in `OverlaysDialog.qml` added on 2026-09-03. Because the app's controls are
  `Rectangle` + `Text` + `MouseArea` rather than Qt Quick Controls, a screen reader sees no
  actionable elements at all - not the Tools menu, Settings, the pane headers, the product
  browser, the site picker, or the palette editor.
- This is the "accessible names/roles" half of the Phase 1 first-run-and-polish gate. A screen
  reader pass is pointless until roles and names exist, so the *audit* below replaces the reader
  pass as the immediate next step.

Retest: add roles/names to the interactive controls, then drive the app with Narrator or NVDA and
confirm every action is reachable and announced.

### P1 — Palette tests ran against palettes the application does not ship (found 2026-09-03)

- [x] `test/test.cmake` bundled `DR.pal`, `DV.pal` and `SRV.pal` from
  `external/legacy-supercell-wx`, while `app/CMakeLists.txt` overrides `DR` and `DV` with
  WxLens's own ramps. Every palette test therefore ran against data the application never loads:
  the vendored `DV` declares `Units: KT`, WxLens's declares `MPH`, and units are exactly what the
  product-family matching reads. A test asserting that a knots palette matches the velocity family
  passed in CI while the packaged app refused the same file.
  *Fixed 2026-09-03:* `test.cmake` now mirrors `app/CMakeLists.txt`, bundling the full vendored WCT
  set plus the two application-owned overrides. Any future divergence changes test behaviour, so it
  will be noticed.
- Lesson: a test fixture that "looks equivalent" to the shipped resource is not equivalent. Where
  the application overrides a vendored asset, the test target has to make the same substitution.

### P1 — Imported palettes were matched by unit spelling, not by quantity (found 2026-09-03)

- [x] The import preview matched a `.pal` to product families by comparing its `Units:` string
  literally. WxLens's velocity ramp declares `MPH` while the community's velocity palettes - and
  the vendored `SRV`/`SW` - declare `KT`, so importing an ordinary knots velocity palette offered
  "Use for spectrum width" and **not** velocity: precisely the palette the user most wanted to
  link was the one the feature hid.
  *Fixed 2026-09-03:* added `PaletteManager::UnitsQuantity()`, which maps a declared unit to the
  physical quantity it measures (KT/MPH/M/S/KM/H -> speed, DEG and DEG/KM -> the KDP field, and so
  on) and matches on that. `CanonicalUnits()` keeps the declared spelling for display, so the
  editor still shows "MPH" or "KT" as written. `BuildColorTableLut` already rescales sample values
  to whatever the chosen table declares, so cross-unit linking renders correctly.
  Covered by `PaletteManager.UnitsMatchByQuantityNotSpelling`, which asserts the end-to-end case
  against the real bundled catalog.

### Retest results — what passed

- **Direct pane targeting** (P1 below): clicking a non-first pane makes it active. Verified in 2x2:
  clicking the bottom-left pane moved the active indicator to it, the bottom bar switched to `P3`,
  and the active pane's header border became visible. The **per-pane site-picker search** half was
  not exercised (the driver's click missed the station button and the session ended), so that
  bullet stays open.
- **Product-family palette defaults** end-to-end in the packaged app: two panes set to velocity
  both rendered the DV ramp while the two reflectivity panes were untouched.
- **1280x800 legibility** apart from the clipped status text above.
- **Top bar at 1.25 scale**: Tools/Help/Settings all render correctly, so the resolved
  "disappearing action row" P0 stays resolved at increased scaling.

## P1 — usability and accessibility blockers

### Weather Overlays contrast and control styling

- [x] Replace platform-default controls mixed into the dark dialog with theme-controlled QML
  controls for consistent foreground, background, border, hover, focus and disabled states.
  *Implemented 2026-09-01* (`OverlayButton` / `OverlayCheckBox` / `OverlayTextField` components).
- [x] Correct the low-contrast checkbox labels and status text, and make URL actions visually
  legible and understandable.
  *2026-09-03:* the token audit (`tools/contrast-audit.py`) found three real problems the
  themed controls still had: the unchecked checkbox box and the empty text field were only their
  `border`-token outline at 1.3–1.7:1 (WCAG 1.4.11 needs 3:1 for a form control's boundary), and a
  failed placefile's row used `danger` text at 3.7:1 in Operational Dark. Outlines now use
  `textMuted` (≥3.6:1 in both themes); a failed placefile keeps its name in `textSecondary` and
  shows "⚠ <error>" on its own line in `warning` (≥4.7:1 on both row backgrounds, and not
  colour-only), with an accessible name that includes the error.
- [x] Verify dark and light themes, keyboard focus visibility, text scaling and relevant WCAG
  contrast targets.
  *2026-09-03:* `python tools/contrast-audit.py` — 17/17 targeted pairs pass in both bundled
  themes (4.5:1 text, 3:1 focus ring and outlines); the two informational rows (labelled-button
  border, disabled label) are WCAG-exempt and recorded so a theme change is noticed. Text scaling
  was exercised later the same day and found an app-wide defect, filed separately as "Bottom
  control bar is unreachable at increased text scaling".

Retest: open Weather Overlays in both bundled themes and operate every control using pointer and
keyboard, including disabled, focused and error states.

Retest 2026-09-03 (packaged Release): opened the dialog in Operational Dark and Daylight; added a
placefile from an unreachable address to produce the error row; tabbed to a button to confirm the
2 px `primary` focus ring; exercised the disabled "Add from web" state; removed the placefile.
Everything legible in both themes. Remaining for the next manual session: text scaling (Windows
125 %/150 %) and a screen-reader pass over the dialog's accessible names.

### Direct pane targeting and radar-site selection

- [ ] Clicking a pane or its header makes it the active pane with a clear but unobtrusive visual
  indication. Shared controls then target it directly; cycling through `P1`/`P2`/… must not be the
  primary targeting workflow.
- [ ] Make the pane's station indicator clickable and open a searchable per-pane radar-site picker.
  Search must support radar ID, station name, city and state/region where the site database supplies
  those fields.
- [ ] Add a **Center map when radar site changes** preference, enabled by default. A site change
  centers that pane at a sensible radar-view zoom; linked Location/Zoom channels follow their
  established synchronization rules. Users retaining a fixed geographic view can disable it.

Retest: use 1x1, 2x2 and 3x3 layouts; target non-first panes directly; find sites through each
supported search field; verify centering on/off and linked/unlinked camera behavior.

Implemented 2026-09-01: pane taps and header actions select the active pane; the active border is
visible only in multi-pane layouts. The station button opens a per-pane picker backed by the
bundled site database and searches ID, place/city, state/region and country. The persisted
`radar-sites` setting defaults to centering at zoom 7 and writes through the existing Location and
Zoom channels. Packaged interactive retesting across all requested layouts remains required before
checking the acceptance boxes above.

### Pane and top-bar information density

- [ ] Reduce each pane header to a responsive essential summary: station, product, data timestamp
  and exceptional loading/error state. Put full station name, archive/live details, elevation,
  exact product code, synchronization metadata and other diagnostics in an expandable details
  surface.
- [ ] Remove the obsolete process-wide radar status line once equivalent per-pane information is
  available. It currently becomes especially misleading in multi-site layouts.

Retest: inspect 1x1 through 3x3 at 1280x800 and the native development display; verify essentials
remain legible without covering meaningful radar data.

## P2 — progressive disclosure and expert efficiency

### Searchable, grouped product browser

- [ ] Group AWIPS variants beneath friendly product families such as Super-Resolution
  Reflectivity. Ordinary selection uses a recommended/default variant without requiring knowledge
  of AWIPS codes.
- [ ] Allow a family to expand so an expert can choose variants such as `N0B`, `N1B` or `N2B`.
- [ ] Add fast search across friendly description, category and AWIPS ID; keep short codes visible.

Retest: select a common product without knowing its code, then locate a specific variant by typing
its AWIPS ID. Verify selection remains per pane unless Product synchronization is enabled.

Implemented in part 2026-09-01: fast filtering now covers description, category, identity and
AWIPS ID, with results ordered and visibly labelled by category and codes retained. Collapsible
friendly families with recommended variants remain open, so the checklist above is intentionally
unchecked.

### Minimal and customizable persistent chrome

- [ ] Ship a curated minimal toolbar containing only frequent, broadly useful actions.
- [ ] Move Overlays, Saved Places, Map Details, palette management and other secondary top-bar
  actions into one clear menu/overflow surface unless testing demonstrates that an item merits
  permanent placement.
- [ ] Move floating-versus-docked control-bar selection into Settings rather than spending a
  permanent toolbar target on it.
- [ ] Let users show/hide optional toolbar controls, persist the configuration and reset it to the
  curated default. Hidden capabilities must remain reachable through the complete tools/menu/search
  surface and keyboard commands.
- [ ] At narrow widths and in dense pane layouts, collapse labels and overflow lower-priority
  actions instead of shrinking targets or covering panes.

Retest: begin from fresh settings, customize the toolbar, restart, reset it, and confirm every
hidden action remains discoverable. Repeat at 1280x800 in 3x3 floating and docked layouts.

Implemented in part 2026-09-01: secondary top-bar actions moved into one persistent Tools menu;
each can be restored as a persisted optional shortcut from the stable `toolbar` settings section,
with a one-action curated-default reset. Narrow-width label collapse/overflow and packaged
interactive retesting remain open.

### Measurement tool preferences and radar-value reader

- [ ] Show one user-preferred measurement/interrogation tool as the primary action. Click activates
  it; click-and-hold opens other tools and can change the preferred default. Provide an equivalent
  keyboard-accessible picker.
- [ ] Put compact help and settings affordances beside the tool surface. Help explains gestures and
  readouts; settings deep-links to the stable `measurement` section.
- [ ] Add retain-versus-clear-session-measurements on tool deactivation. Clearing affects only
  measurements created during that tool activation and never erases separately saved objects.
- [ ] Add a radar-value reader that reports the decoded product value and unit (for example dBZ),
  selected radar site, distance, azimuth/bearing and actual elevation cut where available. Keep it
  temporary by default with an explicit pin action; do not infer AGL without terrain data.

Retest: change the preferred tool, restart, exercise pointer and keyboard selection, verify both
deactivation policies, and compare the reader against a known radar bin and geodesic calculation.

Implemented in part 2026-09-01: the bottom surface now exposes one persisted preferred measurement
tool, right-click switches between point-to-point and path, and the adjacent help/settings action
deep-links to `measurement`. Hold-to-open, keyboard picker parity, session-retention policy and the
decoded radar-value reader remain open.

## Definition of ready for the next user test

The next packaged test build should, at minimum, close both P0 items and the Weather Overlays
contrast defect, include automated regression coverage, and contain a short change list mapping
each fix to the retest steps above. The remaining P1/P2 work may ship incrementally, but its open
status must remain visible here and in the applicable Phase 1 roadmap gates.

**Status 2026-09-03:** the two original P0 items and the Weather Overlays contrast item are closed
with the evidence recorded above (automated: `wxlens-app-test` 151/151,
`tools/contrast-audit.py`; packaged: the dated retest notes).

The second retest pass that same day then found **four new defects** - see "Retest session
2026-09-03 (second pass)" above - of which one is a new P0 (the Palette sync channel, and in fact
every non-camera channel, has no user interface). So the next test build's bar has moved: it now
needs the sync-UI decision plus the scaling and accessibility work, not just the three items that
closed. The remaining `docs/ROADMAP.md` completion gates are unchanged except that the name
screening is now closed.
