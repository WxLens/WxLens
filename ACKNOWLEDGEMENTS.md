Acknowledgements
================

Nimbus reuses `wxdata` and its dependency/attribution discipline from
[Supercell Wx](https://github.com/dpaulat/supercell-wx) by Dan Paulat (see
`docs/adr/0002-wxdata-reuse-strategy.md`). This table starts from Supercell Wx's own
`ACKNOWLEDGEMENTS.md` and will keep growing as later phases add dependencies — see that project's
file for the full list this one will eventually approach.

Dependencies
------------

Nimbus uses code from the following dependencies (as of Phase 0 — empty app shell + `wxdata` +
map renderer):

| Dependency | License | Notes |
| ---------- | ------- | ----- |
| [{fmt}](https://fmt.dev/) | [MIT License](https://spdx.org/licenses/MIT.html) |
| [AWS SDK for C++](https://aws.amazon.com/sdk-for-cpp/) | [Apache License 2.0](https://spdx.org/licenses/Apache-2.0.html) |
| [Boost](https://www.boost.org/) | [Boost Software License 1.0](https://spdx.org/licenses/BSL-1.0.html) |
| [bzip2](https://sourceware.org/bzip2/) | [bzip2 and libbzip2 License v1.0.6](https://spdx.org/licenses/bzip2-1.0.6.html) |
| [cmake-conan](https://github.com/conan-io/cmake-conan) | [MIT License](https://spdx.org/licenses/MIT.html) |
| [cpr](https://github.com/libcpr/cpr) | [MIT License](https://spdx.org/licenses/MIT.html) |
| [Date](https://github.com/HowardHinnant/date) | [MIT License](https://spdx.org/licenses/MIT.html) |
| [GeographicLib](https://geographiclib.sourceforge.io/) | [MIT License](https://spdx.org/licenses/MIT.html) | WGS84 geodesic direct problem, radar radial/gate geo-positioning (`nimbus::util::GeodesicDirect`) |
| [glm](https://github.com/g-truc/glm) | [MIT License](https://spdx.org/licenses/MIT.html) | MVP-matrix construction shared by every MapLibre custom layer |
| [GoogleTest](https://google.github.io/googletest/) | [BSD 3-Clause "New" or "Revised" License](https://spdx.org/licenses/BSD-3-Clause.html) |
| [HSLuv](https://www.hsluv.org/) | [MIT License](https://spdx.org/licenses/MIT.html) |
| [libxml2](http://xmlsoft.org/) | [MIT License](https://spdx.org/licenses/MIT.html) |
| [libzip](https://libzip.org/) | [BSD 3-Clause "New" or "Revised" License](https://spdx.org/licenses/BSD-3-Clause.html) |
| [MapLibre Native](https://maplibre.org/projects/maplibre-native/) | [BSD 2-Clause "Simplified" License](https://spdx.org/licenses/BSD-2-Clause.html) |
| [MapLibre Native Qt](https://github.com/maplibre/maplibre-native-qt) | [BSD 2-Clause License](https://spdx.org/licenses/BSD-2-Clause.html) (Quick module used; see `docs/adr/0004-maplibre-qml-integration.md` for why the LGPL/GPL Location module is not used) |
| [OpenSSL](https://www.openssl.org/) | [OpenSSL License](https://spdx.org/licenses/OpenSSL.html) |
| [Qt](https://www.qt.io/) | [GNU Lesser General Public License v3.0 only](https://spdx.org/licenses/LGPL-3.0-only.html) | Qt Core, Qt GUI, Qt Quick, Qt Quick Controls 2<br/>Additional Licenses: https://doc.qt.io/qt-6/licenses-used-in-qt.html |
| [range-v3](https://github.com/ericniebler/range-v3) | [Boost Software License 1.0](https://spdx.org/licenses/BSL-1.0.html)<br/>[MIT License](https://spdx.org/licenses/MIT.html) |
| [re2](https://github.com/google/re2) | [BSD 3-Clause "New" or "Revised" License](https://spdx.org/licenses/BSD-3-Clause.html) |
| [spdlog](https://github.com/gabime/spdlog) | [MIT License](https://spdx.org/licenses/MIT.html) |
| [Units](https://github.com/nholthaus/units) | [MIT License](https://spdx.org/licenses/MIT.html) |
| [zlib](https://zlib.net/) | [zlib License](https://spdx.org/licenses/Zlib.html) |
| [Supercell Wx `wxdata`](https://github.com/dpaulat/supercell-wx) | [MIT License](https://spdx.org/licenses/MIT.html) | NEXRAD Level 2/3 parsing, `.pal` color tables, AWIPS text products, network providers — reused unmodified, see ADR 0002 |

Source
------

Nimbus derives code from the following sources (carried forward via `wxdata`):

| Source | License |
| ------ | ------- |
| [Color Table File Specification](http://www.grlevelx.com/manuals/color_tables/files_color_table.htm) | Used with permission |
| [Place File Specification](https://www.grlevelx.com/manuals/gis/files_places.htm) | Used with permission |

Map data
--------

Nimbus's default base map (`app/qml/Panes/PaneHost.qml`, see `docs/data-sources.md`) is served by
[OpenFreeMap](https://openfreemap.org), free/no-API-key vector tile hosting of OpenStreetMap data:

| Source | License | Notes |
| ------ | ------- | ----- |
| [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors | [Open Database License (ODbL) 1.0](https://opendatacommons.org/licenses/odbl/1-0/) | Underlying map data. Attribution shown in-app (`PaneHost.qml`). |
| [OpenMapTiles](https://openmaptiles.org/) | [BSD 3-Clause](https://github.com/openmaptiles/openmaptiles/blob/master/LICENSE.md) (schema/tools) | Vector tile schema OpenFreeMap's tiles follow. Attribution shown in-app. |

Assets
------

Nimbus bundles the following assets:

| Source | License | Notes |
| ------ | ------- | ----- |
| [NOAA's Weather and Climate Toolkit](https://www.ncdc.noaa.gov/wct/) | Public Domain | Default Color Tables - the complete `scwx-qt/res/palettes/wct/` set is bundled unmodified as Qt resources; the active product subset grows as products are wired up |
| [Supercell Wx](https://github.com/dpaulat/supercell-wx) `scwx-qt/res/config/radar_sites.json` | [MIT License](https://spdx.org/licenses/MIT.html) | Radar site location/elevation/time-zone data, compiled from public NOAA/NWS sources — copied unmodified into `app/res/config/`, used by `nimbus::data::FindRadarSite` |
