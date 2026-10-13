"""Shared helpers for harness/tests/*.py telemetry loading.

Every harness pytest reads the same shape from a Ymir WRAM dump:
  * find the example's `volatile results struct` symbol in the .map
  * convert its absolute address into a Work-RAM offset
  * read the desired number of bytes from the dump
  * big-endian-unpack the field list and return a dict

This module owns the constants (WRAM_HIGH, the symbol regex) and the
single load_dump helper. Tests that need structured shapes call
load_dump(...) and apply their own field naming on top.

The C-side counterpart (a sat_tel_init / sat_tel_set API in
include/saturn/telemetry.h) is a separate, larger refactor and is not
introduced here.
"""

import os
import re
import struct


#: Base address of Work RAM High on the Sega Saturn. Maps every
#: example's telemetry struct (which always lives there, see
#: docstring of sat_panic in include/saturn/core.h).
WRAM_HIGH = 0x06000000

#: Default regex for finding a global symbol in a GNU ld .map file.
#: Match an address followed by whitespace and the symbol name. Both
#: the address and the symbol allow leading whitespace so columns
#: align with cross-link output. Groups: 1 = address, 2 = name.
_DEFAULT_SYMBOL_RE = r"(?m)^\s*0x([0-9A-Fa-f]+)\s+(\S+)\s*$"


def find_symbol(map_path, name, pattern=None):
    """Return the Work-RAM byte offset of `name` in the link map.

    Raises AssertionError when the symbol is missing or its address
    falls outside Work RAM High, since either case means the harness
    is reading the wrong dump. Tests catch the AssertionError and
    fail cleanly.
    """
    pattern = pattern or _DEFAULT_SYMBOL_RE
    if not os.path.exists(map_path):
        raise AssertionError("map file is absent: " + map_path)
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(pattern, stream.read())
    if not found or found.group(2) != name:
        raise AssertionError("%s is absent from %s" % (name, map_path))
    address = int(found.group(1), 16)
    if address < WRAM_HIGH:
        raise AssertionError(
            "%s resolves to 0x%08x, outside Work RAM High (0x%08x)"
            % (name, address, WRAM_HIGH))
    return address - WRAM_HIGH


def load_dump(dump_path, map_path, symbol_name, format_string, count=None, offset=0):
    """Return the struct.unpack(...) tuple for `symbol_name`.

    Args:
      dump_path: path to the Ymir-written Work RAM dump.
      map_path: path to the example's .map file.
      symbol_name: name of the global telemetry struct.
      format_string: a struct format (no leading '='/'!'). Either a
        small endian ('<') or big endian ('>') prefix may be passed
        -- most tests use '>' because the SH-2 emits big-endian words.
      count: how many bytes to read. If None, compute from
        struct.calcsize(format_string).
      offset: extra byte offset from the symbol's start. Use it for a
        second struct.unpack on a struct that holds two regions
        (header + a body of a different element width, for example).

    Returns: a tuple of the unpacked fields.
    """
    if not os.path.exists(dump_path):
        raise AssertionError("dump file is absent: " + dump_path)
    base = find_symbol(map_path, symbol_name)
    byte_size = count if count is not None else struct.calcsize(format_string)
    with open(dump_path, "rb") as stream:
        image = stream.read()[base + offset:base + offset + byte_size]
    if len(image) < byte_size:
        raise AssertionError(
            "dump is short: requested %d bytes at offset 0x%x from %s"
            % (byte_size, base + offset, dump_path))
    return struct.unpack(format_string, image[:byte_size])


def verify_fields(values, expected, key_format=None):
    """Assert that every key in `expected` matches `values[key]`.

    Accepts either a tuple index or a string key in `values`
    (key_format is unused for tuples). Extra keys in `values` are
    ignored so tests can pass the full tuple and only assert on the
    fields they care about.
    """
    for k, want in expected.items():
        if isinstance(k, int):
            got = values[k] if k < len(values) else None
        else:
            got = values.get(k)
        if got != want:
            raise AssertionError(
                "field %r: want %r, got %r" % (k, want, got))
