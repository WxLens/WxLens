---
type: "query"
date: "2026-09-01T02:59:38.901335+00:00"
question: "Fix the Weather Overlays contrast problem."
contributor: "graphify"
outcome: "useful"
source_nodes: ["Weather Overlays contrast and control styling", "Theme", "control", "textSecondary"]
---

# Q: Fix the Weather Overlays contrast problem.

## Answer

Expanded from original query via vocab: [weather, overlays, contrast, control, styling, theme, text, status]. The acceptance blocker was localized to app/qml/Dialogs/OverlaysDialog.qml, where platform-default Button, CheckBox, and TextField styling conflicted with themed dialog surfaces. Replaced them with dialog-local theme-controlled components covering foreground, background, border, hover, pressed, disabled, and keyboard-focus states; promoted status text to textSecondary; clarified and labeled the web-address action. QML lint and AOT compilation passed; final linking was prevented only by the running wxlens-app.exe locking its output file.

## Outcome

- Signal: useful

## Source Nodes

- Weather Overlays contrast and control styling
- Theme
- control
- textSecondary