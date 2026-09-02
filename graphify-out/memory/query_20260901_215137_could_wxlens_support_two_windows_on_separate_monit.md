---
type: "query"
date: "2026-09-01T21:51:37.879217+00:00"
question: "Could WxLens support two windows on separate monitors, with a 2x2 layout on one and 1x1 on the other, all panes linked, and how hard would that be?"
contributor: "graphify"
outcome: "useful"
source_nodes: ["PaneGridModel", "PaneController", "4.1 Per-channel synchronization model (not a single link boolean)"]
---

# Q: Could WxLens support two windows on separate monitors, with a 2x2 layout on one and 1x1 on the other, all panes linked, and how hard would that be?

## Answer

Expanded from original query via graph vocab: [pane, panes, layout, view, sync, synchronization, channel, channels, screen, location, zoom, bearing, product, palette]. Yes. The per-channel SyncGroupId design already represents linking panes across location, zoom, bearing, product, palette, and other channels. The current limitation is ownership: PaneGridModel owns both one window layout and sync propagation, and main.cpp creates only one PaneGridModel. Supporting independent 2x2 and 1x1 windows cleanly would introduce an application-level workspace or sync coordinator shared by multiple per-window PaneGridModels. Qt Quick supports multiple Window objects, while MapLibre rendering and window lifecycle require careful integration testing. Estimate: moderate feature, roughly several focused development slices; a basic same-process prototype may take days, while polished persistence, docking/window restore, UI, and cross-monitor testing likely take 2-4 weeks.

## Outcome

- Signal: useful

## Source Nodes

- PaneGridModel
- PaneController
- 4.1 Per-channel synchronization model (not a single link boolean)