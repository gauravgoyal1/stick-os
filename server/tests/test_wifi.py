import json


def test_wifi_default_when_missing(client):
    r = client.get("/api/wifi")
    assert r.status_code == 200
    assert r.json() == {"networks": []}


def test_wifi_returns_stored_json(client, storage_dir):
    payload = {"networks": [{"ssid": "Home", "password": "secret"}]}
    (storage_dir / "wifi.json").write_text(json.dumps(payload))

    r = client.get("/api/wifi")
    assert r.status_code == 200
    assert r.json() == payload
