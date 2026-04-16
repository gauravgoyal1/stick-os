import json


def test_catalog_default_when_missing(client):
    r = client.get("/api/catalog")
    assert r.status_code == 200
    assert r.json() == {"version": 1, "apps": []}


def test_catalog_returns_stored_json(client, storage_dir):
    payload = {
        "version": 1,
        "apps": [
            {
                "id": "snake",
                "name": "Snake",
                "version": "1.0.0",
                "sha256": "abc",
            }
        ],
    }
    (storage_dir / "catalog.json").write_text(json.dumps(payload))

    r = client.get("/api/catalog")
    assert r.status_code == 200
    assert r.json() == payload


def test_catalog_content_type_is_json(client, storage_dir):
    (storage_dir / "catalog.json").write_text('{"version":1,"apps":[]}')
    r = client.get("/api/catalog")
    assert r.headers["content-type"].startswith("application/json")
