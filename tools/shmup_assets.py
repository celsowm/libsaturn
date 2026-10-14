#!/usr/bin/env python3
"""Shmup sprite pack: quality gate, import and packing into a SATL atlas blob.

    shmup_assets.py validate <pack dir | pack zip>      reject defective art (exit 1)
    shmup_assets.py import   <pack dir | zip> <out dir> copy the validated runtime subset
    shmup_assets.py pack     <asset dir> --out DIR      write SHMUP.ATL + generated header

The pack format is the one of shmup_saturn_assets_v4_COMPLETO (manifest.json,
sprites_rgba/, vdp1/index8/, vdp1/lut4/). The older v3 ship pack (atlas/ships_atlas.json) is
read too, so that its defects are *reported* instead of silently accepted: it clips wings at
the crop edge, carries fragments of neighbouring ships and a 1..253 alpha halo.

The gate is a hard fail: a sprite with an opaque pixel inside the 2 px border (clipped), a
non-binary alpha value (halo), a stray connected component (fragment), an off-centre pivot, a
width that is not a multiple of 8 (VDP1 character pattern rule) or a raw VDP1 file that disagrees
with its PNG stops the build. Colour quantisation loss is measured and held to a budget.

SATL blob (big-endian, 2048-byte aligned group data so a stage group can be read from disc by
offset while the previous stage runs; see include/saturn/sprite_atlas.h):

    header 64 bytes   'SATL' u16 version, u16 flags, u16 frames, u16 luts, u16 groups,
                      u16 palette_entries, u32 frame_off, lut_off, palette_off, group_off,
                      total_bytes, crc32 (over bytes 64..end), u16 orientation
    frame  32 bytes   u16 w, h; i16 pivot_x, pivot_y; u16 format (0 INDEX8, 1 LUT4);
                      u16 lut_index; u32 data_off (from the start of the file); u32 data_bytes;
                      u16 group; u16 radius; u16 category; u16 reserved; u32 content_hash
    lut    32 bytes   16 BGR555 words (entry 0 transparent, visible entries have bit 15)
    group  16 bytes   u32 data_off, u32 data_bytes, u16 first_frame, u16 frame_count, u32 pad
"""
from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import re
import shutil
import struct
import sys
import tempfile
import zipfile
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

MIN_BORDER = 2
PIVOT_TOLERANCE = 3
INDEX8_ERROR_BUDGET = 16.0   # mean abs RGB error (0..255) after palette quantisation
LUT4_ERROR_BUDGET = 40.0
SECTOR = 2048
HEADER_BYTES = 64
FRAME_BYTES = 32
GROUP_BYTES = 16
VERSION = 1
FORMAT_INDEX8, FORMAT_LUT4 = 0, 1
ORIENTATIONS = {"none": 0, "cw": 1, "ccw": 2}

# Categories whose sprites must be one connected shape (8-connected, alpha > 0).
SINGLE_COMPONENT = {"ships", "enemies", "bosses", "bullets", "player_weapons", "pickups"}
# Sprites that are legitimately made of several shapes: name pattern -> component count.
ALLOWED_COMPONENTS = {"pickup_shield_*": 2}
CATEGORY_IDS = {"ships": 0, "enemies": 1, "bosses": 2, "bullets": 3, "explosions": 4,
                "pickups": 5, "player_weapons": 6, "hud": 7}


class Issue:
    def __init__(self, name: str, kind: str, detail: str, fatal: bool = True):
        self.name, self.kind, self.detail, self.fatal = name, kind, detail, fatal

    def __str__(self) -> str:
        return f"{'ERROR' if self.fatal else 'warn '} {self.name}: {self.kind} ({self.detail})"


# --------------------------------------------------------------------------- pack access

def open_pack(source: Path) -> tuple[Path, tempfile.TemporaryDirectory | None]:
    """A pack directory, or a zip extracted into a private temp dir."""
    if source.is_dir():
        return find_pack_root(source), None
    if zipfile.is_zipfile(source):
        tmp = tempfile.TemporaryDirectory(prefix="shmup_pack_")
        with zipfile.ZipFile(source) as archive:
            archive.extractall(tmp.name)
        return find_pack_root(Path(tmp.name)), tmp
    raise SystemExit(f"not a directory or zip: {source}")


def find_pack_root(path: Path) -> Path:
    for candidate in [path, *sorted(path.iterdir())]:
        if candidate.is_dir() and ((candidate / "manifest.json").exists()
                                   or (candidate / "atlas/ships_atlas.json").exists()):
            return candidate
    raise SystemExit(f"no manifest.json or atlas/ships_atlas.json under {path}")


class Record:
    def __init__(self, name, category, rgba, pivot, radius, index8=None, lut4=None, w=None, h=None):
        self.name, self.category, self.rgba = name, category, rgba
        self.pivot, self.radius, self.index8, self.lut4 = pivot, radius, index8, lut4
        self.w, self.h = w, h


def load_records(root: Path) -> tuple[list[Record], dict]:
    """Records of a v4 pack/subset (manifest.json) or of the old v3 ship pack."""
    meta: dict = {"root": str(root)}
    manifest = root / "manifest.json"
    records = []
    if manifest.exists():
        data = json.loads(manifest.read_text(encoding="utf-8"))
        meta["manifest"] = data
        for name, r in data["sprite_records"].items():
            lut = r.get("vdp1_lut4")
            records.append(Record(
                name, r["category"], root / r["rgba"], (r["pivot"]["x"], r["pivot"]["y"]),
                r.get("collision_suggested_radius", 0),
                index8=root / r["vdp1_index8"] if "vdp1_index8" in r else None,
                lut4=(root / lut["data"], root / lut["palette"]) if lut else None,
                w=r["w"], h=r["h"]))
        meta["palette"] = root / data["native"]["indexed8_palette"]
        return records, meta
    v3 = json.loads((root / "atlas/ships_atlas.json").read_text(encoding="utf-8"))
    meta["legacy_v3"] = True
    for name, a in v3["assets"].items():
        w, h = a["size"]
        records.append(Record(name, "ships", root / a["rgba_png"], (w // 2, h // 2), 0, w=w, h=h))
    return records, meta


def load_any(root: Path) -> tuple[list[Record], dict]:
    """v4 pack, imported subset (schema 1) or legacy v3 ship pack."""
    manifest = root / "manifest.json"
    if manifest.exists():
        data = json.loads(manifest.read_text(encoding="utf-8"))
        return load_subset(root) if data.get("schema") == 1 else load_records(root)
    return load_records(root)


def read_rgba(path: Path) -> np.ndarray:
    return np.array(Image.open(path).convert("RGBA"), dtype=np.uint8)


def components(mask: np.ndarray) -> list[int]:
    """Sizes of the 8-connected components of a boolean mask (iterative flood fill)."""
    h, w = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    sizes = []
    for y0, x0 in zip(*np.nonzero(mask)):
        if seen[y0, x0]:
            continue
        stack, count = [(int(y0), int(x0))], 0
        seen[y0, x0] = True
        while stack:
            y, x = stack.pop()
            count += 1
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    ny, nx = y + dy, x + dx
                    if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = True
                        stack.append((ny, nx))
        sizes.append(count)
    return sorted(sizes, reverse=True)


def bgr555_to_rgb888(words: np.ndarray) -> np.ndarray:
    w = words.astype(np.int32)
    return np.stack([(w & 31) << 3, ((w >> 5) & 31) << 3, ((w >> 10) & 31) << 3], axis=-1)


# --------------------------------------------------------------------------- validation

def validate_records(records: list[Record], meta: dict) -> list[Issue]:
    issues: list[Issue] = []
    palette_words = None
    if meta.get("palette"):
        raw = Path(meta["palette"]).read_bytes()
        if len(raw) != 512:
            issues.append(Issue("palette", "length", f"{len(raw)} bytes, expected 512"))
        else:
            palette_words = np.frombuffer(raw, ">u2")
            if palette_words[0] != 0:
                issues.append(Issue("palette", "index 0", "entry 0 must be 0 (transparent)"))
    for r in records:
        issues += validate_record(r, palette_words)
    names = [r.name for r in records]
    if len(names) != len(set(names)):
        issues.append(Issue("pack", "duplicate names", "sprite names must be unique"))
    return issues


def validate_record(r: Record, palette_words) -> list[Issue]:
    out: list[Issue] = []
    if not r.rgba.exists():
        return [Issue(r.name, "missing file", str(r.rgba))]
    im = read_rgba(r.rgba)
    h, w = im.shape[:2]
    alpha = im[..., 3]
    if r.w is not None and (w, h) != (r.w, r.h):
        out.append(Issue(r.name, "size differs from manifest", f"{w}x{h} vs {r.w}x{r.h}"))
    if w % 8:
        out.append(Issue(r.name, "width not a multiple of 8", f"{w}"))
    if h > 255 or h < 1:
        out.append(Issue(r.name, "height outside 1..255", f"{h}"))
    odd = sorted(int(v) for v in np.unique(alpha) if v not in (0, 255))
    if odd:
        out.append(Issue(r.name, "non-binary alpha (halo)", f"{len(odd)} values, e.g. {odd[:4]}"))
    solid = alpha >= 128
    if not solid.any():
        return out + [Issue(r.name, "empty sprite", "no opaque pixel")]
    ys, xs = np.nonzero(solid)
    x0, y0, x1, y1 = int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1
    for side, gap in (("left", x0), ("top", y0), ("right", w - x1), ("bottom", h - y1)):
        if gap < MIN_BORDER:
            out.append(Issue(r.name, "opaque pixels inside the border (clipped)",
                             f"{side} gap {gap} px < {MIN_BORDER}"))
    expected = 1
    for pattern, count in ALLOWED_COMPONENTS.items():
        if fnmatch.fnmatch(r.name, pattern):
            expected = count
    if r.category in SINGLE_COMPONENT:
        sizes = components(alpha > 0)
        if len(sizes) != expected:
            out.append(Issue(r.name, "stray connected component (fragment)",
                             f"{len(sizes)} components, expected {expected}: {sizes[:5]}"))
    if r.category == "ships" and abs((x0 + x1) / 2 - r.pivot[0]) > PIVOT_TOLERANCE:
        out.append(Issue(r.name, "pivot off-centre",
                         f"art centre x {(x0 + x1) / 2:.1f} vs pivot {r.pivot[0]}"))
    elif r.category != "ships" and (abs((x0 + x1) / 2 - r.pivot[0]) > PIVOT_TOLERANCE
                                    or abs((y0 + y1) / 2 - r.pivot[1]) > PIVOT_TOLERANCE):
        out.append(Issue(r.name, "pivot off-centre",
                         f"art centre ({(x0 + x1) / 2:.1f},{(y0 + y1) / 2:.1f}) vs pivot {r.pivot}"))
    if r.index8 is not None:
        out += validate_index8(r, im, palette_words)
    if r.lut4 is not None:
        out += validate_lut4(r, im)
    return out


def validate_index8(r: Record, im: np.ndarray, palette_words) -> list[Issue]:
    h, w = im.shape[:2]
    assert r.index8 is not None
    if not r.index8.exists():
        return [Issue(r.name, "missing file", str(r.index8))]
    raw = np.frombuffer(r.index8.read_bytes(), np.uint8)
    if raw.size != w * h:
        return [Issue(r.name, "INDEX8 size", f"{raw.size} bytes, expected {w * h}")]
    raw = raw.reshape(h, w)
    out = []
    if ((raw == 0) != (im[..., 3] == 0)).any():
        out.append(Issue(r.name, "INDEX8 transparency differs from PNG", "index 0 vs alpha mismatch"))
    if (raw >= 254).any():
        out.append(Issue(r.name, "INDEX8 uses reserved index", "254/255 are reserved (hi-res shadow)"))
    if palette_words is not None and (im[..., 3] > 0).any():
        rgb = bgr555_to_rgb888(palette_words)[raw]
        err = float(np.abs(rgb - im[..., :3].astype(int)).mean(axis=2)[im[..., 3] > 0].mean())
        if err > INDEX8_ERROR_BUDGET:
            out.append(Issue(r.name, "INDEX8 quantisation error over budget",
                             f"{err:.1f} > {INDEX8_ERROR_BUDGET}"))
    return out


def validate_lut4(r: Record, im: np.ndarray) -> list[Issue]:
    h, w = im.shape[:2]
    assert r.lut4 is not None
    data_path, lut_path = r.lut4
    if not data_path.exists() or not lut_path.exists():
        return [Issue(r.name, "missing file", f"{data_path} / {lut_path}")]
    data, lut = data_path.read_bytes(), lut_path.read_bytes()
    out = []
    if len(data) != w * h // 2:
        return [Issue(r.name, "LUT4 size", f"{len(data)} bytes, expected {w * h // 2}")]
    if len(lut) != 32:
        return [Issue(r.name, "LUT4 table size", f"{len(lut)} bytes, expected 32")]
    words = struct.unpack(">16H", lut)
    if words[0] != 0 or any(not v & 0x8000 for v in words[1:]):
        out.append(Issue(r.name, "LUT4 table", "entry 0 must be 0 and visible entries need bit 15"))
    pix = lut4_unpack(data, w, h)
    if ((pix == 0) != (im[..., 3] == 0)).any():
        out.append(Issue(r.name, "LUT4 transparency differs from PNG", "code 0 vs alpha mismatch"))
    elif (im[..., 3] > 0).any():
        rgb = bgr555_to_rgb888(np.array(words, dtype=np.uint16))[pix]
        err = float(np.abs(rgb - im[..., :3].astype(int)).mean(axis=2)[im[..., 3] > 0].mean())
        if err > LUT4_ERROR_BUDGET:
            out.append(Issue(r.name, "LUT4 quantisation error over budget",
                             f"{err:.1f} > {LUT4_ERROR_BUDGET}"))
    return out


def lut4_unpack(data: bytes, w: int, h: int) -> np.ndarray:
    u = np.frombuffer(data, np.uint8)
    pix = np.empty(u.size * 2, np.uint8)
    pix[0::2], pix[1::2] = u >> 4, u & 15
    return pix.reshape(h, w)


def lut4_pack(pix: np.ndarray) -> bytes:
    flat = pix.reshape(-1)
    return ((flat[0::2] << 4) | flat[1::2]).astype(np.uint8).tobytes()


# --------------------------------------------------------------------------- import

def import_pack(source: Path, out_dir: Path) -> list[Issue]:
    root, tmp = open_pack(source)
    try:
        records, meta = load_records(root)
        issues = validate_records(records, meta)
        if any(i.fatal for i in issues):
            return issues
        if out_dir.exists():
            shutil.rmtree(out_dir)
        manifest = meta["manifest"]
        keep: dict = {}
        for r in records:
            rec = manifest["sprite_records"][r.name]
            entry = {k: rec[k] for k in ("category", "w", "h", "pivot", "collision_suggested_radius")}
            copies = [("rgba", r.rgba), ("vdp1_index8", r.index8)]
            if r.lut4:
                copies += [("lut4_data", r.lut4[0]), ("lut4_palette", r.lut4[1])]
            for key, src in copies:
                rel = src.relative_to(root)
                (out_dir / rel).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(src, out_dir / rel)
                entry[key] = rel.as_posix()
            keep[r.name] = entry
        shutil.copyfile(meta["palette"], out_dir / "palette_bgr555_be.bin")
        for sub in ("vdp2/backgrounds", "vdp2/tiles"):
            if (root / sub).is_dir():
                shutil.copytree(root / sub, out_dir / sub)
        blob = {"schema": 1, "package": manifest["package"], "version": manifest["version"],
                "license_notice": "Original/procedural artwork; not extracted from any arcade game.",
                "palette": "palette_bgr555_be.bin", "sprite_records": keep}
        (out_dir / "manifest.json").write_text(json.dumps(blob, indent=1, sort_keys=True) + "\n",
                                               encoding="utf-8")
        return issues
    finally:
        if tmp:
            tmp.cleanup()


def load_subset(root: Path) -> tuple[list[Record], dict]:
    """Records of a directory produced by `import` (flat manifest schema 1)."""
    data = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    records = []
    for name, r in data["sprite_records"].items():
        lut = (root / r["lut4_data"], root / r["lut4_palette"]) if "lut4_data" in r else None
        records.append(Record(name, r["category"], root / r["rgba"], (r["pivot"]["x"], r["pivot"]["y"]),
                              r["collision_suggested_radius"], index8=root / r["vdp1_index8"],
                              lut4=lut, w=r["w"], h=r["h"]))
    return records, {"palette": root / data["palette"], "manifest": data, "root": str(root)}


# --------------------------------------------------------------------------- pack

def rotate(pix: np.ndarray, orientation: str) -> np.ndarray:
    if orientation == "cw":
        return np.rot90(pix, -1)
    if orientation == "ccw":
        return np.rot90(pix, 1)
    return pix


def rotate_point(x: int, y: int, w: int, h: int, orientation: str) -> tuple[int, int]:
    if orientation == "cw":      # (x, y) in WxH -> (h - 1 - y, x) in HxW, pixel-corner coordinates
        return h - y, x
    if orientation == "ccw":
        return y, w - x
    return x, y


def trim(index: np.ndarray, pivot: tuple[int, int]) -> tuple[np.ndarray, tuple[int, int]]:
    """Cuts transparent padding (the atlas gutter is for the sheet, not for VDP1 fill time)
    down to the opaque box, widened to a multiple of 8 inside the original sprite."""
    h, w = index.shape
    ys, xs = np.nonzero(index)
    x0, x1, y0, y1 = int(xs.min()), int(xs.max()) + 1, int(ys.min()), int(ys.max()) + 1
    width = max(8, (x1 - x0 + 7) // 8 * 8)
    left = max(0, min(x0 - (width - (x1 - x0)) // 2, w - width))
    if width > w:
        return index, pivot
    out = index[y0:y1, left:left + width]
    assert (out != 0).sum() == (index != 0).sum(), "trim removed opaque pixels"
    return np.ascontiguousarray(out), (pivot[0] - left, pivot[1] - y0)


def group_of(name: str, groups: list[tuple[str, int]]) -> int:
    for pattern, gid in groups:
        if fnmatch.fnmatch(name, pattern):
            return gid
    return 0


def pack_blob(records: list[Record], meta: dict, profile: str, orientation: str,
              groups: list[tuple[str, int]]) -> tuple[bytes, dict]:
    palette = bytes(Path(meta["palette"]).read_bytes())
    luts: list[bytes] = []
    frames = []
    for r in sorted(records, key=lambda r: (group_of(r.name, groups), r.name)):
        assert r.index8 is not None and r.w and r.h
        index = np.frombuffer(r.index8.read_bytes(), np.uint8).reshape(r.h, r.w)
        use_lut = profile == "lut4" and r.lut4 is not None
        if use_lut:
            assert r.lut4 is not None
            pix = lut4_unpack(r.lut4[0].read_bytes(), r.w, r.h)
            lut = r.lut4[1].read_bytes()
            if lut not in luts:
                luts.append(lut)
            lut_index = luts.index(lut)
        else:
            pix, lut_index = index, 0
        pix = rotate(pix, orientation)
        pivot = rotate_point(r.pivot[0], r.pivot[1], r.w, r.h, orientation)
        pix, pivot = trim(pix, pivot)
        h, w = pix.shape
        data = lut4_pack(pix) if use_lut else pix.tobytes()
        if use_lut and w % 8:
            raise SystemExit(f"{r.name}: LUT4 width {w} not a multiple of 8")
        frames.append(dict(name=r.name, w=w, h=h, pivot=pivot, fmt=FORMAT_LUT4 if use_lut else FORMAT_INDEX8,
                           lut=lut_index, data=data, group=group_of(r.name, groups), radius=r.radius,
                           category=CATEGORY_IDS[r.category], hash=zlib.crc32(data) ^ (w << 24) ^ (h << 16)))
    n_groups = max(f["group"] for f in frames) + 1
    frame_off = HEADER_BYTES
    lut_off = frame_off + FRAME_BYTES * len(frames)
    palette_off = lut_off + 32 * len(luts)
    group_off = palette_off + len(palette)
    cursor = (group_off + GROUP_BYTES * n_groups + SECTOR - 1) // SECTOR * SECTOR
    group_table, data_blob, shared = [], bytearray(), {}
    for g in range(n_groups):
        start = cursor + len(data_blob)
        first = next((i for i, f in enumerate(frames) if f["group"] == g), 0)
        count = sum(1 for f in frames if f["group"] == g)
        local = bytearray()
        for f in (f for f in frames if f["group"] == g):
            # Identical pixel data is stored once per residency group, never across
            # groups: a group must be loadable without any other being resident.
            key = (g, f["fmt"], f["data"])
            if key not in shared:
                shared[key] = start + len(local)
                local += f["data"] + b"\0" * (-len(f["data"]) % 8)   # VDP1 character addresses are /8
            f["off"] = shared[key]
        local += b"\0" * (-len(local) % SECTOR)
        group_table.append((start, len(local), first, count))
        data_blob += local
    total = cursor + len(data_blob)
    body = bytearray()
    for f in frames:
        body += struct.pack(">HHhhHHIIHHHHI", f["w"], f["h"], f["pivot"][0], f["pivot"][1], f["fmt"],
                            f["lut"], f["off"], len(f["data"]), f["group"], f["radius"], f["category"],
                            0, f["hash"] & 0xFFFFFFFF)
    for lut in luts:
        body += lut
    body += palette
    for start, size, first, count in group_table:
        body += struct.pack(">IIHHI", start, size, first, count, 0)
    body += b"\0" * (cursor - HEADER_BYTES - len(body))
    body += data_blob
    crc = zlib.crc32(bytes(body)) & 0xFFFFFFFF
    header = struct.pack(">4sHHHHHHIIIIIIH", b"SATL", VERSION, 0, len(frames), len(luts), n_groups,
                         len(palette) // 2, frame_off, lut_off, palette_off, group_off, total, crc,
                         ORIENTATIONS[orientation])
    blob = header + b"\0" * (HEADER_BYTES - len(header)) + bytes(body)
    assert len(blob) == total
    report = {"profile": profile, "orientation": orientation, "frames": len(frames), "luts": len(luts),
              "groups": [{"data_off": s, "data_bytes": b, "first": fi, "count": c}
                         for s, b, fi, c in group_table],
              "unique_pixel_bytes": sum(len(k[2]) for k in shared),
              "sum_frame_bytes": sum(len(f["data"]) for f in frames),
              "bytes": total, "crc32": f"{crc:08x}", "sha256": hashlib.sha256(blob).hexdigest(),
              "vdp1_pixels": sum(f["w"] * f["h"] for f in frames),
              "untrimmed_pixels": sum(r.w * r.h for r in records),
              "frame_list": [{k: f[k] for k in ("name", "w", "h", "pivot", "fmt", "group", "radius")}
                             for f in frames]}
    return blob, report


def emit_header(report: dict, path: Path, prefix: str) -> None:
    lines = ["/* Generated by tools/shmup_assets.py pack. Do not edit. */",
             f"#ifndef {prefix}_IDS_H", f"#define {prefix}_IDS_H", ""]
    for i, f in enumerate(report["frame_list"]):
        lines.append(f"#define {prefix}_{re.sub('[^0-9A-Za-z]+', '_', f['name']).upper()} {i}u")
    lines += ["", f"#define {prefix}_FRAME_COUNT {len(report['frame_list'])}u",
              f"#define {prefix}_GROUP_COUNT {len(report['groups'])}u", "", "#endif", ""]
    path.write_text("\n".join(lines), encoding="utf-8")


def parse_groups(spec: list[str]) -> list[tuple[str, int]]:
    out = []
    for item in spec:
        pattern, _, gid = item.rpartition("=")
        out.append((pattern, int(gid)))
    return out


# --------------------------------------------------------------------------- CLI

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    v = sub.add_parser("validate"); v.add_argument("source", type=Path)
    i = sub.add_parser("import"); i.add_argument("source", type=Path); i.add_argument("out", type=Path)
    p = sub.add_parser("pack"); p.add_argument("assets", type=Path)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--name", default="SHMUP")
    p.add_argument("--profile", choices=("lut4", "hires"), default="lut4",
                   help="lut4: LUT4 where the pack has it; hires: INDEX8 only (8 bpp video modes)")
    p.add_argument("--orientation", choices=tuple(ORIENTATIONS), default="none")
    p.add_argument("--group", action="append", default=[], metavar="GLOB=ID",
                   help="residency group of matching sprite names (first match wins, default 0)")
    a = ap.parse_args(argv)

    if a.cmd == "validate":
        root, tmp = open_pack(a.source)
        try:
            records, meta = load_any(root)
            issues = validate_records(records, meta)
        finally:
            if tmp:
                tmp.cleanup()
        for issue in issues:
            print(issue)
        fatal = [x for x in issues if x.fatal]
        print(f"[shmup-assets] {len(records)} sprites, {len(fatal)} defects -> {'REJECTED' if fatal else 'OK'}")
        return 1 if fatal else 0
    if a.cmd == "import":
        issues = import_pack(a.source, a.out)
        for issue in issues:
            print(issue)
        if any(x.fatal for x in issues):
            print("[shmup-assets] pack REJECTED, nothing imported")
            return 1
        print(f"[shmup-assets] imported to {a.out}")
        return 0
    records, meta = load_subset(a.assets)
    issues = [x for x in validate_records(records, meta) if x.fatal]
    for issue in issues:
        print(issue)
    if issues:
        print("[shmup-assets] assets REJECTED, nothing packed")
        return 1
    blob, report = pack_blob(records, meta, a.profile, a.orientation, parse_groups(a.group))
    a.out.mkdir(parents=True, exist_ok=True)
    (a.out / f"{a.name}.ATL").write_bytes(blob)
    (a.out / f"{a.name.lower()}_atlas_report.json").write_text(json.dumps(report, indent=1) + "\n")
    emit_header(report, a.out / f"{a.name.lower()}_atlas_ids.h", a.name.upper())
    print(f"[shmup-assets] {a.name}.ATL {len(blob)} bytes, {report['frames']} frames, "
          f"{report['unique_pixel_bytes']} unique pixel bytes (of {report['sum_frame_bytes']}), "
          f"crc32 {report['crc32']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
