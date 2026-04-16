import os
from pathlib import Path

from fastapi import APIRouter
from fastapi.responses import FileResponse, JSONResponse

router = APIRouter()

STORAGE = Path(os.getenv("STORAGE_PATH", "./storage"))


@router.get("/wifi")
async def get_wifi_networks():
    path = STORAGE / "wifi.json"
    if not path.exists():
        return JSONResponse({"networks": []})
    return FileResponse(str(path), media_type="application/json")
