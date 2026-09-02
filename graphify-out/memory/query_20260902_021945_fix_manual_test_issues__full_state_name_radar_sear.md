---
type: "query"
date: "2026-09-02T02:19:45.350824+00:00"
question: "Fix manual-test issues: full state-name radar search, site-change map recentering, and visible pane-local palette changes"
contributor: "graphify"
outcome: "useful"
source_nodes: ["PaneController", "PaletteManager", "PaneGridModel", "SyncChannel"]
---

# Q: Fix manual-test issues: full state-name radar search, site-change map recentering, and visible pane-local palette changes

## Answer

Expanded from original query via graph vocab: [panecontroller, palettemanager, palettechanged, syncchannel, centermaponsitechange, setcenter, radarsite, findradarsite, search, state, panegridmodel, productidentity]. Added full US state and country aliases to radar-site search data; added PaneController::centerOn so picker-originated camera commands update MapLibre and propagate Location/Zoom channels; exposed SRV as a compatible alternate velocity palette and clarified when no alternate exists. All 132 wxlens-app-test cases pass.

## Outcome

- Signal: useful

## Source Nodes

- PaneController
- PaletteManager
- PaneGridModel
- SyncChannel