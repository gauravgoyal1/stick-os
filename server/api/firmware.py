import os
from pathlib import Path

from fastapi import APIRouter
from fastapi.responses import FileResponse, JSONResponse

router = APIRouter()

STORAGE = Path(os.getenv("STORAGE_PATH", "./storage"))


@router.get("/firmware")
async def get_firmware():
    path = STORAGE / "firmware.json"
    if not path.exists():
        return JSONResponse({"version": "0.0.0", "path": "", "size": 0, "sha256": ""})
    return FileResponse(str(path), media_type="application/json")
