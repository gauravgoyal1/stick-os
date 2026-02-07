#!/usr/bin/env python3
"""
AiPin WiFi Audio Server
Receives audio streamed from AiPin (M5StickC Plus 2) over TCP/WiFi
and generates transcripts with speaker identification using Gemini AI.

Usage:
    python server.py --port 8765
    python server.py --port 8765 --output recordings/
"""

import argparse
import base64
import json
import logging
import os
import socket
import struct
import sys
import threading
import time
import wave
from datetime import datetime
from pathlib import Path

log = logging.getLogger("aipin-server")

# Load .env file if present
_env_path = Path(__file__).parent.parent / '.env'
if _env_path.exists():
    for line in _env_path.read_text().splitlines():
        line = line.strip()
        if line and not line.startswith('#') and '=' in line:
            key, _, value = line.partition('=')
            os.environ.setdefault(key.strip(), value.strip())

try:
    import requests
except ImportError:
    print("Error: requests is required. Install with: pip install requests")
    sys.exit(1)

# Protocol constants (must match firmware)
START_MAGIC = b'\x41\x50\x53\x54'  # "APST"
STOP_MAGIC  = b'\x41\x50\x4E\x44'  # "APND"


class AudioSession:
    """Handles a single audio recording session from a client."""
    
    def __init__(self, conn, addr, output_dir, transcript_dir):
        self.conn = conn
        self.addr = addr
        self.output_dir = Path(output_dir)
        self.transcript_dir = Path(transcript_dir)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.transcript_dir.mkdir(parents=True, exist_ok=True)
        
        self.sample_rate = 8000
        self.bit_depth = 8
        self.channels = 1
        self.pcm_data = bytearray()
        
    def wait_for_header(self):
        """Wait for the stream start header."""
        log.info(f"[{self.addr}] Waiting for audio stream...")
        buf = b''
        
        while True:
            try:
                byte = self.conn.recv(1)
                if not byte:
                    log.warning(f"[{self.addr}] Connection closed while waiting for header")
                    return False
                    
                buf += byte
                if len(buf) > 256:
                    buf = buf[-4:]
                    
                if buf[-4:] == START_MAGIC:
                    log.debug(f"[{self.addr}] Start magic found")
                    rest = self.conn.recv(8)
                    if len(rest) < 8:
                        log.error(f"[{self.addr}] Incomplete header received")
                        return False
                        
                    self.sample_rate = struct.unpack('<I', rest[0:4])[0]
                    self.bit_depth = struct.unpack('<H', rest[4:6])[0]
                    self.channels = struct.unpack('<H', rest[6:8])[0]
                    
                    log.info(f"[{self.addr}] Header: {self.sample_rate}Hz, {self.bit_depth}-bit, {self.channels}ch")
                    return True
                    
            except socket.timeout:
                continue
            except Exception as e:
                log.error(f"[{self.addr}] Error reading header: {e}")
                return False
    
    def receive_audio(self):
        """Receive audio data until stop marker."""
        chunk_size = 1024
        trailing = b''
        start_time = time.time()
        
        log.info(f"[{self.addr}] Recording: {self.sample_rate}Hz, {self.bit_depth}-bit, "
                 f"{'mono' if self.channels == 1 else 'stereo'}")
        
        try:
            while True:
                try:
                    data = self.conn.recv(chunk_size)
                    if not data:
                        log.warning(f"[{self.addr}] Connection closed during recording")
                        break
                        
                    # Check for stop magic
                    combined = trailing + data
                    stop_pos = combined.find(STOP_MAGIC)
                    
                    if stop_pos >= 0:
                        # Extract audio before stop marker
                        audio_before_stop = combined[:stop_pos]
                        fresh_audio = audio_before_stop[len(trailing):]
                        self.pcm_data.extend(fresh_audio)
                        
                        elapsed = time.time() - start_time
                        log.info(f"[{self.addr}] Stop marker received after {elapsed:.1f}s, "
                                 f"{len(self.pcm_data):,} bytes")
                        break
                    else:
                        # Save all but last 3 bytes (stop magic could span reads)
                        if len(combined) > 3:
                            safe = combined[:-3]
                            new_safe = safe[len(trailing):]
                            self.pcm_data.extend(new_safe)
                            trailing = combined[-3:]
                        else:
                            trailing = combined
                            
                except socket.timeout:
                    continue
                    
        except Exception as e:
            log.error(f"[{self.addr}] Error receiving audio: {e}")
    
    def save_wav(self):
        """Save recorded audio as WAV file."""
        sample_width = self.bit_depth // 8
        duration = len(self.pcm_data) / (self.sample_rate * sample_width * self.channels)
        
        if len(self.pcm_data) == 0:
            log.warning(f"[{self.addr}] No audio data to save")
            return None
        
        # WAV 8-bit format requires unsigned samples (0-255, center=128)
        # Firmware sends signed int8 (-128 to 127), so convert
        if self.bit_depth == 8:
            self.pcm_data = bytearray((b + 128) & 0xFF for b in self.pcm_data)
        
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        output_path = self.output_dir / f'aipin_{timestamp}.wav'
        
        log.info(f"[{self.addr}] Writing WAV: {output_path}")
        log.info(f"[{self.addr}] Duration: {duration:.2f}s, Size: {len(self.pcm_data):,} bytes")
        
        with wave.open(str(output_path), 'wb') as wf:
            wf.setnchannels(self.channels)
            wf.setsampwidth(sample_width)
            wf.setframerate(self.sample_rate)
            wf.writeframes(bytes(self.pcm_data))
        
        return output_path


def transcribe_audio(audio_path, transcript_dir):
    """
    Transcribe audio using Gemini REST API with speaker diarization.
    Identifies different speakers and uses their names if introduced.
    """
    api_key = os.environ.get('GEMINI_API_KEY')
    if not api_key:
        log.warning("GEMINI_API_KEY not set. Skipping transcription.")
        return None

    transcript_dir = Path(transcript_dir)
    transcript_dir.mkdir(parents=True, exist_ok=True)

    log.info(f"Transcribing {audio_path}...")

    audio_b64 = base64.b64encode(Path(audio_path).read_bytes()).decode()
    
    url = ("https://generativelanguage.googleapis.com/v1beta/models/"
           "gemini-3-flash-preview:streamGenerateContent?alt=sse&key=" + api_key)
    
    # Prompt designed for speaker identification and diarization
    transcription_prompt = """Transcribe this audio with speaker diarization. 

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

    payload = {
        "contents": [{
            "parts": [
                {"text": transcription_prompt},
                {"inline_data": {"mime_type": "audio/wav", "data": audio_b64}},
            ]
        }]
    }

    try:
        resp = requests.post(url, json=payload, stream=True, timeout=120)
        if resp.status_code != 200:
            log.error(f"Gemini API error {resp.status_code}: {resp.text[:200]}")
            return None

        log.info("--- Transcription ---")
        full_text = []
        for line in resp.iter_lines(decode_unicode=True):
            if not line or not line.startswith("data: "):
                continue
            try:
                chunk = json.loads(line[len("data: "):])
                text = chunk["candidates"][0]["content"]["parts"][0].get("text", "")
                print(text, end='', flush=True)
                full_text.append(text)
            except (json.JSONDecodeError, KeyError, IndexError):
                continue
        print("\n--- End ---")

        transcript_name = Path(audio_path).stem + '.txt'
        transcript_path = transcript_dir / transcript_name
        transcript_path.write_text(''.join(full_text))

        log.info(f"Transcript saved: {transcript_path}")
        return transcript_path
        
    except requests.RequestException as e:
        log.error(f"Request error: {e}")
        return None


def handle_client(conn, addr, output_dir, transcript_dir, no_transcribe):
    """Handle a single client connection."""
    log.info(f"New connection from {addr}")
    conn.settimeout(2.0)
    
    try:
        session = AudioSession(conn, addr, output_dir, transcript_dir)
        
        while True:
            if not session.wait_for_header():
                break
                
            session.receive_audio()
            audio_path = session.save_wav()
            
            if audio_path and not no_transcribe:
                transcribe_audio(audio_path, transcript_dir)
            
            # Reset for next recording in same session
            session.pcm_data = bytearray()
            log.info(f"[{addr}] Ready for next recording...")
            
    except Exception as e:
        log.error(f"[{addr}] Session error: {e}")
    finally:
        conn.close()
        log.info(f"[{addr}] Connection closed")


def run_server(host, port, output_dir, transcript_dir, no_transcribe):
    """Run the TCP server."""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((host, port))
    server.listen(5)
    
    # Get local IP addresses for display
    hostname = socket.gethostname()
    try:
        local_ip = socket.gethostbyname(hostname)
    except:
        local_ip = "127.0.0.1"
    
    log.info(f"=" * 50)
    log.info(f"AiPin WiFi Server started")
    log.info(f"Listening on {host}:{port}")
    log.info(f"Local IP: {local_ip}")
    log.info(f"Output: {output_dir}")
    log.info(f"Transcripts: {transcript_dir}")
    log.info(f"=" * 50)
    print(f"\nUpdate your aipin_wifi.ino with:")
    print(f'  const char* serverHost = "{local_ip}";')
    print(f'  const uint16_t serverPort = {port};\n')
    
    try:
        while True:
            conn, addr = server.accept()
            # Handle each client in a separate thread
            client_thread = threading.Thread(
                target=handle_client,
                args=(conn, addr, output_dir, transcript_dir, no_transcribe),
                daemon=True
            )
            client_thread.start()
            
    except KeyboardInterrupt:
        log.info("\nShutting down server...")
    finally:
        server.close()


def main():
    parser = argparse.ArgumentParser(
        description="AiPin WiFi Audio Server - Receive audio over TCP and transcribe"
    )
    parser.add_argument('--host', '-H', default='0.0.0.0',
                        help='Host to bind to (default: 0.0.0.0)')
    parser.add_argument('--port', '-p', type=int, default=8765,
                        help='Port to listen on (default: 8765)')
    parser.add_argument('--output', '-o', default='../recordings',
                        help='Output directory for WAV files (default: ../recordings)')
    parser.add_argument('--transcripts', '-t', default='../transcripts',
                        help='Output directory for transcripts (default: ../transcripts)')
    parser.add_argument('--no-transcribe', action='store_true',
                        help='Skip Gemini transcription')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Enable debug logging')
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format='%(asctime)s [%(levelname)s] %(message)s',
        datefmt='%H:%M:%S'
    )

    run_server(args.host, args.port, args.output, args.transcripts, args.no_transcribe)


if __name__ == '__main__':
    main()
