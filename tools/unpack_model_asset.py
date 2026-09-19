#!/usr/bin/env python3
"""Reconstitute a checked-in, losslessly compressed GLB model source.

The GitHub file connector currently accepts UTF-8 text only. This generic,
deterministic host-build adapter keeps a user-authorized binary GLB as small
base64/zlib text parts; model conversion *still* runs through the repository's
existing tools/import_model.py with its silhouette/animation quality gates.
No network, external assets or hidden fallback meshes are involved.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
from pathlib import Path
import sys
import zlib

def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--prefix", required=True)
    p.add_argument("--parts", type=int, required=True)
    p.add_argument("--sha256", required=True)
    p.add_argument("--out", required=True)
    args = p.parse_args()
    if args.parts < 1 or args.parts > 256:
        p.error("--parts must be in 1..256")
    joined = []
    for i in range(1, args.parts+1):
        path = Path(f"{args.prefix}{i:02d}")
        if not path.is_file():
            p.error(f"model source part missing: {path}")
        chunk = path.read_text(encoding="ascii").strip()
        if not chunk:
            p.error(f"model source part empty: {path}")
        joined.append(chunk)
    try:
        compressed = base64.b64decode("".join(joined), validate=True)
        payload = zlib.decompress(compressed)
    except (ValueError, zlib.error) as exc:
        p.error(f"source archive could not be decoded: {exc}")
    got = hashlib.sha256(payload).hexdigest()
    if got.lower() != args.sha256.lower():
        p.error(f"wrong model source checksum: got {got}, expected {args.sha256}")
    if len(payload)<12 or payload[:4]!=b"glTF" or int.from_bytes(payload[8:12],"little")!=len(payload):
        p.error("source is not a valid-size GLB header")
    out=Path(args.out)
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_bytes(payload)
    print(f"model source: {out} bytes={len(payload)} sha256={got}")
    return 0

if __name__=="__main__":
    sys.exit(main())
