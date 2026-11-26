# Data source catalog

Living catalog of external data sources WxLens integrates or plans to integrate, by phase. This
is the detail version of `docs/ROADMAP.md` §6 — update it as agents actually integrate a source
(access pattern specifics, gotchas, auth requirements found in practice), rather than duplicating
§6's table verbatim. §6 stays the planning-level summary; this file is the operational reference.

## Phase 1 (implemented via `wxdata`, already working)

| Layer | Source | Access pattern | Status |
|---|---|---|---|
| NEXRAD Level 2 (single site) | NOAA `noaa-nexrad-level2` S3 bucket (AWS Open Data) | `wxdata/provider/aws_level2_data_provider.cpp`, unauthenticated | Reused from `wxdata`, unmodified |
| NEXRAD Level 2 (chunks, live) | Same bucket, chunked live-volume mode | `wxdata/provider/aws_level2_chunks_data_provider.cpp` | Reused from `wxdata`, unmodified |
| NEXRAD Level 3 (single site) | NOAA `unidata-nexrad-level3` S3 bucket + HTTP mirrors | `wxdata/provider/aws_level3_data_provider.cpp`, `http_level3_data_provider.cpp` | Reused from `wxdata`, unmodified |
| Warnings/watches (AWIPS text products) | NWS API, IEM API | `wxdata/provider/nws_api_provider.cpp`, `iem_api_provider.cpp`, `warnings_provider.cpp` | Reused from `wxdata`, unmodified |
| Ondas (community radar network) | Ondas API | `wxdata/provider/ondas_level2_data_provider.cpp`, `ondas_level3_behavior.cpp` | Reused from `wxdata`, unmodified |

## Phase 2 — multi-site mesh/mosaic

| Layer | Primary source | Access pattern | Notes |
|---|---|---|---|
| Multi-radar mosaic | NOAA MRMS | `noaa-mrms-pds` S3 bucket, AWS Open Data (free, unauthenticated) | GRIB2 format, needs a decoder — see below. |
| Mosaic bootstrap | IEM MRMS tile/WMS endpoints (`mesonet.agron.iastate.edu`) | Same operator `wxdata`'s `iem_api_provider.cpp` already talks to | Pre-rendered tiles are a faster first cut than raw GRIB2. |
| Recent large-hail detections (MESH) | NOAA MRMS `MESH`/`MESH_Max_*` products, same `noaa-mrms-pds` bucket | Same GRIB2 decoder as the mosaic layer above | Candidate overlay: recent (e.g. 30min-24hr trailing window) high-confidence large-hail detection markers/contours, thresholded by estimated size — a display/filtering feature on top of the existing MESH field, not a new data source. Backlog, see `docs/ROADMAP.md` §8. |

## Phase 3 — additional data layers

| Layer | Primary source | Access pattern | Notes |
|---|---|---|---|
| Satellite imagery | NOAA GOES-16/18/19 | `noaa-goes16`/`-goes18`/`-goes19` S3 buckets (free) — ABI L1b or L2 CMIP/MCMIP, NetCDF4 | Needs a NetCDF4 decoder + geostationary reprojection. |
| Satellite bootstrap | SSEC RealEarth or NOAA nowCOAST WMS/ArcGIS image services | Pre-rendered PNG/WMS tiles | Faster first cut. |
| Soundings | University of Wyoming upper-air archive | Plain HTML/text tables, no auth | Simplest to parse. |
| Soundings alt | `rucsoundings.noaa.gov` (RAOB text) | Plain text, no auth | Real-time-oriented complement. |
| Jet stream / pressure / convective parameters | NOAA NOMADS (GFS/RAP/HRRR) | GRIB2, free, unauthenticated HTTP | 250mb wind = jet stream, MSLP = surface pressure. Scope widened 2026-09-25 (`docs/ROADMAP.md` §9 Q13, resolved) to also include CAPE, CIN, Supercell Composite, Significant Tornado Parameter, 500mb/850mb wind, and max updraft helicity — same decoder/access pattern as the original slice. |
| Surface analysis (RTMA) | NOAA Real-Time Mesoscale Analysis | GRIB2, free, unauthenticated — likely same NOMADS-adjacent access pattern, TBD at kickoff | Hourly-ish CONUS surface temp/dewpoint/wind/visibility/cloud-cover analysis. Added to Phase 3 scope 2026-09-25 alongside the widened model-field set above; a separate product/access pattern from the GFS/RAP/HRRR row, not a variant of it. |
| Overlay bootstrap | NOAA nowCOAST pre-rendered tiles | WMS/REST | Same bootstrap-first pattern. |
| Terrain/DEM (feeds beam-center AGL, §4.7) | Mapzen/Terrarium elevation tiles | `elevation-tiles-prod` S3 bucket (free) — PNG-encoded elevation raster tiles | Not needed until AGL display is implemented. |

**GRIB2/NetCDF4 decoder** (needed for MRMS, GOES, NOMADS): evaluate **eccodes** (Apache-2.0) vs.
**NCEPLIBS-g2c** (permissive) for GRIB2, and **netcdf-c** (MIT-style) for NetCDF4, at Phase 2/3
kickoff — license + binary-footprint review first, per `docs/ROADMAP.md` §0/§9 Q7. Not evaluated
yet as of Phase 0.

## Phase 3+ candidates from the competitor feature-parity audit (2026-09-25, not yet scoped)

A pass through two competing radar apps' settings surfaces (catalogued for ideas/inspiration
only, per the "never copied" discipline in `docs/ROADMAP.md` §0.1 — no UI layout, branding, or
proprietary implementation carried over) turned up several free/public NOAA-family sources not
previously listed here. None of these are scheduled into a phase yet; they're candidates for
whenever Phase 3's "togglable overlay stacking" work picks them up (`docs/ROADMAP.md` §7 Phase 3,
§8 backlog).

| Layer | Primary source | Notes |
|---|---|---|
| SPC Mesoscale Discussions | SPC MD product (text + polygon) | Severe/Precip/Winter categories; text-product link + polygon, similar shape to existing warnings parsing. |
| SPC Convective Outlooks (Day 1-8) | SPC Convective Outlook (categorical + probabilistic: tornado/wind/hail) | Day 1-3 full categorical+probabilistic, Day 4-8 probabilistic only. |
| Hydrological Outlooks | NWS/WPC hydrologic outlook products | Day 1-3 standard + Day 4-5 extended. |
| Fire Weather | SPC Fire Weather Outlook (Conditions + Dry Thunderstorms, Day 1-2) | Distinct from Drought Monitor below, often paired in UI. |
| Drought Monitor | U.S. Drought Monitor (NDMC/USDA/NOAA joint product) | Weekly-updated polygon overlay, separate data source from SPC fire weather. |
| Winter Weather Outlooks | NWS/WPC Winter Storm Severity Index (WSSI) + snowfall/freezing-rain probability fields | WSSI has a distinct impact-level legend (minor/moderate/major/extreme) worth surfacing in-app if built. |
| Climatological Outlooks | NOAA CPC (Climate Prediction Center) temperature + precipitation outlooks | 6-10 day, 8-14 day, 3-4 week, 1 month, 3 month ranges. |
| METAR surface observations | NWS/AWC METAR feed (already adjacent to `iem_api_provider.cpp`'s coverage) | Per-field display (temp/dewpoint/wind/visibility) rather than raw METAR text. |
| Marine tools | NOAA marine warnings + NOAA CO-OPS tide stations/buoys | Buoys and tide-forecast points are two distinct sub-layers. |
| Surface fronts/pressure centers | NOAA WPC surface analysis | Updated ~every 3 hours; standard frontal/pressure symbology (L/H, cold/warm/stationary/occluded front, trough, outflow boundary, squall line, dry line). |
| Local Storm Reports | NWS Local Storm Reports (official) | Category families: Severe (tornado/wind/hail), Tropical, Flood, Other (waterspout). A historical archive-search mode (date range, not just live) is a distinct capability worth scoping separately from live display. |
| mPING crowdsourced reports | NOAA/NSSL mPING ("Meteorological Phenomena Identification Near the Ground") | Free, public, crowdsourced ground-truth reports — a different (and much higher-volume/lower-severity) source from official LSRs; complements rather than replaces them. |
| Canadian severe weather (alerts + outlooks) | Environment and Climate Change Canada (ECCC) | Alerts parallel to NWS warnings/watches; severe thunderstorm outlooks parallel to SPC's, but region-based (BC/Yukon, NWT, Prairies, Ontario, Quebec, Atlantic) rather than polygon-based. Already decided in scope alongside US coverage — see `docs/ROADMAP.md` §9. |
| Tropical cyclone tracking (active systems, wind-probability fields, spaghetti model tracks, recon aircraft flight tracking) | NHC advisories/cones/wind-radii, ATCF-format multi-model tracks, recon aircraft position feeds | Large, self-contained data domain, no existing NEXRAD-radar precedent in `wxdata`. Confirmed low-priority backlog item, not a scoped phase — see `docs/ROADMAP.md` §8. |
| Personal weather station (PWS) integration | e.g. a vendor API (WeatherFlow Tempest, Ambient Weather) or a generic Weather-Underground-style PWS upload format | New idea: let a user register their own IoT weather station as a live station marker on the map, distinct from both METARs and static Custom Locations. No source chosen yet — needs a protocol/vendor decision if ever scoped. |

### Restricted or account-gated sources (need a data-source decision before scoping, not free/open like the rest of this catalog)

| Layer | Why it's flagged | Notes |
|---|---|---|
| Real-time lightning detection | Quality feeds (Vaisala, Earth Networks) are commercial/paid; community networks (e.g. Blitzortung) carry redistribution restrictions | Tracked as backlog per user decision 2026-09-25 — pick a source (or drop) when actually scoped, don't assume one now. |
| Power outage overlay | Utility-level outage data is typically sourced from a commercial aggregator, not free bulk NOAA-style access | Same backlog treatment as lightning. |
| Spotter Network overlay | Public site (spotternetwork.org), but viewing other spotters' live positions requires an authenticated member account in at least one competing app — redistribution terms for a third-party app are unconfirmed | Same backlog treatment; needs a ToS check before committing to build. |

## Base map (vector tiles for the map surface itself, not a weather data layer)

| Layer | Source | Access pattern | Notes |
|---|---|---|---|
| Base map (light + dark) | [OpenFreeMap](https://openfreemap.org) | `https://tiles.openfreemap.org/styles/{dark,positron,liberty,bright}` style JSON, tiles from `https://tiles.openfreemap.org/planet` | Free, unlimited, no API key/signup, MIT-licensed project. Chosen over MapLibre's own `demotiles.maplibre.org` (Phase 1 slice 1's original placeholder - too sparse/low-detail for real use, explicitly a demo/test dataset) and over a self-hosted Protomaps PMTiles extract (would need bundling a large regional file plus a custom `pmtiles://` protocol handler in MapLibre Native - real engineering cost not justified this early). Appearance offers `Same as app` (default), `Dark`, and `Light`; `app/qml/Panes/PaneHost.qml` resolves that setting to `dark` or `positron`. MapLibre reloads the style and the existing `onStyleLoaded` path reattaches WxLens's custom radar layer. Attribution required by OSM's ODbL and the OpenMapTiles schema ("© OpenStreetMap contributors © OpenMapTiles") - added by hand in `PaneHost.qml` since MapLibre Native Qt's Quick item has no built-in attribution control. |
| Base map, user-provided key (future) | e.g. MapTiler, Stadia Maps, or any MapLibre-compatible style URL | User-supplied style URL/API key in Settings | Not built yet - belongs to Phase 1's "map provider choice" settings item (§7). Framed as an opt-in upgrade for users who want more detail/a different look, not a requirement - OpenFreeMap must keep working with zero configuration. |
| Radar site metadata (lat/lon/elevation/time zone) | Bundled `app/res/config/radar_sites.json`, copied unmodified from Supercell Wx's `scwx-qt/res/config/radar_sites.json` (MIT, compiled from public NOAA/NWS sources) | `wxlens::data::FindRadarSite` (`app/source/wxlens/data/radar_site_database.hpp`), Phase 1 slice 2 | Static/offline, no network access needed. Also the intended lookup for §4.7's beam-height site lat/lon/elevation - reuse this then, don't add a second site database. |

## Not yet integrated / no provider exists anywhere in the codebase
Confirmed by inspection during Phase 0: no satellite, sounding, model, or mosaic provider exists in
`wxdata` today. All Phase 2/3 provider work listed above is genuinely new, not a port of anything.
