#!/usr/bin/env python3
"""Merge two Intel HEX files into one using only 32-bit linear address records.

usage: mergehex.py softdevice.hex app.hex out.hex
Checks every record checksum and refuses to merge if the two files overlap.
"""
import sys


def parse(path):
    mem = {}
    base = 0
    for lineno, line in enumerate(open(path), 1):
        line = line.strip()
        if not line.startswith(":"):
            continue
        b = bytes.fromhex(line[1:])
        n, addr, typ = b[0], (b[1] << 8) | b[2], b[3]
        data = b[4:4 + n]
        if sum(b) & 0xFF != 0:
            raise SystemExit(f"{path}:{lineno}: bad checksum")
        if typ == 0:
            for i, v in enumerate(data):
                mem[(base + addr + i) & 0xFFFFFFFF] = v
        elif typ == 1:
            break
        elif typ == 2:
            base = ((data[0] << 8) | data[1]) << 4
        elif typ == 4:
            base = ((data[0] << 8) | data[1]) << 16
        # types 3 and 5 (start address) are ignored on purpose
    return mem


def rec(addr16, typ, data):
    b = bytes([len(data), addr16 >> 8, addr16 & 0xFF, typ]) + bytes(data)
    return ":" + (b + bytes([(-sum(b)) & 0xFF])).hex().upper()


def write(mem, path):
    lines = []
    hi = None
    addrs = sorted(mem)
    i = 0
    while i < len(addrs):
        a = addrs[i]
        if (a >> 16) != hi:
            hi = a >> 16
            lines.append(rec(0, 4, [hi >> 8, hi & 0xFF]))
        chunk = [mem[a]]
        j = i + 1
        while (j < len(addrs) and addrs[j] == a + len(chunk)
               and len(chunk) < 16 and ((a + len(chunk)) >> 16) == hi):
            chunk.append(mem[addrs[j]])
            j += 1
        lines.append(rec(a & 0xFFFF, 0, chunk))
        i = j
    lines.append(":00000001FF")
    open(path, "w").write("\n".join(lines) + "\n")


def span(mem):
    return f"0x{min(mem):08X}..0x{max(mem):08X} ({len(mem)} bytes)"


sd_path, app_path, out_path = sys.argv[1:4]
sd, app = parse(sd_path), parse(app_path)
print("softdevice:", span(sd))
print("app       :", span(app))
overlap = set(sd) & set(app)
if overlap:
    raise SystemExit(f"refusing to merge: {len(overlap)} overlapping bytes, e.g. 0x{min(overlap):X}")
sd.update(app)
write(sd, out_path)
print("merged    :", span(sd), "->", out_path)
