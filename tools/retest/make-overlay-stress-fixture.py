#!/usr/bin/env python3
"""Create a deterministic synthetic placefile for camera-cost comparisons.

These shapes are test geometry, not weather alerts. The default has 200 closed
polygons with 33 coordinates each, centered around KEAX.
"""
import argparse
import math
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
args = parser.parse_args()
lines = ["Title: SYNTHETIC performance fixture - not weather alerts",
         "Threshold: 999", "Color: 255 160 40"]
for row in range(10):
    for col in range(20):
        latitude = 36.5 + row * .4
        longitude = -98.0 + col * .25
        points = [f"{latitude + .15 * math.sin(i * math.tau / 32):.6f}, "
                  f"{longitude + .1 * math.cos(i * math.tau / 32):.6f}"
                  for i in range(32)]
        lines.extend(["Polygon:", *points, points[0], "End:"])
args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
print("Wrote 200 synthetic polygons / 6,600 coordinates")
