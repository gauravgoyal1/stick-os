#!/usr/bin/env python3
"""Push a local file to the Stick OS device's LittleFS via USB serial.

Uses the FILE_PUT protocol added in os/serial_cmd.cpp:
  FILE_PUT <path> <size>\n<size bytes>

Usage:
  tools/push_file.py --port /dev/cu.usbserial-XXXX --src apps/snake/main.py --dest /littlefs/apps/snake/main.py
"""
import argparse
import pathlib
import sys
import time

import serial


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", required=True)
    p.add_argument("--src", required=True, help="local file path")
    p.add_argument("--dest", required=True, help="target path on LittleFS (e.g. /littlefs/demo.py)")
    args = p.parse_args()

    src = pathlib.Path(args.src)
    if not src.exists():
        print(f"ERR: {src} does not exist", file=sys.stderr)
        return 1
    data = src.read_bytes()

    with serial.Serial(args.port, 115200, timeout=5) as s:
        time.sleep(0.3)
        s.reset_input_buffer()
        header = f"FILE_PUT {args.dest} {len(data)}\n".encode()
        s.write(header)
        s.flush()
        # Device reads bytes directly after the header line. Chunk the
        # payload — ESP32's Serial buffer is ~256 bytes, writing faster
        # than the device drains will drop bytes silently.
        time.sleep(0.1)
        chunk = 128
        for i in range(0, len(data), chunk):
            s.write(data[i:i + chunk])
            s.flush()
            time.sleep(0.02)  # 20ms gives the device time to drain
        # Collect the OK/ERR reply (device echoes it after writing the file).
        time.sleep(0.5)
        reply = b""
        deadline = time.time() + 10
        while time.time() < deadline:
            chunk = s.read(256)
            if chunk:
                reply += chunk
                if b"OK:" in reply or b"ERR:" in reply:
                    break
        out = reply.decode(errors="replace").strip()
        print(out)
        return 0 if b"OK:" in reply else 2


if __name__ == "__main__":
    raise SystemExit(main())
