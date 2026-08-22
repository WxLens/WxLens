# ADR 0001: Qt 6 / Qt Quick (QML) over a web-shell or Flutter UI

## Status
Accepted (2026-08-21)

## Context
Nimbus is a full, from-scratch rewrite of Supercell Wx's UI/app architecture (the one piece
carried forward unchanged is `wxdata`). Three UI stacks were considered: Qt 6/QML, a web shell
(Tauri/Electron + MapLibre GL JS), and Flutter. See `docs/ROADMAP.md` §1 for the full writeup;
this ADR records the decision itself for fast lookup.

## Decision
Qt 6 (C++20) with Qt Quick/QML for the UI, `wxdata` linked directly (no FFI boundary), and
MapLibre Native (Qt bindings) hosting a ported version of the existing custom-OpenGL radar
renderer as MapLibre custom layers.

## Rationale
- **Modest-laptop constraint:** Qt Quick is natively GPU-composited (Qt RHI) with no embedded
  browser engine; Electron/Tauri carry a Chromium or equivalent baseline cost a GL-heavy radar
  renderer shouldn't pay.
- **Renderer reuse:** `scwx-qt/source/scwx/qt/gl/draw/`'s custom-OpenGL draw-item system ports to
  a Qt RHI custom render node behind a `QQuickItem` — same GL algorithms and shaders, new plumbing
  only. A web shell would mean re-deriving this renderer in WebGL from zero.
- **`wxdata` reuse:** direct C++ linkage. A web shell needs a Rust/WASM bridge; Flutter needs Dart
  FFI over the C++ ABI.
- **Licensing continuity:** Qt LGPL-3.0-dynamic-link-only is already a validated setup in the
  current repo; a web shell means auditing an entirely new Node/Electron-or-Tauri dependency tree.
- **Real theming (the actual gap-closer):** QML's declarative, CSS-adjacent styling with zero
  native-widget leakage is what makes a "drastically modern" look achievable — the current app's
  `QPalette`/`QStyle` theming cannot deliver this, and that gap is the technical justification for
  doing a rewrite at all rather than re-skinning.
- **Mobile stretch goal:** Qt has a maintained mobile deployment story sharing the same QML
  codebase, unlike Electron (no mobile story) or a from-scratch Flutter rewrite.

## Consequences
- All application state/business logic lives in C++, exposed to QML via `Q_PROPERTY`/
  `Q_INVOKABLE`/signals; QML is presentation-only (see `AGENTS.md`).
- MapLibre Native Qt's QML bindings (`src/location/plugins/`) are licensed
  `LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`; Nimbus takes the LGPL-3.0-only option and links
  the library dynamically only, consistent with how Qt itself is already used in the ecosystem this
  project came from. See ADR 0004 for the concrete verification of this library's QML integration.
- This is a recommendation, not an unconditional lock-in — revisit if a hard web/browser
  deployment requirement emerges later (see `docs/ROADMAP.md` §1).
