"""
Tests for the /services/scribe WebSocket endpoint.

Protocol:
  Connect with ?key=<STICK_API_KEY> query param.
  Send APST header (12 bytes: b'APST' + u32 sample_rate + u16 bit_depth + u16 channels).
  Send audio frames as binary messages.
  Send APND magic (b'APND') to signal end — server writes a WAV file.

Transcription is skipped in tests (no GEMINI_API_KEY), so we verify only
the audio-capture portion of the flow.
"""
import os
import struct
import wave

import pytest
from starlette.websockets import WebSocketDisconnect


def _connect(client, key):
    """Open the scribe WebSocket with the given auth key."""
    return client.websocket_connect(f"/services/scribe?key={key}")


def _apst_header(sample_rate=8000, bit_depth=8, channels=1):
    return b"APST" + struct.pack("<IHH", sample_rate, bit_depth, channels)


def test_scribe_rejects_missing_key(client):
    with pytest.raises(WebSocketDisconnect):
        with client.websocket_connect("/services/scribe") as ws:
            ws.receive_bytes()  # force the handshake to surface the close


def test_scribe_rejects_wrong_key(client):
    with pytest.raises(WebSocketDisconnect):
        with _connect(client, "not-the-key") as ws:
            ws.receive_bytes()


def test_scribe_accepts_correct_key(client, api_key):
    # Merely opening and closing cleanly should not raise during accept.
    with _connect(client, api_key) as ws:
        ws.send_bytes(_apst_header())
        # Send one small audio frame so the server has something to write.
        ws.send_bytes(b"\x00" * 256)
        ws.send_bytes(b"APND")
    # Exiting the context closes cleanly.


def test_scribe_rejects_bad_header_magic(client, api_key):
    with _connect(client, api_key) as ws:
        # Send 12 bytes that don't start with APST.
        ws.send_bytes(b"XXXX" + b"\x00" * 8)
        # Server closes immediately on bad magic.


def test_scribe_writes_wav_on_apnd(client, api_key, storage_dir):
    sample_rate = 8000
    bit_depth = 8
    channels = 1
    # Use a deterministic audio payload so we can verify bytes round-trip.
    audio = bytes(range(256)) * 4  # 1024 bytes of distinct samples

    with _connect(client, api_key) as ws:
        ws.send_bytes(_apst_header(sample_rate, bit_depth, channels))
        ws.send_bytes(audio)
        ws.send_bytes(b"APND")
    # Give the server a moment to finish writing after the ws closes.
    # (TestClient runs in-process; the context manager exit already waited.)

    recordings = list((storage_dir / "recordings").glob("scribe_*.wav"))
    assert len(recordings) == 1, f"expected 1 wav, got {recordings}"

    with wave.open(str(recordings[0]), "rb") as wf:
        assert wf.getframerate() == sample_rate
        assert wf.getsampwidth() == bit_depth // 8
        assert wf.getnchannels() == channels
        assert wf.readframes(wf.getnframes()) == audio


def test_scribe_handles_multiple_audio_chunks(client, api_key, storage_dir):
    with _connect(client, api_key) as ws:
        ws.send_bytes(_apst_header())
        # 5 chunks of 512 bytes each
        for i in range(5):
            ws.send_bytes(bytes([i]) * 512)
        ws.send_bytes(b"APND")

    recordings = list((storage_dir / "recordings").glob("scribe_*.wav"))
    assert len(recordings) == 1
    with wave.open(str(recordings[0]), "rb") as wf:
        # 5 chunks × 512 bytes @ 1 byte/sample = 2560 frames
        assert wf.getnframes() == 2560
