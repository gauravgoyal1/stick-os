#!/usr/bin/env python3
"""Install a scripted app on the Stick OS device via USB serial.

Reads the app directory (must contain manifest.json + entry .py file),
creates /apps/<id>/ on the device's LittleFS, pushes each file via the
FILE_PUT protocol, and reboots so scanInstalledApps() picks it up.

Usage:
  tools/push_app.py --port /dev/cu.usbserial-XXXX --app apps/snake
"""
import argparse
import json
import pathlib
import subprocess
import sys


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", required=True)
    p.add_argument("--app", required=True, help="local app directory (apps/<id>/)")
    p.add_argument("--no-reboot", action="store_true")
    args = p.parse_args()

    app_dir = pathlib.Path(args.app)
    manifest_path = app_dir / "manifest.json"
    if not manifest_path.exists():
        print(f"ERR: {manifest_path} missing", file=sys.stderr)
        return 1
    manifest = json.loads(manifest_path.read_text())
    app_id = manifest.get("id")
    entry = manifest.get("entry", "main.py")
    if not app_id:
        print("ERR: manifest.json has no id", file=sys.stderr)
        return 1

    remote_dir = f"/apps/{app_id}"
    files = [
        (manifest_path, f"{remote_dir}/manifest.json"),
        (app_dir / entry, f"{remote_dir}/{entry}"),
    ]
    for local, remote in files:
        if not local.exists():
            print(f"ERR: {local} missing", file=sys.stderr)
            return 1

    here = pathlib.Path(__file__).parent
    push_file = here / "push_file.py"
    for local, remote in files:
        print(f"-> {remote}")
        r = subprocess.run(
            [sys.executable, str(push_file),
             "--port", args.port,
             "--src", str(local),
             "--dest", remote],
            check=False,
        )
        if r.returncode != 0:
            print(f"ERR: push_file failed for {local}", file=sys.stderr)
            return r.returncode

    print(f"Installed {app_id} to {remote_dir}. Reboot to activate.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
