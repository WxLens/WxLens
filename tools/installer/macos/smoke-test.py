#!/usr/bin/env python3
"""Launch the shipped bundle on a native Mac and retain startup diagnostics."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


def launch(app, output, seconds):
    # install-qt-action exports paths that can mask missing packaged plugins/QML.
    env = {
        key: value for key, value in os.environ.items()
        if not key.startswith(("QT_", "QML", "QSG_", "DYLD_"))
    }
    env.update(PATH="/usr/bin:/bin:/usr/sbin:/sbin", QSG_INFO="1",
               QT_DEBUG_PLUGINS="1")
    with (output / "startup.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen(
            [str(app / "Contents/MacOS/WxLens")], cwd=app.parent,
            env=env, stdout=log, stderr=subprocess.STDOUT,
        )
        try:
            try:
                code = process.wait(timeout=seconds)
            except subprocess.TimeoutExpired:
                print(f"Packaged WxLens stayed running for {seconds} seconds.")
                return
            raise RuntimeError(f"Packaged WxLens exited during startup: {code}")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dmg", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=30)
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("This check requires a native macOS GUI session")
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    started = time.time()
    try:
        with tempfile.TemporaryDirectory(prefix="wxlens-startup-") as directory:
            stage = Path(directory)
            mount = stage / "volume"
            mount.mkdir()
            subprocess.run(["hdiutil", "attach", str(args.dmg.resolve()),
                            "-readonly", "-nobrowse", "-mountpoint", str(mount)],
                           check=True)
            try:
                # Match dragging the app to a writable location before opening it.
                app = stage / "WxLens.app"
                subprocess.run(["ditto", str(mount / "WxLens.app"), str(app)],
                               check=True)
            finally:
                subprocess.run(["hdiutil", "detach", str(mount)], check=True)
            subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)],
                           check=True)
            launch(app, output, args.seconds)
    finally:
        # Preserve app logs as well as stderr; Qt messages may go to the file sink.
        library = Path.home() / "Library"
        roots = [library / "Logs/DiagnosticReports",
                 library / "Application Support/WxLens"]
        for index, root in enumerate(roots):
            if not root.exists():
                continue
            for source in root.rglob("*"):
                if (source.is_file() and source.stat().st_mtime >= started
                        and (index != 0 or "wxlens" in source.name.lower())):
                    target = output / str(index) / source.relative_to(root)
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, target)
        log = output / "startup.log"
        if log.exists():
            print(log.read_text(encoding="utf-8", errors="replace")[-24000:])


if __name__ == "__main__":
    main()
