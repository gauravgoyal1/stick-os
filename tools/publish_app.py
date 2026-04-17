#!/usr/bin/env python3
"""Publish scripted apps into the server's static storage tree.

Copies apps/<id>/ -> <storage>/apps/<id>/ and regenerates <storage>/catalog.json
with real sizes and sha256s for every published app.

Usage:
  tools/publish_app.py                      # publish every dir under apps/
  tools/publish_app.py snake tilt           # publish only the named apps
  tools/publish_app.py --storage /srv/stick-os/server/storage snake
  tools/publish_app.py --dry-run

Run this on the server host (or anywhere the storage path is reachable).
After a first-time publish into a previously empty tree, restart uvicorn
so StaticFiles mounts /apps (server/main.py mounts it only if the dir
exists at startup).
"""
import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_APPS = REPO / "apps"
DEFAULT_STORAGE = REPO / "server" / "storage"


def load_manifest(app_dir: Path) -> dict:
    m = json.loads((app_dir / "manifest.json").read_text())
    for field in ("id", "name", "version", "category"):
        if field not in m:
            raise ValueError(f"{app_dir}/manifest.json missing '{field}'")
    return m


def copy_app(src: Path, dst: Path, dry_run: bool) -> None:
    if dry_run:
        print(f"  would copy {src} -> {dst}")
        return
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def entry_for(app_dir: Path) -> dict:
    m = load_manifest(app_dir)
    files = []
    for f in sorted(app_dir.iterdir()):
        if not f.is_file():
            continue
        data = f.read_bytes()
        files.append({
            "name": f.name,
            "url": f"/apps/{m['id']}/{f.name}",
            "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
        })
    return {
        "id": m["id"],
        "name": m["name"],
        "version": m["version"],
        "category": m["category"],
        "description": m.get("description", ""),
        "files": files,
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("apps", nargs="*", help="app ids (dir names under --apps-dir); default: all")
    p.add_argument("--apps-dir", type=Path, default=DEFAULT_APPS)
    p.add_argument("--storage", type=Path, default=DEFAULT_STORAGE)
    p.add_argument("--dry-run", action="store_true")
    args = p.parse_args()

    if not args.apps_dir.is_dir():
        print(f"ERR: apps dir not found: {args.apps_dir}", file=sys.stderr)
        return 1

    selected = args.apps or sorted(
        d.name for d in args.apps_dir.iterdir()
        if d.is_dir() and (d / "manifest.json").exists()
    )
    if not selected:
        print("ERR: no apps to publish", file=sys.stderr)
        return 1

    dst_root = args.storage / "apps"
    if not args.dry_run:
        dst_root.mkdir(parents=True, exist_ok=True)

    for name in selected:
        src = args.apps_dir / name
        if not (src / "manifest.json").exists():
            print(f"ERR: {src}/manifest.json missing", file=sys.stderr)
            return 1
        print(f"publishing {name}")
        copy_app(src, dst_root / name, args.dry_run)

    catalog = {"version": 1, "apps": []}
    if not args.dry_run and dst_root.exists():
        for app_dir in sorted(dst_root.iterdir()):
            if app_dir.is_dir() and (app_dir / "manifest.json").exists():
                catalog["apps"].append(entry_for(app_dir))

    catalog_path = args.storage / "catalog.json"
    if args.dry_run:
        print(f"  would write {catalog_path} ({len(selected)} app(s) staged)")
    else:
        catalog_path.write_text(json.dumps(catalog, indent=2) + "\n")
        print(f"wrote {catalog_path} ({len(catalog['apps'])} app(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
