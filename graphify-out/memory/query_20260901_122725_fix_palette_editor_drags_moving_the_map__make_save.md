---
type: "query"
date: "2026-09-01T12:27:25.077514+00:00"
question: "Fix palette editor drags moving the map, make saved velocity use the new palette, and soften low-reflectivity blue noise."
contributor: "graphify"
outcome: "useful"
source_nodes: ["PaneController", "PaletteManager", "ColorTableLut"]
---

# Q: Fix palette editor drags moving the map, make saved velocity use the new palette, and soften low-reflectivity blue noise.

## Answer

PaletteDialog and ColorPicker drag MouseAreas lacked preventStealing, allowing MapLibre behind the modal editor to seize the pointer grab. Added preventStealing to the modal shields, RGB sliders, and stop-value handles. PaneController now clears restored product-incompatible overrides during product binding, so a stale SRV override cannot prevent Velocity from using DV. Packaged local DR and DV app-owned palettes; DV is higher contrast and DR maps -20 through +5 dBZ to subdued neutral gray/gray-blue before the familiar blue/green/yellow/red ramp. QML lint and AOT compilation pass; four palette isolation/migration tests pass; Release build launched.

## Outcome

- Signal: useful

## Source Nodes

- PaneController
- PaletteManager
- ColorTableLut