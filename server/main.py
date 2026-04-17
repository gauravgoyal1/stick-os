import os
from pathlib import Path

from dotenv import load_dotenv
from fastapi import FastAPI
from fastapi.staticfiles import StaticFiles

load_dotenv()

STORAGE = Path(os.getenv("STORAGE_PATH", "./storage"))

app = FastAPI(title="Stick OS Server")

# API routes
from api.catalog import router as catalog_router
from api.firmware import router as firmware_router
from api.transcripts import router as transcripts_router
from api.wifi import router as wifi_router

app.include_router(catalog_router, prefix="/api")
app.include_router(firmware_router, prefix="/api")
app.include_router(transcripts_router, prefix="/api")
app.include_router(wifi_router, prefix="/api")

# Service routes (WebSocket)
from services.scribe import router as scribe_router

app.include_router(scribe_router, prefix="/services")

# Static file serving for downloads
if (STORAGE / "apps").exists():
    app.mount("/apps", StaticFiles(directory=str(STORAGE / "apps")), name="apps")
if (STORAGE / "firmware").exists():
    app.mount("/firmware", StaticFiles(directory=str(STORAGE / "firmware")), name="firmware")


@app.get("/")
async def root():
    return {"name": "Stick OS Server", "version": "1.0.0"}
