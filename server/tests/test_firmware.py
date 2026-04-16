import json


def test_firmware_default_when_missing(client):
    r = client.get("/api/firmware")
    assert r.status_code == 200
    body = r.json()
    assert body == {"version": "0.0.0", "path": "", "size": 0, "sha256": ""}


def test_firmware_returns_stored_json(client, storage_dir):
    payload = {
        "version": "1.2.3",
        "path": "firmware/v1.2.3.bin",
        "size": 1048576,
        "sha256": "deadbeef",
    }
    (storage_dir / "firmware.json").write_text(json.dumps(payload))

    r = client.get("/api/firmware")
    assert r.status_code == 200
    assert r.json() == payload
