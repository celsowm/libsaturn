#!/usr/bin/env python3
"""SLZ1: the LZ format sat_lz_decode / sat_load_lz / sat_cart_overlay_load_lz read (saturn/lz.h).

    python satlz.py IN OUT          # compress IN into OUT
    python satlz.py -d IN OUT       # decompress (reference decoder)
    python satlz.py --out-dir DIR IN...   # each IN to DIR/<name>, skipping unchanged inputs

Disc data that compresses well (tile maps, sprite tiles) loads in less time: the CD drive reads
~300 KB/s and the SH-2 decodes several times faster. The decoder runs IN PLACE: the file is read to
the end of the destination and decoded forwards over it, so a load needs no buffer beyond
`decoded + margin` bytes (the header says how much margin).

Layout (all words big-endian):
    0  "SLZ1"
    4  u32 decoded size
    8  u32 margin: a destination of at least decoded + margin bytes decodes in place
   12  u32 payload size (bytes after this 16-byte header)
   16  sequences, LZ4-style:
         token: high nibble literal count L, low nibble match length M - 4
         L == 15: add following bytes until one is not 255; then L literal bytes
         end of payload here: done (the last sequence has literals only)
         u16 offset (1..65535) back from the output position
         M == 15: add following bytes as for L; copy M + 4 bytes (may overlap)
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

MAGIC = b"SLZ1"
HEADER = 16
MIN_MATCH = 4
WINDOW = 0xFFFF
CHAIN = 24  # match candidates tried per position


def _length(out: bytearray, n: int) -> None:
    while n >= 255:
        out.append(255)
        n -= 255
    out.append(n)


def _sequence(out: bytearray, data: bytes, lit_start: int, lit_end: int, offset: int, length: int) -> None:
    lits = lit_end - lit_start
    m = length - MIN_MATCH if length else 0
    out.append((min(lits, 15) << 4) | (min(m, 15) if length else 0))
    if lits >= 15:
        _length(out, lits - 15)
    out += data[lit_start:lit_end]
    if length:
        out += struct.pack(">H", offset)
        if m >= 15:
            _length(out, m - 15)


def compress_payload(data: bytes) -> bytes:
    n = len(data)
    out = bytearray()
    heads: dict[bytes, list[int]] = {}
    i = lit = 0
    limit = n - MIN_MATCH  # a match may not start past here
    while i <= limit:
        key = data[i:i + 4]
        chain = heads.get(key)
        best_len = best_off = 0
        if chain:
            for j in reversed(chain[-CHAIN:]):
                off = i - j
                if off > WINDOW:
                    break
                k = 4
                while i + k < n and data[j + k] == data[i + k]:
                    k += 1
                if k > best_len:
                    best_len, best_off = k, off
        if best_len >= MIN_MATCH:
            _sequence(out, data, lit, i, best_off, best_len)
            end = i + best_len
            # index the matched positions too (cheap: only every position's 4-byte key)
            while i < end:
                if i <= limit:
                    heads.setdefault(data[i:i + 4], []).append(i)
                i += 1
            lit = i
        else:
            if chain is None:
                heads[key] = [i]
            else:
                chain.append(i)
                if len(chain) > 4 * CHAIN:
                    del chain[:-CHAIN]
            i += 1
    _sequence(out, data, lit, n, 0, 0)
    return bytes(out)


def in_place_margin(payload: bytes, decoded: int) -> int:
    """Smallest margin with which decoding over the file placed at the end of the destination never
    writes a byte the decoder has not read yet (see sat_load_lz: the file starts at
    align_down(capacity - file_size, 4))."""
    file_size = HEADER + len(payload)
    need = 0  # smallest start offset of the payload, relative to the destination
    i = o = 0
    end = len(payload)
    while True:
        t = payload[i]
        i += 1
        lits = t >> 4
        if lits == 15:
            while True:
                b = payload[i]
                i += 1
                lits += b
                if b != 255:
                    break
        # literal k is written at o + k after reading payload byte i + k: needs o + k < start + i + k + 1
        if lits:
            need = max(need, o - i)
        o += lits
        i += lits
        if i >= end:
            break
        i += 2
        m = (t & 15)
        if m == 15:
            while True:
                b = payload[i]
                i += 1
                m += b
                if b != 255:
                    break
        m += MIN_MATCH
        # the match writes o .. o + m - 1 with the next unread byte at i
        need = max(need, o + m - i)
        o += m
    assert o == decoded
    # payload starts at off + HEADER, off >= capacity - file_size - 3 >= decoded + margin - file_size - 3
    margin = need - HEADER + file_size + 3 - decoded
    return max(margin, file_size - decoded, 0)


def compress(data: bytes) -> bytes:
    payload = compress_payload(data)
    margin = in_place_margin(payload, len(data))
    return MAGIC + struct.pack(">III", len(data), margin, len(payload)) + payload


def decompress(blob: bytes) -> bytes:
    if blob[:4] != MAGIC:
        raise ValueError("not an SLZ1 file")
    decoded, _margin, size = struct.unpack(">III", blob[4:16])
    payload = blob[HEADER:HEADER + size]
    out = bytearray()
    i = 0
    while True:
        t = payload[i]
        i += 1
        lits = t >> 4
        if lits == 15:
            while True:
                b = payload[i]
                i += 1
                lits += b
                if b != 255:
                    break
        out += payload[i:i + lits]
        i += lits
        if i >= size:
            break
        off = (payload[i] << 8) | payload[i + 1]
        i += 2
        m = t & 15
        if m == 15:
            while True:
                b = payload[i]
                i += 1
                m += b
                if b != 255:
                    break
        m += MIN_MATCH
        if off == 0 or off > len(out):
            raise ValueError("bad match offset")
        for _ in range(m):
            out.append(out[-off])
    if len(out) != decoded:
        raise ValueError("decoded size mismatch")
    return bytes(out)


def compress_many(inputs: list[str], out_dir: str) -> int:
    """Compresses each input to out_dir/<same name>. An output whose input is unchanged since it
    was made (sha256 kept in out_dir/.satlz-cache) is left alone, so a build can run this on every
    file each time. Returns how many files were compressed."""
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    cache_file = out / ".satlz-cache"
    cache: dict[str, str] = {}
    if cache_file.exists():
        for line in cache_file.read_text(encoding="utf-8").splitlines():
            name, _, digest = line.partition(" ")
            cache[name] = digest
    done = 0
    for name in inputs:
        src = Path(name)
        data = src.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        dst = out / src.name
        if cache.get(src.name) == digest and dst.exists():
            continue
        dst.write_bytes(compress(data))
        cache[src.name] = digest
        done += 1
    cache_file.write_text("".join(f"{k} {v}\n" for k, v in sorted(cache.items())), encoding="utf-8")
    return done


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n\n")[0])
    ap.add_argument("-d", "--decompress", action="store_true", help="IN OUT: decode (reference decoder)")
    ap.add_argument("--out-dir", help="compress every FILE to OUT_DIR/<its name> (skips unchanged ones)")
    ap.add_argument("files", nargs="+", metavar="FILE", help="IN OUT, or the inputs with --out-dir")
    args = ap.parse_args(argv)
    if args.out_dir:
        if args.decompress:
            ap.error("--out-dir compresses only")
        compress_many(args.files, args.out_dir)
        return 0
    if len(args.files) != 2:
        ap.error("expected IN OUT")
    data = Path(args.files[0]).read_bytes()
    Path(args.files[1]).write_bytes(decompress(data) if args.decompress else compress(data))
    return 0


if __name__ == "__main__":
    sys.exit(main())
