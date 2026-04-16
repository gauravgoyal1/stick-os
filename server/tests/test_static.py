def test_apps_static_serves_file(client, storage_dir):
    (storage_dir / "apps" / "hello.txt").write_text("hello from apps")
    r = client.get("/apps/hello.txt")
    assert r.status_code == 200
    assert r.text == "hello from apps"


def test_firmware_static_serves_file(client, storage_dir):
    (storage_dir / "firmware" / "v1.0.0.bin").write_bytes(b"\x00\x01\x02\x03")
    r = client.get("/firmware/v1.0.0.bin")
    assert r.status_code == 200
    assert r.content == b"\x00\x01\x02\x03"


def test_apps_static_404_for_missing_file(client):
    r = client.get("/apps/does-not-exist.bin")
    assert r.status_code == 404
