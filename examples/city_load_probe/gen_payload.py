"""Writes the deterministic 1 MiB payload city_load_probe streams off the disc.

Word k (big-endian) is (k * 40503 + (k >> 7)) & 0xFFFF, so a wrong sector, a
duplicated span or a swapped half-word all show up as mismatches on read-back.
"""
import struct
import sys

WORDS = 512 * 1024
with open(sys.argv[1], "wb") as stream:
    stream.write(b"".join(
        struct.pack(">H", (k * 40503 + (k >> 7)) & 0xFFFF) for k in range(WORDS)))
