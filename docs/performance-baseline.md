# WxLens performance baseline

This record is intentionally incomplete until the packaged visual acceptance run is performed.
Numbers must be measured, never inferred from a successful build or automated fixture timing.

## Required capture procedure

1. Use the packaged Release executable at the display's native resolution and record CPU, GPU,
   RAM, GPU/driver, OS, power mode, display resolution and refresh rate.
2. Run 1x1, 2x2 and 3x3 layouts for at least 60 seconds each. Capture Level 2 reflectivity and
   velocity, Level 3 radial, Level 3 raster, storm overlay and graphic/tabular text selections.
3. Record median, p95 and worst frame time plus visible stalls; process CPU, GPU utilization and
   peak working set; decode latency; request count/latency; cache hits/misses; and a repeated live
   update. State the profiler/counter used for each number.
4. Save the exact site, AWIPS IDs, archive timestamps or fixture names. Do not compare runs that
   used different sources without saying so.

## 2026-08-29 status

Automated fixture coverage exists for the Level 3 radial, raster, storm-overlay and text
translation families. A Release build completed and 123 of 124 model tests passed before a
Palette-channel switch fallthrough was corrected; the corrected suite is rerun as part of this
slice. No packaged frame/CPU/GPU/network baseline has been captured yet, so the Phase 1
performance gate remains open.

## 2026-08-30 preliminary packaged run

This is a partial 1x1 baseline, not the Phase 1 performance baseline. It establishes that the
packaged Release executable can sustain a live Level 2 view on the current development machine,
but it does not include frame-time, GPU, network/cache, repeated-update, 2x2 or 3x3 evidence.

- Build: `8814ec3` plus the working-tree QML changes present on 2026-08-30.
- Executable: `build-release-vs2026/Release/bin/wxlens-app.exe`, produced by the normal
  `wxlens-app` Release target and its `windeployqt` post-build deployment.
- Machine: Windows 11 Pro build 26200; Intel Core Ultra 9 185H (16 cores/22 logical processors);
  32 GB RAM; NVIDIA GeForce RTX 4050 Laptop GPU, driver 32.0.15.8205; Intel Arc Graphics,
  driver 32.0.101.8331.
- Display: 2048x1280 native capture. Refresh rate and power mode were not captured.
- Scenario: maximized 1x1 pane, live KEAX Level 2 reflectivity, 60 seconds after the initial
  volume and basemap were visibly rendered.
- Counter: one-second `Get-Process` CPU-time and memory samples. CPU is normalized across all 22
  logical processors. This is process CPU, not frame time.
- Result: median CPU 0.00%, p95 CPU 1.58%, peak CPU 6.55%, peak working set 691.8 MB, peak private
  memory 500.8 MB. The zero median means the process was mostly idle between updates; it must not
  be read as a rendering FPS measurement.
- Visual result: the packaged app remained responsive and displayed the live radar sweep over a
  detailed basemap. No crash, blank map or stranded custom-layer repaint was observed.

The required 2x2/3x3 product-family matrix, frame-time capture, GPU counters, decode latency,
request/cache instrumentation and repeated live update remain open. See
`docs/phase1-acceptance-2026-08-30.md` for the matching validation record.
