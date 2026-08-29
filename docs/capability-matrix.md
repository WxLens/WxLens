# Capability & interaction-taxonomy audit (Phase 0.5)

Per `docs/ROADMAP.md` §7 Phase 0.5: a side-by-side of current Supercell Wx, publicly documented
AWIPS/CAVE capability, and planned WxLens capability, plus a product taxonomy and a tool/
interaction taxonomy — so later phases extend a stable vocabulary instead of inventing one ad hoc
per feature. This is a research/documentation deliverable, not code.

AWIPS claims below are sourced from NOAA/NWS public training material (OCLO's "AWIPS
Fundamentals" Virtual Lab pages and the NSF Unidata AWIPS Manual) — no proprietary/internal
material was used. See Sources at the end.

## Capability matrix

| Capability | Current Supercell Wx | AWIPS/CAVE (public docs) | Planned WxLens |
|---|---|---|---|
| Multi-pane display | Linked/unlinked grid, per-site data sharing (`docs/ROADMAP.md` §2 Context) | Configurable perspectives with groups of editors; sites have moved from fixed side-panes toward flexible multi-editor layouts on larger monitors | Per-channel sync (§4.1), pane = generic View not fixed radar viewport (§4.6) — a superset of both prior models |
| Camera/view linking | Single linked/unlinked boolean per pane group (`map_link_policy.cpp`) | Editors can be grouped, but public docs don't describe a granular per-property sync model | Per-channel (`Location`/`Zoom`/`Bearing`/`Product`/... independently groupable), §4.1 — no public precedent for this granularity in either prior system |
| Single-site radar (Level 2/3) | Full NEXRAD Level 2/3 via `wxdata`, ~90 ICD product codes | Full NEXRAD Level 2/3 via "Local Radar Stations"/"Radar" data menus | Same `wxdata` coverage, reused unmodified (Phase 1) |
| Multi-radar mosaic | None | MRMS-derived national mosaic products under the "MRMS" data menu | Phase 2 — `MosaicDataService`, MRMS via AWS Open Data (`docs/data-sources.md`) |
| Satellite imagery | None | Full GOES-R ABI product suite under "Satellite" data menu | Phase 3 — GOES via AWS Open Data |
| Model/gridded fields | None | Volume Browser: plan views, cross-sections, time-height, var-vs-height, soundings, time series, over full NWP model suite ("Models" data menu) | Phase 3 — jet stream/MSLP only from NOMADS GRIB2 initially; full Volume-Browser-equivalent breadth is out of scope for the stated phase set |
| Soundings | None | NsharpEditor Skew-T, driven by the Volume Browser's Interactive Points Tool, for both observed and model soundings | Phase 3 — Wyoming/RUC observed soundings; tabular-vs-diagram form factor is `[OPEN QUESTION]` #5 in `docs/ROADMAP.md` §9 |
| Cross-section / time-height | None | Core Volume Browser feature, loopable through time or space | Not in the Phase 0-4 scope this roadmap plans; flagged below as a Phase 3+ candidate, not a second-class add-on |
| Storm tracking / STI | None surfaced in UI (STI is parsed but unused) | Storm-relative tools, SCAN, WarnGen's "drag me to storm" centroid workflow | Phase 1 builds the `SelectedStorm` sync channel + basic STI-based selection UI (§4.1); full SCAN-equivalent analytics not planned |
| Warnings/watches overlay | Yes, via `wxdata/awips` text-product parsing | Full WarnGen issuance + display workflow (issuance is out of scope for WxLens, a viewer not a forecast-ops tool) | Phase 1 — display only, reusing `wxdata/awips`, no issuance workflow (WxLens is a viewer, not a WFO ops tool) |
| Measurement / interrogation | Implicit radar-relative range/azimuth via `radar_coordinate_table.cpp`; no general point-to-point tool | "All Tilts" products, multi-volume-scan interrogation (SAILS/MRLE-aware), sampling readouts | Phase 1 — explicit point-to-point/radar-to-point/multi-segment measurement framework (§4.4), radar beam-height interrogation (§4.7); SAILS/MRLE-aware "last actual volume" browsing is not separately planned but isn't precluded by the architecture |
| Palette/color-table editing | Advanced `.pal` editor (per-stop customization) | Fixed, admin-configured color tables; no public end-user color-table editor is documented | Phase 1 — QML rewrite of the existing editor, same `.pal` engine (§5.1); this is a capability WxLens already exceeds AWIPS on |
| UI theming | Native `QPalette`/`QStyle`, identified as the core limitation motivating this rewrite | Fixed CAVE look, not end-user themeable per public docs | Phase 1 — full QML `ThemeManager`, ≥2 bundled themes (§5.2) |
| Placefile overlays | Yes, GR-format via `wxdata/gr/placefile.cpp` | Not applicable — GR placefiles are a GRLevelX-ecosystem community format, not an AWIPS concept | Phase 1 — reused unmodified |
| 3D storm structure | None | No public-facing volumetric storm rendering in CAVE's documented D2D feature set | Phase 4, explicitly last priority (§7 Phase 4) |
| Multi-user/server | None | AWIPS is inherently a networked, multi-workstation system (EDEX/CAVE architecture) but that's operational infrastructure, not an end-user feature comparable to WxLens's stretch goal | Phase 5, explicitly unscoped stretch goal |

## Product taxonomy

The kinds of Data Products the §4.6 pipeline will eventually carry (radar ships first; this list
exists so later additions plug into an already-considered slot rather than forcing a taxonomy
rework):

- **Radar moments** (Phase 1, via `wxdata`): reflectivity, velocity, spectrum width, ZDR,
  KDP/PHI, CC/RHO, HCA, VIL, echo tops, precip accumulation, meso/hail/TVS detection markers, TDWR
  products. (~90 ICD codes already parsed — see `docs/ROADMAP.md` §2.)
- **Radar mosaic fields** (Phase 2): national/regional reflectivity, echo tops, precip, sourced
  from MRMS.
- **Satellite bands** (Phase 3): GOES ABI channels (visible, IR, water vapor) and derived products
  (CMIP/MCMIP composites).
- **Model/gridded fields** (Phase 3, narrow slice only): 250mb wind (jet stream), MSLP. AWIPS's
  full Volume Browser breadth (arbitrary model/level/parameter combinations) is explicitly not
  targeted by the current phase plan — noted here as a gap for a future phase to pick up
  deliberately, not to be filled speculatively.
- **Soundings** (Phase 3): observed (Wyoming/RUC) initially; model soundings not planned yet.
- **Warnings/watches** (Phase 1, via `wxdata/awips`): polygon-based text products (severity,
  phenomenon, VTEC).
- **Placefiles** (Phase 1, via `wxdata/gr`): community vector overlays (icons/lines/polygons).
- **User Analysis objects** (Phase 1, via `objects/`): markers, drawings, measurements, range
  rings, annotations, storm tracks — not a "data product" from an external source, but part of the
  same View-facing taxonomy since they render in the same layer stack (§4.3).
- **Volumetric radar** (Phase 4): the same Level 2 volume scan as radar moments, consumed
  differently (full elevation stack rather than a single tilt) for 3D reconstruction.

## Tool/interaction taxonomy

The map-interaction vocabulary the app should eventually support without its architecture treating
any of them as afterthoughts. Status reflects whether §4 already designs for it:

| Interaction | §4 coverage | Notes |
|---|---|---|
| Pan / Zoom / Rotate / Pitch | Covered — `Location`/`Zoom`/`Bearing`/`Pitch` sync channels (§4.1) | |
| Identify (click for point info) | Covered — Measurement framework's "Point info" mode (§4.4) | |
| Probe (live drag/hover readout) | Covered — temporary interrogation tier (§4.3's three-tier lifecycle) | |
| Measure (point-to-point, multi-segment) | Covered — §4.4 | |
| Radar range/azimuth | Covered — §4.4's "Radar → Point" mode | |
| Radar beam geometry (beam height) | Covered — §4.7 | |
| Storm selection | Covered — `SelectedStorm` sync channel (§4.1), STI-based UI scoped into Phase 1 | |
| Draw / Annotate | Covered — unified `MapObject` types (§4.3) | |
| Select (pane, object) | Covered implicitly — pane focus, `MapObject` scope resolution (§4.3) | Not called out as its own named tool anywhere in §4; fine as an implicit UI affordance, doesn't need its own architecture. |
| Compare (side-by-side panes) | Covered — this is what the multi-pane grid + per-channel sync already is (§4.1, §4.6) | |
| Cross-section | **Not designed yet** | No radar/model 3D-volume slicing concept exists in §4. Flag as a Phase 3+ candidate once soundings/model volumetric data exist — don't let Phase 1's architecture implicitly preclude it (e.g. don't assume a View only ever renders a single 2D plan-view tile stack). |
| Time series | **Not designed yet** | Same gap as cross-section — needs a concept of "a Data Product sampled across a time range at a point/area," which today's pipeline (§4.6) describes only as "a pane's current product at its current time." Phase 3+ candidate. |
| Vertical profile (soundings) | **Not designed yet** | Depends on the sounding Data Source existing first (Phase 3); the Skew-T/tabular form-factor open question (`docs/ROADMAP.md` §9 Q5) is adjacent to this. |

**Conclusion for Phase 1**: no changes needed to §4's architecture as a result of this audit — the
three gaps (cross-section, time series, vertical profile) all depend on data sources that don't
exist until Phase 3, so they're correctly deferred, not accidentally precluded. The one thing worth
carrying forward explicitly: when Phase 3 designs the sounding/model Data Sources, revisit whether
a Data Product needs to support "sampled across time" or "sampled across a spatial cross-section"
as first-class query shapes, rather than only "current value at current time for current pane."

## Sources
- [AWIPS Fundamentals — Perspective, Displays, Panes, and Editors](https://vlab.noaa.gov/web/oclo/awipsfundamentals?page=perspective-displays-panes-and-editors)
- [AWIPS Fundamentals — Radar Displays](https://vlab.noaa.gov/web/oclo/awipsfundamentals?page=radar-displays)
- [AWIPS Fundamentals — Volume Browser](https://vlab.noaa.gov/web/oclo/awipsfundamentals?page=volume-browser)
- [AWIPS Tips: Explore the CAVE Volume Browser — Plan Views (Unidata)](https://www.unidata.ucar.edu/blogs/news/entry/awips-tips-explore-the-cave1)
- [AWIPS Tips: Explore the CAVE Volume Browser — Cross Section and Time Series (Unidata)](https://www.unidata.ucar.edu/blogs/news/entry/awips-tips-explore-the-cave2)
- [AWIPS Tips: Explore the CAVE Volume Browser — Model Soundings (Unidata)](https://www.unidata.ucar.edu/blogs/news/entry/awips-tips-explore-the-cave3)
- [D2D Perspective — NSF Unidata AWIPS Manual](http://unidata.github.io/awips2/cave/d2d-perspective/)
- [WarnGen Walkthrough — NSF Unidata AWIPS Manual](http://unidata.github.io/awips2/cave/warngen/)
