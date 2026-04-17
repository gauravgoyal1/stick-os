import os
import re
from pathlib import Path

from fastapi import APIRouter, HTTPException
from fastapi.responses import FileResponse, JSONResponse

router = APIRouter()

STORAGE = Path(os.getenv("STORAGE_PATH", "./storage"))
TRANSCRIPTS_DIR = STORAGE / "transcripts"

# Filenames come from services/scribe.py: scribe_YYYYMMDD_HHMMSS.txt
_TS_PATTERN = re.compile(r"^scribe_(\d{8})_(\d{6})\.txt$")


def _parse_timestamp(name: str) -> str:
    m = _TS_PATTERN.match(name)
    if not m:
        return ""
    d, t = m.groups()
    return f"{d[:4]}-{d[4:6]}-{d[6:8]} {t[:2]}:{t[2:4]}:{t[4:6]}"


@router.get("/transcripts")
async def list_transcripts():
    if not TRANSCRIPTS_DIR.exists():
        return JSONResponse({"transcripts": []})
    items = []
    for f in sorted(TRANSCRIPTS_DIR.iterdir(), reverse=True):
        if not f.is_file() or not f.name.endswith(".txt"):
            continue
        items.append({
            "name": f.name,
            "timestamp": _parse_timestamp(f.name),
            "size": f.stat().st_size,
        })
    return JSONResponse({"transcripts": items})


@router.get("/transcripts/{name}")
async def get_transcript(name: str):
    # Defense-in-depth: no traversal, must be our own filename shape.
    if not _TS_PATTERN.match(name):
        raise HTTPException(status_code=404)
    path = TRANSCRIPTS_DIR / name
    if not path.is_file():
        raise HTTPException(status_code=404)
    return FileResponse(str(path), media_type="text/plain; charset=utf-8")
