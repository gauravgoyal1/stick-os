#!/usr/bin/env python3
"""Seed WiFi credentials to a Stick OS device over USB serial."""

import argparse
import serial
import sys
import time


def main():
    p = argparse.ArgumentParser(description="Stick OS WiFi credential seeder")
    sub = p.add_subparsers(dest="cmd", required=True)

    add = sub.add_parser("add", help="Add a WiFi network")
    add.add_argument("--port", required=True)
    add.add_argument("--ssid", required=True)
    add.add_argument("--password", required=True)

    ls = sub.add_parser("list", help="List stored networks")
    ls.add_argument("--port", required=True)

    rm = sub.add_parser("delete", help="Delete a WiFi network")
    rm.add_argument("--port", required=True)
    rm.add_argument("--ssid", required=True)

    ak_set = sub.add_parser("apikey-set", help="Set API key for service auth")
    ak_set.add_argument("--port", required=True)
    ak_set.add_argument("--key", required=True)

    ak_get = sub.add_parser("apikey-get", help="Show stored API key")
    ak_get.add_argument("--port", required=True)

    args = p.parse_args()

    with serial.Serial(args.port, 115200, timeout=3) as s:
        time.sleep(0.5)  # wait for device to be ready
        s.reset_input_buffer()

        if args.cmd == "add":
            s.write(f"WIFI_SET {args.ssid} {args.password}\n".encode())
        elif args.cmd == "list":
            s.write(b"WIFI_LIST\n")
        elif args.cmd == "delete":
            s.write(f"WIFI_DEL {args.ssid}\n".encode())
        elif args.cmd == "apikey-set":
            s.write(f"APIKEY_SET {args.key}\n".encode())
        elif args.cmd == "apikey-get":
            s.write(b"APIKEY_GET\n")

        time.sleep(0.5)
        while s.in_waiting:
            print(s.readline().decode().strip())


if __name__ == "__main__":
    main()
