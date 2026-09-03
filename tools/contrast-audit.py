"""WCAG 2.1 contrast audit for the theme-token pairs the dialogs actually use.

Run from the repo root: `python tools/contrast-audit.py`. Exits non-zero if any pair misses its
target, so it can gate a theme change in CI. Evidence for the Weather Overlays contrast item in
docs/phase1-ux-feedback-2026-08-31.md and the Phase 1 "first-run usability and polish" gate.

Targets: 4.5:1 for text (SC 1.4.3), 3:1 for focus rings and form-control outlines (SC 1.4.11).
Pairs with target 0 are informational: labelled buttons are identified by their text so their
border is not required to contrast, and inactive controls are exempt from 1.4.3.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent / "app" / "res" / "themes"


def parse(path):
    colors = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        m = re.match(r'\s*(\w+)\s*=\s*"#([0-9a-fA-F]{6,8})"', line)
        if m:
            colors[m.group(1)] = m.group(2)
    return colors


def rgb(hexstr):
    h = hexstr[-6:]
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def composite(fg_hex, bg_hex):
    """#AARRGGBB tokens (elevated_surface) are composited onto the surface they sit on."""
    if len(fg_hex) == 8:
        a = int(fg_hex[:2], 16) / 255.0
        f, b = rgb(fg_hex), rgb(bg_hex)
        return tuple(round(a * fc + (1 - a) * bc) for fc, bc in zip(f, b))
    return rgb(fg_hex)


def luminance(c):
    def channel(v):
        v /= 255.0
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4

    r, g, b = (channel(v) for v in c)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def ratio(a, b):
    la, lb = luminance(a), luminance(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


# (label, foreground token, background token, target ratio)
PAIRS = [
    ("Dialog title / body text on surface", "text_primary", "surface", 4.5),
    ("Helper and status text (textSecondary) on surface", "text_secondary", "surface", 4.5),
    ("Button label on control", "text_primary", "control", 4.5),
    ("Button label on controlHover", "text_primary", "control_hover", 4.5),
    ("Button label on controlActive", "text_primary", "control_active", 4.5),
    ("Checkbox label on surface", "text_primary", "surface", 4.5),
    ("Checkbox tick on control", "text_primary", "control", 4.5),
    ("Text field text on control", "text_primary", "control", 4.5),
    ("Placeholder (textSecondary) on control", "text_secondary", "control", 4.5),
    ("List row text on elevatedSurface", "text_secondary", "elevated_surface", 4.5),
    ("List error line (warning) on surface", "warning", "surface", 4.5),
    ("List error line (warning) on elevatedSurface", "warning", "elevated_surface", 4.5),
    ("Warning text (catalog errors) on elevatedSurface", "warning", "elevated_surface", 4.5),
    ("Focus ring (primary) on control", "primary", "control", 3.0),
    ("Focus ring (primary) on surface", "primary", "surface", 3.0),
    ("Checkbox / text-field outline (textMuted) on surface", "text_muted", "surface", 3.0),
    ("Checkbox / text-field outline (textMuted) on control", "text_muted", "control", 3.0),
    ("(info) Button border on surface", "border", "surface", 0.0),
    ("(info) Disabled label (textMuted) on surface", "text_muted", "surface", 0.0),
    ("(info) Danger text on surface - do not use for body text in dark theme", "danger", "surface", 0.0),
]


def main():
    failures = 0
    for theme_file in sorted(ROOT.glob("*.toml")):
        colors = parse(theme_file)
        print(f"\n== {theme_file.name} ==")
        for label, fg, bg, target in PAIRS:
            background = composite(colors[bg], colors["surface"])
            foreground = composite(colors[fg], colors[bg][-6:])
            r = ratio(foreground, background)
            status = "PASS" if r >= target else "FAIL"
            failures += r < target
            need = f"need {target}:1" if target else "informational"
            print(f"  {status}  {r:5.2f}:1 ({need})  {label}")
    print(f"\n{failures} failing pair(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
