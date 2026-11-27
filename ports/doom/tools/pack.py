#!/usr/bin/env python3
"""pack.py -- Doom's two files from its link (doom.ld): DOOM.APP and DOOM.BIN.

    python3 tools/pack.py build/doom.elf build/DOOM.APP

DOOM.APP is the segment at GILB_APP_BASE, as GILB loads and checks it (sdk/tools/gilb_app.py sets
its CRC). DOOM.BIN, next to it, holds the other segments for the first stage (gilb/gilb_main.c):

    u32 magic "MBIN", u32 the CRC field of DOOM.APP's header, u32 CRC-32 of the blocks' bytes,
    u32 the number of blocks, then 4 x (u32 address, u32 bytes in the file, u32 bytes in memory),
    then each block's bytes in turn.

CRC-32: GILB's (poly 0x04C10DB7, MSB first, init 0, no final xor).
"""
import os
import struct
import subprocess
import sys

BASE = 0x03030000
HERE = os.path.dirname(os.path.abspath(__file__))
GILB_APP = os.path.join(HERE, "..", "..", "..", "sdk", "tools", "gilb_app.py")


def crc32(data, c=0):
    for b in data:
        c ^= b << 24
        for _ in range(8):
            c = ((c << 1) ^ 0x04C10DB7) & 0xFFFFFFFF if c & 0x80000000 else (c << 1) & 0xFFFFFFFF
    return c


def segments(elf):
    if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 1:
        raise SystemExit("pack: not a 32-bit little-endian ELF")
    phoff, = struct.unpack_from("<I", elf, 28)
    phentsize, phnum = struct.unpack_from("<HH", elf, 42)
    out = []
    for i in range(phnum):
        p_type, p_offset, p_vaddr, _, p_filesz, p_memsz = struct.unpack_from("<6I", elf, phoff + i * phentsize)
        if p_type == 1 and p_memsz:
            out.append((p_vaddr, elf[p_offset:p_offset + p_filesz], p_memsz))
    return out


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    elf = open(sys.argv[1], "rb").read()
    segs = segments(elf)
    app = [s for s in segs if s[0] == BASE]
    rest = [s for s in segs if s[0] != BASE]
    if len(app) != 1 or len(rest) > 4:
        raise SystemExit("pack: expected one segment at 0x%08X and at most 4 more" % BASE)
    raw = sys.argv[2] + ".raw"
    open(raw, "wb").write(app[0][1])
    subprocess.check_call([sys.executable, "-I", GILB_APP, raw, sys.argv[2]])
    os.remove(raw)
    app_crc, = struct.unpack_from("<I", open(sys.argv[2], "rb").read(), 28)

    body = b"".join(s[1] for s in rest)
    hdr = struct.pack("<4I", 0x4E49424D, app_crc, crc32(body), len(rest))
    for addr, data, memsz in rest + [(0, b"", 0)] * (4 - len(rest)):
        hdr += struct.pack("<3I", addr, len(data), memsz)
    binpath = os.path.join(os.path.dirname(sys.argv[2]), "DOOM.BIN")
    open(binpath, "wb").write(hdr + body)
    for addr, data, memsz in rest:
        print("  0x%08X: %6d B from the file, %6d B in memory" % (addr, len(data), memsz))
    print("%s: %d B" % (binpath, len(hdr) + len(body)))


if __name__ == "__main__":
    main()
