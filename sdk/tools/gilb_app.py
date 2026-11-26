#!/usr/bin/env python3
"""gilb_app.py -- turn a linked GILB program (objcopy -O binary) into a .APP file.

    python3 sdk/tools/gilb_app.py IN.bin OUT.APP

Checks the header (sdk/include/gilb_app.h), that the file's length matches the header's load_end
and that everything fits GILB_APP_BASE..GILB_APP_END, then sets the CRC GILB checks: CRC-32 with
poly 0x04C10DB7 (not 0x04C11DB7), MSB first, init 0, no final xor, over the whole file with the
crc field as 0 (the player's own, firmware/services/fw_update.c).
"""
import struct
import sys

MAGIC = 0x50504147          # "GAPP"
HDR = 64
BASE, END = 0x03030000, 0x0304C000
POLY = 0x04C10DB7


def crc32(data):
    c = 0
    for b in data:
        c ^= b << 24
        for _ in range(8):
            c = ((c << 1) ^ POLY) & 0xFFFFFFFF if c & 0x80000000 else (c << 1) & 0xFFFFFFFF
    return c


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    blob = bytearray(open(sys.argv[1], "rb").read())
    if len(blob) < HDR:
        raise SystemExit("gilb_app: too short for a header")
    magic, hsize, api_min, base, load_end, mem_end, entry = struct.unpack_from("<7I", blob, 0)
    if magic != MAGIC or hsize != HDR:
        raise SystemExit("gilb_app: no GILB header first (is crt0.c linked, with app.ld?)")
    if base != BASE:
        raise SystemExit(f"gilb_app: linked at 0x{base:08X}, not 0x{BASE:08X}")
    if load_end != BASE + len(blob):
        raise SystemExit(f"gilb_app: header says {load_end - BASE} B, the file is {len(blob)} B")
    if mem_end > END or mem_end < load_end:
        raise SystemExit(f"gilb_app: needs {mem_end - BASE} B of memory, there are {END - BASE}")
    if not entry & 1 or not BASE + HDR <= entry < load_end:
        raise SystemExit(f"gilb_app: entry 0x{entry:08X} is not Thumb code inside the program")
    struct.pack_into("<I", blob, 28, 0)
    struct.pack_into("<I", blob, 28, crc32(bytes(blob)))
    open(sys.argv[2], "wb").write(blob)
    name = bytes(blob[32:64]).split(b"\0")[0].decode("utf-8", "replace")
    print(f"{sys.argv[2]}: {name or '(no name)'}, {len(blob)} B file, {mem_end - BASE} of {END - BASE} B memory")


if __name__ == "__main__":
    main()
