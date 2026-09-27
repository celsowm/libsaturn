"""Draco-compressed GLB -> plain GLB, cached by content hash.

city_walk vendors the 6.6 MB Draco-compressed asset. tools/model_pipeline/gltf.py
reads plain accessors only, so the source is decoded once with the glTF
Transform CLI (measured: 3.9 s, 6.6 -> 23.7 MB) and the result is cached under
build/generated/. A GLB that is not Draco-compressed is returned untouched.
"""

from __future__ import annotations

import hashlib
import json
import shutil
import struct
import subprocess
from pathlib import Path

from .gltf import GltfError

CLI_PACKAGE = "@gltf-transform/cli@4"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def uses_draco(path: Path) -> bool:
    """True when the GLB's JSON chunk lists KHR_draco_mesh_compression."""
    with open(path, "rb") as stream:
        head = stream.read(20)
        if len(head) < 20 or head[:4] != b"glTF":
            raise GltfError(f"{path}: not a binary glTF")
        json_len = struct.unpack_from("<I", head, 12)[0]
        doc = json.loads(stream.read(json_len))
    listed = set(doc.get("extensionsRequired", [])) | set(doc.get("extensionsUsed", []))
    return "KHR_draco_mesh_compression" in listed


def decode_to_plain_glb(source: Path, cache_dir: Path) -> Path:
    """Returns a GLB gltf.parse_glb can read, decoding Draco when needed."""
    source = Path(source)
    if not source.exists():
        raise GltfError(f"source GLB not found: {source}")
    if not uses_draco(source):
        return source
    cache_dir = Path(cache_dir)
    cache_dir.mkdir(parents=True, exist_ok=True)
    target = cache_dir / f"plain_{sha256_file(source)[:16]}.glb"
    if target.exists():
        return target
    npx = shutil.which("npx") or shutil.which("npx.cmd")
    if npx is None:
        raise GltfError(
            f"{source.name} is Draco-compressed and npx was not found. Install Node.js, or "
            f"decode it yourself: npx --yes {CLI_PACKAGE} copy {source} {target}")
    partial = target.with_name(target.stem + ".part.glb")
    # `copy` re-writes the document, and the CLI decodes Draco on read.
    result = subprocess.run(
        [npx, "--yes", CLI_PACKAGE, "copy", str(source), str(partial)],
        capture_output=True, text=True)
    if result.returncode != 0 or not partial.exists():
        raise GltfError(
            f"Draco decode failed ({result.returncode}): {result.stderr.strip() or result.stdout.strip()}\n"
            f"Run by hand to see why: npx --yes {CLI_PACKAGE} copy {source} {target}")
    if uses_draco(partial):
        raise GltfError(f"{partial}: still Draco-compressed after decoding")
    partial.replace(target)
    return target
