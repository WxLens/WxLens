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

## 2026-09-28 site-switch scheduling fix, verified

Verifies the three scheduling commits on `perf/slice1-service-lifecycle`
(`87b4dfb` `RadarTaskQueue`, `5aabf46` weak_ptr site lifecycle, `d521d93` four
io_context threads), which targeted the specific problem measured on 2026-09-26:
a site switch queued behind whatever other sites' background work was already
running, up to 59.5s worst-case, even though the actual network+decode for that
same request took only a few seconds.

**Method.** End-to-end site-switch time conflates two things: the real
network/decode work, and queueing/contention overhead waiting for a free worker.
Only the second is what this fix changed, and raw network conditions vary
session to session, so comparing raw end-to-end times before/after is confounded.
Instead, for every `Requesting latest Level 2 data for <site>` / `Level 2 load
metrics` pair in the log, `overhead = end_to_end - (listing_ms + download_decode_ms)`
isolates time spent waiting on scheduling from time spent actually transferring
and decoding data.

**Data.** Pre-fix: `wxlens.log.pre-slice123`, a snapshot taken 2026-09-26 just
before these commits (197 site-load pairs, the same session the commit messages'
"624s/96 loads/59.5s worst" figures came from). Post-fix: the live `wxlens.log`
that has been running continuously since the `d521d93` build (568 site-load
pairs across 2026-09-26 through 2026-09-28, real usage, not a synthetic burst).

| | pre-fix (n=197) | post-fix (n=568) |
| --- | ---: | ---: |
| overhead, median | 0.001s | 0.001s |
| overhead, p95 | 8.883s | 0.091s |
| overhead, worst | 47.619s | 1.433s |
| requests with >1s overhead | 7.1% | 0.2% |

Post-fix, a site switch pays almost exclusively for its own network/decode time;
before the fix, worst-case switches lost 16-47s purely to being queued behind
other sites' background history-warming.

**Anomaly found while measuring, out of scope for this branch:** the post-fix log
also has several KTLX requests where `download_decode_ms` itself runs 50-143s
(overhead near zero — the wait is real, not queueing). That is a genuine
slow-download/slow-source issue for that site, unrelated to the scheduling fix,
and is not investigated here.

**Also verified:** a non-interactive smoke launch (`tools/retest/ui-drive.ps1
smoke`) against the current build - clean startup, zero QML errors. Full suite
`wxlens-app-test`: 233 tests, 232 passed, 1 pre-existing skip (`MapCache.Child`),
0 failed.

**Not verified:** interactive click-driven site switching
(`tools/retest/ui-drive.ps1 click`/`type`) - the session was locked
(`GetForegroundWindow` returned the lock screen) when this check ran, so no
window could be focused and no automated click/keystroke could reach the app.
This is a capture-validity issue (see the DPI/foreground pitfalls recorded
elsewhere), not a result. A live click-through site-switch retest while the
desktop is unlocked would still be worth doing before calling this fully closed.

## 2026-11-26 parallel Level 2 LDM-record decompression

Measures `external/patches/0016-wxdata-parallel-ldm-decompress.patch`. In `Ar2vFile`, it
decompresses a volume's independent bzip2 LDM records in parallel instead of one at a time.
Parsing still runs afterwards, in file order. The patch was written because the field logs
showed `download_decode_ms` dominating site loads. `wxlens.3.log`, 158 loads from late September:
median 7,580 ms, p90 14,518 ms, against a median listing of 304 ms and geometry of 1,672 ms.

**Method.** `wxlens-level2-decode-bench` (Release, `test/source/wxlens/bench/`) reads each file
into memory, then times `Ar2vFile::LoadData`, which covers decompression, parsing and indexing.
No network or disk is involved. Files: the latest KEAX, KLSX and KTLX volumes from
`unidata-nexrad-level2` at about 19:45Z on 2026-11-26, plus the legacy repo's
`Level2_KLSX_20210527_1757.ar2v` fixture. The unpatched and patched executables were run
alternately for five rounds of five loads each. The table gives the median of the five
per-round medians.

The machine was the Core Ultra 9 185H (22 logical processors) on AC power, with the Balanced
power plan. Unrelated background processes used about 18% of total CPU, which made individual
rounds vary by up to 3x. The patched build was faster in every round.

| Volume | Compressed | LDM records | Before | After | Speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| KLSX20261126_194843_V06 | 13.6 MB | 73 | 2,444 ms | 829 ms | 2.9x |
| Level2_KLSX_20210527_1757 (fixture) | 14.9 MB | 94 | 2,498 ms | 921 ms | 2.7x |
| KEAX20261126_194455_V06 | 3.6 MB | 67 | 900 ms | 423 ms | 2.1x |
| KTLX20261126_194803_V06 | 2.5 MB | 55 | 695 ms | 428 ms | 1.6x |

Before the patch, a decompress-only pass accounted for about 60–85% of `LoadData`. The time
left after the patch is mostly sequential message parsing. That makes parsing the next
candidate, and it is the reason smaller volumes gain less.

**Correctness.** The bench fingerprints every radial: its identity, azimuth, time, and the raw
bytes of every moment block. It also fingerprints the `GetElevationScan` cut list. All five
files produced identical fingerprints before and after the patch.

Tests: `wxlens-wxdata-test` had 186 passed and 12 skipped. Its 2 failures were
`IemApiProviderTest`, a live third-party API that CI already excludes; they are unrelated. Every
`Ar2v`/`NexradFile`/Level 2 provider case passed. `wxlens-app-test` had 244 passed and 1
skipped (the existing skip).

The patched app passed `tools/retest/ui-drive.ps1 smoke` and loaded live KSRX data with
`download_decode_ms=919.565`. That is a single sample and is not compared with anything.

**Not measured.** Network transfer time was not isolated. The bench also did not cover
contention when several sites decode at once on the four radar `io_context` threads; each
decode now spawns up to `hardware_concurrency()` short-lived workers.
