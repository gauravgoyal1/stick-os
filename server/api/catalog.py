import os
from pathlib import Path

from fastapi import APIRouter
from fastapi.responses import FileResponse, JSONResponse

router = APIRouter()

STORAGE = Path(os.getenv("STORAGE_PATH", "./storage"))


@router.get("/catalog")
async def get_catalog():
    path = STORAGE / "catalog.json"
    if not path.exists():
        return JSONResponse({"version": 1, "apps": []})
    return FileResponse(str(path), media_type="application/json")
