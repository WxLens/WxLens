---
type: "query"
date: "2026-09-01T12:03:38.826621+00:00"
question: "Why is WxLens velocity less vivid than RadarOmega and how should palette buttons be categorized without mixing radar fields?"
contributor: "graphify"
outcome: "useful"
source_nodes: ["PaletteManager", "PaneController", "DefaultPalette()", "ColorTableLut", "Product-browser wheel isolation"]
---

# Q: Why is WxLens velocity less vivid than RadarOmega and how should palette buttons be categorized without mixing radar fields?

## Answer

Expanded from the query via graph vocabulary: [palette, product, browser, velocity, reflectivity, default, color, table, lut, active, override, available, names]. WxLens was offering every factory palette for every product. The screenshot had SRV selected on raw Level 2 Velocity; that only recolored base velocity and did not compute RadarOmega-style storm-relative velocity. Implemented product-family compatibility in PaneController, rejected incompatible local and synchronized overrides, exposed only compatible alternatives to QML, changed the overflowing Row to a contained Flow, labeled the default palette, and packaged an application-owned higher-contrast DV red/green ramp. Release build, QML AOT, and three targeted pane palette tests pass.

## Outcome

- Signal: useful

## Source Nodes

- PaletteManager
- PaneController
- DefaultPalette()
- ColorTableLut
- Product-browser wheel isolation