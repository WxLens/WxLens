#!/usr/bin/env python3
"""Summarize frame capture within an explicitly bounded Windows scenario."""
import argparse
import csv
import json
import math
from pathlib import Path
import re
import statistics


def stats(values):
    values = sorted(values)
    if not values:
        return None
    return {"count": len(values), "median": statistics.median(values),
            "p95": values[math.ceil(len(values) * .95) - 1], "max": values[-1]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", type=Path)
    parser.add_argument("log", type=Path)
    parser.add_argument("scenario", type=Path)
    parser.add_argument("--verified-gesture", action="store_true",
                        help="Confirm visual inspection showed map movement with no open dialog")
    args = parser.parse_args()
    log = args.log.read_text(encoding="utf-8-sig")
    origins = re.findall(r"Frame capture started, UTC epoch ms (\d+)", log)
    if len(origins) != 1 or "Frame capture dropped" in log or "Frame capture write failed" in log:
        parser.error("Capture log must contain exactly one start and no dropped samples/write errors")
    scenario = json.loads(args.scenario.read_text(encoding="utf-8-sig"))
    if not scenario["valid"]:
        parser.error("Scenario did not complete successfully")
    if scenario["gesture"] != "idle" and not args.verified_gesture:
        parser.error("Visually verify map movement before passing --verified-gesture")
    start = scenario["start_utc_ms"] - int(origins[0])
    end = scenario["end_utc_ms"] - int(origins[0])
    with args.frames.open(encoding="utf-8") as stream:
        frames = [{key: float(value) for key, value in row.items()}
                  for row in csv.DictReader(stream)]
    selected = [f for f in frames if start <= f["elapsed_ms"] <= end]
    intervals = [f["swap_interval_ms"] for f in selected
                 if f["swap_interval_ms"] >= 0
                 and f["elapsed_ms"] - f["swap_interval_ms"] >= start]
    if not intervals and scenario["gesture"] != "idle":
        parser.error("No complete swap intervals in active-gesture window")
    samples = scenario["process_samples"]
    print(json.dumps({
        "scenario": scenario["scenario"], "gesture": scenario["gesture"],
        "duration_seconds": (end - start) / 1000,
        "render_thread_wall_ms": stats([f["render_wall_ms"] for f in selected
                                        if f["render_wall_ms"] >= 0]),
        "swap_interval_ms": stats(intervals),
        "swap_intervals_over_50ms": sum(value > 50 for value in intervals),
        "process_cpu_percent": stats([s["cpu_percent"] for s in samples]),
        "peak_working_set_mib": max(s["working_set_bytes"] for s in samples) / 1048576,
        "peak_private_mib": max(s["private_bytes"] for s in samples) / 1048576,
        "limitations": "Qt callback wall times, not GPU execution or display presentation. "
                        "Swap gaps include idle time; compare active gestures separately. "
                        "CPU is normalized over all logical processors; excludes capture tool."
    }, indent=2))


if __name__ == "__main__":
    main()
