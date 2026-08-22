# Data source catalog

Living catalog of external data sources Nimbus integrates or plans to integrate, by phase. This
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

## Phase 3 — additional data layers

| Layer | Primary source | Access pattern | Notes |
|---|---|---|---|
| Satellite imagery | NOAA GOES-16/18/19 | `noaa-goes16`/`-goes18`/`-goes19` S3 buckets (free) — ABI L1b or L2 CMIP/MCMIP, NetCDF4 | Needs a NetCDF4 decoder + geostationary reprojection. |
| Satellite bootstrap | SSEC RealEarth or NOAA nowCOAST WMS/ArcGIS image services | Pre-rendered PNG/WMS tiles | Faster first cut. |
| Soundings | University of Wyoming upper-air archive | Plain HTML/text tables, no auth | Simplest to parse. |
| Soundings alt | `rucsoundings.noaa.gov` (RAOB text) | Plain text, no auth | Real-time-oriented complement. |
| Jet stream / pressure | NOAA NOMADS (GFS/RAP/HRRR) | GRIB2, free, unauthenticated HTTP | 250mb wind = jet stream, MSLP = surface pressure. |
| Overlay bootstrap | NOAA nowCOAST pre-rendered tiles | WMS/REST | Same bootstrap-first pattern. |
| Terrain/DEM (feeds beam-center AGL, §4.7) | Mapzen/Terrarium elevation tiles | `elevation-tiles-prod` S3 bucket (free) — PNG-encoded elevation raster tiles | Not needed until AGL display is implemented. |

**GRIB2/NetCDF4 decoder** (needed for MRMS, GOES, NOMADS): evaluate **eccodes** (Apache-2.0) vs.
**NCEPLIBS-g2c** (permissive) for GRIB2, and **netcdf-c** (MIT-style) for NetCDF4, at Phase 2/3
kickoff — license + binary-footprint review first, per `docs/ROADMAP.md` §0/§9 Q7. Not evaluated
yet as of Phase 0.

## Base map (vector tiles for the map surface itself, not a weather data layer)

| Layer | Source | Access pattern | Notes |
|---|---|---|---|
| Base map (light + dark) | [OpenFreeMap](https://openfreemap.org) | `https://tiles.openfreemap.org/styles/{dark,positron,liberty,bright}` style JSON, tiles from `https://tiles.openfreemap.org/planet` | Free, unlimited, no API key/signup, MIT-licensed project. Chosen over MapLibre's own `demotiles.maplibre.org` (Phase 1 slice 1's original placeholder - too sparse/low-detail for real use, explicitly a demo/test dataset) and over a self-hosted Protomaps PMTiles extract (would need bundling a large regional file plus a custom `pmtiles://` protocol handler in MapLibre Native - real engineering cost not justified this early). `app/qml/Panes/PaneHost.qml` currently hardcodes `dark`; should switch on `ThemeManager`'s active theme once that exists (Phase 1 slice 10). Attribution required by OSM's ODbL and the OpenMapTiles schema ("© OpenStreetMap contributors © OpenMapTiles") - added by hand in `PaneHost.qml` since MapLibre Native Qt's Quick item has no built-in attribution control. |
| Base map, user-provided key (future) | e.g. MapTiler, Stadia Maps, or any MapLibre-compatible style URL | User-supplied style URL/API key in Settings | Not built yet - belongs to Phase 1's "map provider choice" settings item (§7). Framed as an opt-in upgrade for users who want more detail/a different look, not a requirement - OpenFreeMap must keep working with zero configuration. |

## Not yet integrated / no provider exists anywhere in the codebase
Confirmed by inspection during Phase 0: no satellite, sounding, model, or mosaic provider exists in
`wxdata` today. All Phase 2/3 provider work listed above is genuinely new, not a port of anything.
