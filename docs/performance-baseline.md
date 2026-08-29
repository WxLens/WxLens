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
