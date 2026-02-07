#!/usr/bin/env python3
"""
AiPin Audio Receiver
Receives audio streamed from AiPin (M5StickC Plus 2) over Bluetooth SPP
and saves it as a .wav file.

Usage:
    python receiver.py --port /dev/cu.AiPin --output recording.wav
    python receiver.py --port /dev/cu.AiPin  # auto-generates timestamped filename
    python receiver.py --list                 # list available serial ports
"""

import argparse
import logging
import struct
import sys
import time
import wave
from datetime import datetime
from pathlib import Path

log = logging.getLogger("aipin")

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial is required. Install with: pip install pyserial")
    sys.exit(1)

# Protocol constants (must match firmware)
START_MAGIC = b'\x41\x50\x53\x54'  # "APST"
STOP_MAGIC  = b'\x41\x50\x4E\x44'  # "APND"


def list_ports():
    """List all available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("Available serial ports:")
    for p in ports:
        print(f"  {p.device} - {p.description}")


def wait_for_header(ser):
    """
    Wait for the stream start header.
    Returns (sample_rate, bit_depth, channels) or None on timeout/error.
    """
    print("Waiting for audio stream from AiPin...")
    buf = b''
    total_bytes_seen = 0
    last_status = time.time()
    last_data_time = None
    timeouts = 0
    while True:
        byte = ser.read(1)
        if not byte:
            timeouts += 1
            now = time.time()
            if now - last_status >= 5:
                elapsed = now - last_status
                log.debug(f"Still waiting... {total_bytes_seen} bytes received so far, "
                          f"{timeouts} read timeouts in last {elapsed:.0f}s")
                if total_bytes_seen == 0:
                    log.debug("No data received yet — is the device connected and streaming?")
                    log.debug(f"  Port: {ser.port}, DSR={ser.dsr}, CTS={ser.cts}")
                last_status = now
                timeouts = 0
            continue
        total_bytes_seen += 1
        if last_data_time is None:
            log.debug(f"First byte received: 0x{byte[0]:02x}")
        last_data_time = time.time()
        buf += byte
        if len(buf) > 256:
            buf = buf[-4:]
        if total_bytes_seen <= 20:
            log.debug(f"  byte #{total_bytes_seen}: 0x{byte[0]:02x} "
                      f"({chr(byte[0]) if 32 <= byte[0] < 127 else '.'})")
        if buf[-4:] == START_MAGIC:
            log.debug(f"Start magic found after {total_bytes_seen} bytes")
            rest = ser.read(8)
            if len(rest) < 8:
                print("Error: incomplete header received.")
                log.debug(f"  Only got {len(rest)} of 8 header bytes: {rest.hex()}")
                return None
            sample_rate = struct.unpack('<I', rest[0:4])[0]
            bit_depth = struct.unpack('<H', rest[4:6])[0]
            channels = struct.unpack('<H', rest[6:8])[0]
            log.debug(f"Header parsed: rate={sample_rate}, bits={bit_depth}, ch={channels}")
            return sample_rate, bit_depth, channels


def receive_audio(ser, sample_rate, bit_depth, channels, output_path):
    """
    Receive raw PCM audio data until stop marker is received.
    Writes a proper WAV file.
    """
    sample_width = bit_depth // 8
    chunk_size = 1024

    total_bytes = 0
    pcm_data = bytearray()
    trailing = b''

    print(f"Recording: {sample_rate}Hz, {bit_depth}-bit, "
          f"{'mono' if channels == 1 else 'stereo'}")
    print("Receiving audio data... (press Ctrl+C to force stop)")

    start_time = time.time()
    chunks_received = 0
    empty_reads = 0
    last_data_time = time.time()

    try:
        while True:
            data = ser.read(chunk_size)
            if not data:
                empty_reads += 1
                gap = time.time() - last_data_time
                if gap > 3:
                    log.debug(f"No data for {gap:.1f}s ({empty_reads} empty reads) — "
                              f"connection may be lost")
                    last_data_time = time.time()  # reset to avoid spamming
                    empty_reads = 0
                continue

            last_data_time = time.time()
            chunks_received += 1
            if chunks_received <= 3:
                log.debug(f"Chunk #{chunks_received}: {len(data)} bytes, "
                          f"first 16: {data[:16].hex()}")

            # Check for stop magic in received data
            combined = trailing + data
            stop_pos = combined.find(STOP_MAGIC)

            if stop_pos >= 0:
                audio_before_stop = combined[:stop_pos]
                fresh_audio = audio_before_stop[len(trailing):]
                pcm_data.extend(fresh_audio)
                total_bytes += len(fresh_audio)
                elapsed = time.time() - start_time
                print("\nStop marker received.")
                log.debug(f"Stop after {chunks_received} chunks, "
                          f"{total_bytes:,} bytes, {elapsed:.1f}s")
                break
            else:
                # No stop marker. Save all but last 3 bytes (stop magic could span reads)
                if len(combined) > 3:
                    safe = combined[:-3]
                    new_safe = safe[len(trailing):]
                    pcm_data.extend(new_safe)
                    total_bytes += len(new_safe)
                    trailing = combined[-3:]
                else:
                    trailing = combined

                elapsed = time.time() - start_time
                print(f"\r  Received: {total_bytes:,} bytes ({elapsed:.1f}s)",
                      end='', flush=True)

    except KeyboardInterrupt:
        print("\n\nRecording interrupted by user.")

    duration = len(pcm_data) / (sample_rate * sample_width * channels)

    print(f"Writing WAV file: {output_path}")
    print(f"  Duration: {duration:.2f}s")
    print(f"  Data size: {len(pcm_data):,} bytes")

    with wave.open(str(output_path), 'wb') as wf:
        wf.setnchannels(channels)
        wf.setsampwidth(sample_width)
        wf.setframerate(sample_rate)
        wf.writeframes(bytes(pcm_data))

    print(f"Saved: {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description="AiPin Audio Receiver - Receive BT audio and save as WAV"
    )
    parser.add_argument('--port', '-p',
                        help='Serial port (e.g., /dev/cu.AiPin)')
    parser.add_argument('--output', '-o',
                        help='Output WAV file path (default: aipin_YYYYMMDD_HHMMSS.wav)')
    parser.add_argument('--list', '-l', action='store_true',
                        help='List available serial ports and exit')
    parser.add_argument('--baud', '-b', type=int, default=115200,
                        help='Baud rate (default: 115200)')
    parser.add_argument('--once', action='store_true',
                        help='Exit after first recording (default: keep listening)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Enable debug logging')
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.WARNING,
        format='[%(levelname)s] %(message)s'
    )

    if args.list:
        list_ports()
        return

    if not args.port:
        print("Error: --port is required. Use --list to see available ports.")
        sys.exit(1)

    if not args.output:
        args.output = 'recordings/'
    output_dir = None
    if Path(args.output).is_dir() or args.output.endswith('/'):
        output_dir = Path(args.output)
        output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Opening serial port: {args.port}")
    try:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            timeout=1.0,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        )
    except serial.SerialException as e:
        print(f"Error opening serial port: {e}")
        sys.exit(1)

    print(f"Connected to {args.port} @ {args.baud} baud")
    log.debug(f"Port details: DSR={ser.dsr}, CTS={ser.cts}, RI={ser.ri}, CD={ser.cd}")
    log.debug(f"  timeout={ser.timeout}s, bytesize={ser.bytesize}, "
              f"parity={ser.parity}, stopbits={ser.stopbits}")
    print(f"Output: {args.output}")

    try:
        recording_count = 0
        while True:
            header = wait_for_header(ser)
            if header is None:
                print("Failed to receive valid header. Retrying...")
                continue

            sample_rate, bit_depth, channels = header
            print(f"Header received: {sample_rate}Hz, {bit_depth}-bit, {channels}ch")

            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            if output_dir:
                output_path = output_dir / f'aipin_{timestamp}.wav'
            else:
                output_path = Path(args.output)

            receive_audio(ser, sample_rate, bit_depth, channels, output_path)
            recording_count += 1

            if args.once:
                break

            print("\nWaiting for next recording...")

    except KeyboardInterrupt:
        print("\nExiting.")
    finally:
        ser.close()
        print("Serial port closed.")


if __name__ == '__main__':
    main()
