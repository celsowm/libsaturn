#!/usr/bin/env python3
"""Deterministic filesystem-safe key for the actual compiler/profile arguments."""
import hashlib
import sys

if len(sys.argv) < 2:
    raise SystemExit("build_variant_key.py requires profile arguments")
print(hashlib.sha256("\0".join(sys.argv[1:]).encode("utf-8")).hexdigest()[:16])
