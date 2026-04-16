def test_root_returns_server_info(client):
    r = client.get("/")
    assert r.status_code == 200
    body = r.json()
    assert body["name"] == "Stick OS Server"
    assert "version" in body
