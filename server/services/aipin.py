import base64
import json
import logging
import os
import struct
import wave
from datetime import datetime
from pathlib import Path

import httpx
from fastapi import APIRouter, WebSocket, WebSocketDisconnect

log = logging.getLogger("aipin")

router = APIRouter()

STORAGE = Path(os.getenv("STORAGE_PATH", "./storage"))

# Prompt designed for speaker identification and diarization
TRANSCRIPTION_PROMPT = """Transcribe this audio with speaker diarization.

Instructions:
1. Identify different speakers by their voice characteristics
2. Label speakers as Person 1, Person 2, Person 3, etc. initially
3. If a speaker introduces themselves by name during the conversation, use their actual name from that point forward (and retroactively update previous labels if clear)
4. Format the output as a conversation transcript with speaker labels

Output format:
Person 1: [what they said]
Person 2: [what they said]

OR if names are known:
John: [what they said]
Sarah: [what they said]

Notes:
- Pay attention to voice pitch, tone, and speaking patterns to distinguish speakers
- If someone says "I'm [Name]" or "My name is [Name]", use that name
- Keep the transcript natural and readable
- Include filler words and natural speech patterns for authenticity
- If only one speaker is detected, still label them appropriately

Begin transcription:"""


@router.websocket("/aipin")
async def aipin_stream(websocket: WebSocket):
    await websocket.accept()
    log.info("AiPin client connected")

    recordings_dir = STORAGE / "recordings"
    transcripts_dir = STORAGE / "transcripts"
    recordings_dir.mkdir(parents=True, exist_ok=True)
    transcripts_dir.mkdir(parents=True, exist_ok=True)

    try:
        # 1. Receive APST header
        header = await websocket.receive_bytes()
        if header[:4] != b"APST":
            log.error("Invalid header magic")
            await websocket.close()
            return

        sample_rate = struct.unpack("<I", header[4:8])[0]
        bit_depth = struct.unpack("<H", header[8:10])[0]
        channels = struct.unpack("<H", header[10:12])[0]
        log.info(f"Stream: {sample_rate}Hz {bit_depth}bit {channels}ch")

        # 2. Prepare WAV file
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        wav_path = recordings_dir / f"aipin_{timestamp}.wav"
        wf = wave.open(str(wav_path), "wb")
        wf.setnchannels(channels)
        wf.setsampwidth(bit_depth // 8)
        wf.setframerate(sample_rate)

        # 3. Receive audio chunks until APND
        chunk_count = 0
        while True:
            data = await websocket.receive_bytes()
            if len(data) >= 4 and data[:4] == b"APND":
                break
            wf.writeframes(data)
            chunk_count += 1

        wf.close()
        log.info(f"Recording saved: {wav_path} ({chunk_count} chunks)")

        # 4. Transcribe via Gemini (if API key available)
        gemini_key = os.getenv("GEMINI_API_KEY")
        if gemini_key:
            transcript = await _transcribe(wav_path, gemini_key)
            if transcript:
                txt_path = transcripts_dir / f"aipin_{timestamp}.txt"
                txt_path.write_text(transcript)
                log.info(f"Transcript saved: {txt_path}")
                await websocket.send_text(transcript)

    except WebSocketDisconnect:
        log.info("AiPin client disconnected")
    except Exception as e:
        log.error(f"AiPin error: {e}")


async def _transcribe(wav_path: Path, api_key: str) -> str:
    """
    Transcribe audio using Gemini REST API with speaker diarization.
    Ported from aipin/server/server.py — uses raw HTTP POST to Gemini's
    multimodal streaming endpoint with base64-encoded WAV audio.
    """
    log.info(f"Transcribing {wav_path}...")

    audio_b64 = base64.b64encode(wav_path.read_bytes()).decode()

    url = (
        "https://generativelanguage.googleapis.com/v1beta/models/"
        "gemini-3-flash-preview:streamGenerateContent?alt=sse&key=" + api_key
    )

    payload = {
        "contents": [
            {
                "parts": [
                    {"text": TRANSCRIPTION_PROMPT},
                    {
                        "inline_data": {
                            "mime_type": "audio/wav",
                            "data": audio_b64,
                        }
                    },
                ]
            }
        ]
    }

    try:
        async with httpx.AsyncClient(timeout=120.0) as client:
            async with client.stream("POST", url, json=payload) as resp:
                if resp.status_code != 200:
                    body = await resp.aread()
                    log.error(
                        f"Gemini API error {resp.status_code}: "
                        f"{body.decode()[:200]}"
                    )
                    return ""

                log.info("--- Transcription ---")
                full_text = []
                async for line in resp.aiter_lines():
                    if not line or not line.startswith("data: "):
                        continue
                    try:
                        chunk = json.loads(line[len("data: "):])
                        text = (
                            chunk["candidates"][0]["content"]["parts"][0]
                            .get("text", "")
                        )
                        full_text.append(text)
                    except (json.JSONDecodeError, KeyError, IndexError):
                        continue

                transcript = "".join(full_text)
                log.info(f"Transcription complete ({len(transcript)} chars)")
                return transcript

    except httpx.HTTPError as e:
        log.error(f"Request error: {e}")
        return ""
