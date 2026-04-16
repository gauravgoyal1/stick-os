"""
Shared fixtures for server tests.

STORAGE_PATH and STICK_API_KEY are captured at module import time by the
route files, so we set them via env vars *before* importing the FastAPI
app. The `tmp_storage` fixture points STORAGE at a pytest tmp dir so tests
never touch the real server/storage/.
"""
import os
import shutil
import sys
from pathlib import Path

import pytest

TEST_STORAGE = Path("/tmp/stick-os-test-storage")
TEST_API_KEY = "test-api-key-12345"


def pytest_configure(config):
    """Set env vars before any test collection imports the app."""
    TEST_STORAGE.mkdir(parents=True, exist_ok=True)
    (TEST_STORAGE / "apps").mkdir(exist_ok=True)
    (TEST_STORAGE / "firmware").mkdir(exist_ok=True)
    (TEST_STORAGE / "recordings").mkdir(exist_ok=True)
    (TEST_STORAGE / "transcripts").mkdir(exist_ok=True)
    os.environ["STORAGE_PATH"] = str(TEST_STORAGE)
    os.environ["STICK_API_KEY"] = TEST_API_KEY

    # Ensure the server package is importable
    server_dir = Path(__file__).parent.parent
    sys.path.insert(0, str(server_dir))


def pytest_unconfigure(config):
    """Clean up tmp storage after the test session."""
    if TEST_STORAGE.exists():
        shutil.rmtree(TEST_STORAGE, ignore_errors=True)


@pytest.fixture
def storage_dir():
    """Path to the test storage directory. Tests write files here to
    exercise the "file present" code paths. Cleaned between tests to
    keep "file missing" defaults intact."""
    for name in ("catalog.json", "firmware.json", "wifi.json"):
        (TEST_STORAGE / name).unlink(missing_ok=True)
    for sub in ("apps", "firmware"):
        d = TEST_STORAGE / sub
        for child in d.iterdir():
            if child.is_file():
                child.unlink()
    return TEST_STORAGE


@pytest.fixture
def client(storage_dir):
    """FastAPI TestClient — imports the app lazily so env vars are set."""
    from fastapi.testclient import TestClient
    from main import app
    return TestClient(app)


@pytest.fixture
def api_key():
    return TEST_API_KEY
