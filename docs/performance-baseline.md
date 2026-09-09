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

## 2026-09-09 capture tooling and background-work constraint

The app now supports opt-in `WXLENS_FRAME_TIMINGS=<new CSV path>`. It records
Qt `beforeRendering`/`afterRendering` wall time and `frameSwapped` intervals using
a monotonic clock. Samples are buffered (20,000 maximum between flushes); the
GUI thread writes them once a second. Existing output files are never overwritten.
The application log records the UTC origin; preserve that log alongside the CSV.
No capture callbacks or timer are installed when the variable is unset.

These are **render-thread wall times, not GPU execution or actual display
presentation times**. Swap intervals include idle gaps and must only be compared
inside a verified active-gesture window. GUI-side projection/Canvas work can
increase swap gaps without increasing the render callback duration. Capture itself
adds some overhead, so use identical instrumentation for before/after comparisons.

The data service also logs listing time, combined download/decode duration, object
key and decoded-cache hit status. Level 2 geometry preparation and sweep-buffer
upload submission have separate durations. `wxdata::LoadObjectByKey` combines the
network read and parser call internally: the combined metric must not be described
as either network-only or decode-only timing. Upload submission does not insert a
GPU fence and is not GPU completion time. These logs count provider load calls,
not HTTP requests/retries or transferred bytes.

Reproducible helpers:

- `tools/retest/capture-performance.ps1`: a named 5–60 second idle or
  press-drag-release scenario with normalized process CPU and memory samples.
  Requires a visible, unobstructed app; aborts on lost focus or displaced cursor.
- `tools/retest/summarize-performance.py`: selects complete swap intervals inside
  the scenario's time bounds. Rejects invalid runs, dropped samples and write
  errors; `--verified-gesture` requires the operator to first verify actual map
  movement with no dialog open. Uses nearest-rank p95.
- `tools/retest/make-overlay-stress-fixture.py`: produces 200 synthetic closed
  placefile polygons / 6,600 coordinates near KEAX. These are stress-test shapes,
  **not weather alerts or a measured typical warning workload**. Import the file,
  compare visibility on/off with the same camera and data, and remove it afterward.

The initial attempted interactive capture is **invalid**: its post-run screenshot
showed an open overlays dialog. Subsequent attempts aborted on displaced cursor.
No numbers from those attempts qualify as a camera-performance baseline. Live
warning retrieval also failed and the overlay panel showed zero warning polygons.

The project owner then requested background execution. Desktop input automation
was stopped and the agent's test window closed. Builds, native CI and nonvisual
checks may continue; the visible 1x1/2x2/3x3 comparisons remain pending. No overlay
optimization, cache change or playback implementation is claimed by this slice.

### Background data-path observations

Release build based on `4fa1eab` plus this instrumentation, Windows 11 on the
Core Ultra 9 185H / 32 GB development machine. The app's window was hidden;
these numbers say nothing about rendering responsiveness or the modest-laptop
acceptance target. Site KEAX, Level 2 reflectivity, actual elevation 0.483395°,
2,438,664 generated vertices. The same object was selected on both requests:
`2026/09/09/KEAX/KEAX20260909_171919_V06`.

| Local log time (2026-09-09) | Listing ms | Download + decode ms | Geometry ms |
| --- | ---: | ---: | ---: |
| 13:26:24, initial load | 1,295.014 | 6,586.325 | 378.586 |
| 13:27:20, periodic repeat of the same key | 567.063 | 3,577.887 | 354.570 |

The first request overlapped the local model-test run; the repeat occurred after
those tests completed. Both report `decoded_cache_hit=false`. The geometry log
ran on the same thread as application initialization (GUI thread 14756); it
followed the background data-service completion (thread 41660). This demonstrates
repeated loading/geometry work for an unchanged volume and substantial synchronous
GUI-thread geometry work. It does not isolate network transfer from decoding or
establish how much of a particular user's camera lag either accounts for.

Verification: Release app/model-test builds passed; 104 relevant radar/Level 3/
crash-report tests passed and four network-dependent cases were skipped. The CSV
recorder produced real frame rows, refused to overwrite an existing capture, and
the summary tool passed known-value/time-bound/invalid-run checks. Primary camera
gesture performance remains unverified.
