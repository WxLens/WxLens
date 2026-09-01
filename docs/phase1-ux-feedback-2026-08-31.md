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

### Product-aware palette ownership and defaults

- [ ] Remove the process-wide active palette from Level 2 renderer state. Editing or selecting a
  velocity palette must never recolor reflectivity, and vice versa.
- [ ] Assign the correct bundled default palette by canonical Level 2 moment and Level 3 product
  identity. Velocity should open with the conventional red/green velocity palette; reflectivity
  should open with its reflectivity palette.
- [ ] Define the distinction between a product-family default and an explicit per-pane override.
  Changing a velocity-family default should update open velocity panes without touching other
  product families; an explicit pane override remains local unless the Palette synchronization
  channel intentionally links it.
- [ ] Applying an incoming Palette synchronization change must rebuild the receiving pane's LUT
  and repaint immediately.
- [ ] Add renderer-level tests that compare effective LUT/default identity across simultaneous
  reflectivity and velocity panes. Tests that assert only palette-name strings are insufficient.

Retest: open KICX reflectivity and velocity in separate panes; verify their defaults; change and
edit the velocity palette; confirm every intended velocity pane changes and no reflectivity pane
does; repeat with Palette synchronization both disabled and enabled.

### Product-browser wheel isolation

- [ ] Consume wheel/touchpad scrolling while the pointer is over the product browser so browsing
  products never zooms or moves the underlying map.
- [ ] Test rapid wheel input at both ends of the product list, where event leakage is easiest to
  expose.

Retest: record the pane camera, rapidly scroll the complete product list, and verify the camera is
pixel-for-pixel unchanged until the pointer leaves the browser.

## P1 — usability and accessibility blockers

### Weather Overlays contrast and control styling

- [ ] Replace platform-default controls mixed into the dark dialog with theme-controlled QML
  controls for consistent foreground, background, border, hover, focus and disabled states.
- [ ] Correct the low-contrast checkbox labels and status text, and make URL actions visually
  legible and understandable.
- [ ] Verify dark and light themes, keyboard focus visibility, text scaling and relevant WCAG
  contrast targets.

Retest: open Weather Overlays in both bundled themes and operate every control using pointer and
keyboard, including disabled, focused and error states.

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

## Definition of ready for the next user test

The next packaged test build should, at minimum, close both P0 items and the Weather Overlays
contrast defect, include automated regression coverage, and contain a short change list mapping
each fix to the retest steps above. The remaining P1/P2 work may ship incrementally, but its open
status must remain visible here and in the applicable Phase 1 roadmap gates.
