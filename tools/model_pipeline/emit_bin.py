"""CITY.BIN packer for examples/city_walk (big-endian, pointer-free).

The layout is defined by examples/city_walk/city_format.h; this file writes it
and tests/test_city_chunker.py reads it back with an independent struct-based
parser, so neither side can drift silently.

    header(128) | materials | TOC(16 * 256 * 3) | ground | palette | texture palette | blobs

Version 2 added facade textures. Version 3 uses the texture-entry reserved word for CUTOUT/BILLBOARD flags. A blob may be followed (32-byte aligned, same
2 MiB bank) by its texture block, whose size the TOC entry carries:

    u16 count, u16 0 | count x {u16 width, u16 height, u16 texel_offset / 8, u16 0}
    | texels (INDEXED8, each texture 8-byte aligned, offsets from the block start)

The runtime copies the block as is into a VDP1 VRAM slot, so a texture's
character address is (slot_base + texel_offset) / 8. Byte 9 of a face is its
texture index + 1 (0 = solid colour from the material byte).
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass, field

from .gltf import GltfError

MAGIC = 0x43545931  # "CTY1"
BLOB_MAGIC = 0x43484E4B  # "CHNK"
VERSION = 3
HEADER_BYTES = 128
TOC_ENTRY_BYTES = 16
TEXTURE_ENTRY_BYTES = 8
TEXTURE_TABLE_HEADER = 4
TEXTURE_MAX_WIDTH = 504  # VDP1: width a multiple of 8, at most 504
TEXTURE_MAX_HEIGHT = 255
TEXTURE_FLAG_CUTOUT = 0x0001
TEXTURE_FLAG_BILLBOARD = 0x0002
TEXTURE_FLAG_MASK = TEXTURE_FLAG_CUTOUT | TEXTURE_FLAG_BILLBOARD
GRID_X = 16
GRID_Z = 16
CHUNK_COUNT = GRID_X * GRID_Z
LOD_COUNT = 3
CHUNK_UNITS = 32
QUANT_SCALE = 64
ORIGIN_X_UNITS = -128
ORIGIN_Z_UNITS = -320
ALIGN = 32
BANK_BYTES = 2 * 1024 * 1024
MATERIAL_MAX = 240
BLOB_HEADER_BYTES = 32
VERTEX_BYTES = 6
FACE_BYTES = 10
BOX_BYTES = 12
TOC_FLAG_EMPTY = 0x01
BLOB_FLAG_COLLISION = 0x01

# The caps the runtime is compiled with (city_format.h); the archive header
# records what was actually used so the loader can refuse a mismatch.
LOD_VERT_CAP = (384, 144, 48)
LOD_FACE_CAP = (176, 64, 24)
LOD_SLOT_BYTES = (4096, 1536, 1024)
# VDP1 VRAM per ring slot for a LOD's texture block (0 = that LOD is untextured).
LOD_TEXTURE_BYTES = (16384, 5120, 1536)
LOD2_BOX_CAP = 24


def align(value: int, to: int = ALIGN) -> int:
    return (value + to - 1) // to * to


@dataclass
class BlobSpec:
    chunk_index: int
    lod: int
    vertices: list  # (x, y, z) int16 ticks, chunk-local
    faces: list  # (a, b, c, d, material)
    boxes: list = field(default_factory=list)  # (cx, cy, cz, hx, hy, hz) int16 ticks
    # (width, height, texels bytes[, flags]) INDEXED8; faces may carry a 6th
    # element, the texture index + 1 (0 = solid).
    textures: list = field(default_factory=list)


@dataclass
class GroundSection:
    bitmap: bytes  # width * height, one palette index per dot, 0 = no ground
    width: int
    height: int
    palette: list  # RGB555 ints, index 0 is a placeholder
    ground_y_units: float
    units_per_dot_x: int = 1
    units_per_dot_z: int = 1


def _i16(value: int, what: str) -> int:
    if not -32768 <= value <= 32767:
        raise GltfError(f"{what} = {value} does not fit int16")
    return value


def pack_blob(spec: BlobSpec) -> bytes:
    vc, fc, nb = len(spec.vertices), len(spec.faces), len(spec.boxes)
    if vc == 0 or fc == 0:
        raise GltfError(f"chunk {spec.chunk_index} LOD{spec.lod}: empty blobs are TOC flags, not blobs")
    xs = [v[0] for v in spec.vertices]
    ys = [v[1] for v in spec.vertices]
    zs = [v[2] for v in spec.vertices]
    bbox_min = (min(xs), min(ys), min(zs))
    bbox_max = (max(xs), max(ys), max(zs))
    face_offset = align(BLOB_HEADER_BYTES + VERTEX_BYTES * vc, 4)
    box_offset = align(face_offset + FACE_BYTES * fc, 2) if nb else 0
    flags = BLOB_FLAG_COLLISION if nb else 0
    out = bytearray()
    out += struct.pack(">IHBBHH", BLOB_MAGIC, spec.chunk_index, spec.lod, flags, vc, fc)
    out += struct.pack(">6h", *(_i16(v, "bbox") for v in bbox_min + bbox_max))
    out += struct.pack(">HHHH", BLOB_HEADER_BYTES, face_offset, box_offset, nb)
    assert len(out) == BLOB_HEADER_BYTES
    for x, y, z in spec.vertices:
        out += struct.pack(">3h", _i16(x, "x"), _i16(y, "y"), _i16(z, "z"))
    out += b"\x00" * (face_offset - len(out))
    for face in spec.faces:
        a, b, c, d, material = face[:5]
        texture = face[5] if len(face) > 5 else 0
        if max(a, b, c, d) >= vc:
            raise GltfError(f"chunk {spec.chunk_index}: face index beyond {vc} vertices")
        if not 0 <= material < MATERIAL_MAX:
            raise GltfError(f"material {material} out of range")
        if not 0 <= texture <= min(len(spec.textures), 255):
            raise GltfError(f"chunk {spec.chunk_index}: face texture {texture} of {len(spec.textures)}")
        out += struct.pack(">4HBB", a, b, c, d, material, texture)
    if nb:
        out += b"\x00" * (box_offset - len(out))
        for box in spec.boxes:
            out += struct.pack(">6h", *(_i16(v, "box") for v in box))
    if len(out) > 0xFFFF or vc > 0xFFFF or fc > 0xFFFF:
        raise GltfError(f"chunk {spec.chunk_index} LOD{spec.lod}: blob is {len(out)} bytes "
                        f"(the TOC stores u16 sizes: at most 65535)")
    return bytes(out)


def pack_textures(spec: BlobSpec) -> bytes:
    """The texture block of one blob (empty bytes when it has none)."""
    if not spec.textures:
        return b""
    count = len(spec.textures)
    cursor = align(TEXTURE_TABLE_HEADER + TEXTURE_ENTRY_BYTES * count, 8)
    table = bytearray(struct.pack(">HH", count, 0))
    texels = bytearray()
    for texture in spec.textures:
        if len(texture) == 3:
            w, h, data = texture
            flags = 0
        elif len(texture) == 4:
            w, h, data, flags = texture
        else:
            raise GltfError(f"chunk {spec.chunk_index}: invalid texture tuple")
        if w % 8 or not 8 <= w <= TEXTURE_MAX_WIDTH or not 1 <= h <= TEXTURE_MAX_HEIGHT:
            raise GltfError(f"chunk {spec.chunk_index}: texture {w}x{h} is not a VDP1 size")
        if len(data) != w * h:
            raise GltfError(f"chunk {spec.chunk_index}: texture {w}x{h} has {len(data)} texels")
        if flags & ~TEXTURE_FLAG_MASK:
            raise GltfError(f"chunk {spec.chunk_index}: texture flags 0x{flags:04x} unsupported")
        table += struct.pack(">HHHH", w, h, cursor // 8, flags)
        texels += data
        cursor += len(data)
        pad = align(cursor, 8) - cursor
        texels += bytes(pad)
        cursor += pad
    table += bytes(align(len(table), 8) - len(table))
    out = bytes(table + texels)
    if len(out) > 0xFFFF:
        raise GltfError(f"chunk {spec.chunk_index} LOD{spec.lod}: texture block {len(out)} B > 65535")
    return out


def _toc_bytes(entries: dict) -> bytes:
    out = bytearray()
    for chunk in range(CHUNK_COUNT):
        for lod in range(LOD_COUNT):
            e = entries.get((chunk, lod))
            if e is None:
                out += struct.pack(">IHHHBBHH", 0, 0, 0, 0, 0, TOC_FLAG_EMPTY, 0, 0)
            else:
                offset, nbytes, vc, fc, bank, tex_bytes = e
                out += struct.pack(">IHHHBBHH", offset, nbytes, vc, fc, bank, 0, tex_bytes, 0)
    return bytes(out)


def pack_archive(blobs: dict, material_rgb555: list, ground: GroundSection | None,
                 world_min_y_units: int, world_max_y_units: int,
                 max_archive_bytes: int | None = None, texture_palette: list | None = None):
    """``blobs`` maps (chunk_index, lod) -> BlobSpec; missing keys are empty.

    Returns ``(bytes, info)``. Blobs never straddle a 2 MiB boundary of the
    region starting at blob_base: the packer pads to the next bank instead, so
    the runtime can copy each bank with one contiguous loop. Output is
    deterministic: same inputs, same bytes."""
    if not 1 <= len(material_rgb555) <= MATERIAL_MAX:
        raise GltfError(f"{len(material_rgb555)} materials (1..{MATERIAL_MAX} allowed)")

    packed = {}
    for key in sorted(blobs):
        spec = blobs[key]
        packed[key] = (pack_blob(spec), len(spec.vertices), len(spec.faces),
                       len(spec.boxes), pack_textures(spec))

    material_offset = HEADER_BYTES
    material_bytes = b"".join(struct.pack(">HBB", rgb, 0, 0) for rgb in material_rgb555)
    toc_offset = align(material_offset + len(material_bytes))
    toc_end = toc_offset + TOC_ENTRY_BYTES * CHUNK_COUNT * LOD_COUNT

    ground_offset = ground_bytes = palette_offset = palette_count = 0
    ground_blob = b""
    palette_blob = b""
    cursor = align(toc_end)
    if ground is not None:
        if len(ground.bitmap) != ground.width * ground.height:
            raise GltfError("ground bitmap size disagrees with width * height")
        if not 1 <= len(ground.palette) <= 256:
            raise GltfError("ground palette needs 1..256 entries")
        ground_offset, ground_bytes = cursor, len(ground.bitmap)
        ground_blob = ground.bitmap
        palette_offset = align(ground_offset + ground_bytes)
        palette_count = len(ground.palette)
        palette_blob = b"".join(struct.pack(">H", c) for c in ground.palette)
        cursor = align(palette_offset + len(palette_blob))
    tex_palette_offset = tex_palette_count = 0
    tex_palette_blob = b""
    if texture_palette:
        if not 1 <= len(texture_palette) <= 256:
            raise GltfError("texture palette needs 1..256 entries")
        tex_palette_offset, tex_palette_count = cursor, len(texture_palette)
        tex_palette_blob = b"".join(struct.pack(">H", c) for c in texture_palette)
        cursor = align(tex_palette_offset + len(tex_palette_blob))
    blob_base = cursor

    body = bytearray()
    entries = {}
    maxima = {"v": [0] * LOD_COUNT, "f": [0] * LOD_COUNT, "b": [0] * LOD_COUNT,
              "t": [0] * LOD_COUNT}
    max_boxes = 0
    for (chunk, lod), (data, vc, fc, nb, tex) in packed.items():
        offset = align(len(body))
        # A blob and its texture block share one bank: never straddle 2 MiB.
        span = align(len(data)) + len(tex)
        bank_end = (offset // BANK_BYTES + 1) * BANK_BYTES
        if offset + span > bank_end:
            offset = bank_end
        bank = offset // BANK_BYTES
        if bank > 1:
            raise GltfError(
                f"archive needs a third 2 MiB bank at chunk {chunk} LOD{lod}: the cart has two")
        body += b"\x00" * (offset - len(body))
        body += data
        if tex:
            body += bytes(align(len(body)) - len(body))
            body += tex
        entries[(chunk, lod)] = (offset, len(data), vc, fc, bank, len(tex))
        maxima["t"][lod] = max(maxima["t"][lod], len(tex))
        maxima["v"][lod] = max(maxima["v"][lod], vc)
        maxima["f"][lod] = max(maxima["f"][lod], fc)
        maxima["b"][lod] = max(maxima["b"][lod], len(data))
        max_boxes = max(max_boxes, nb)
    total = align(blob_base + len(body))
    if max_archive_bytes is not None and total > max_archive_bytes:
        raise GltfError(
            f"archive is {total} bytes, over --max-archive-bytes {max_archive_bytes}")

    toc = _toc_bytes(entries)
    table = bytearray(toc_end - material_offset)
    table[:len(material_bytes)] = material_bytes
    table[toc_offset - material_offset:] = toc
    crc = zlib.crc32(bytes(table)) & 0xFFFFFFFF

    header = bytearray()
    header += struct.pack(">IHHHH", MAGIC, VERSION, 0, GRID_X, GRID_Z)
    header += struct.pack(">ii", ORIGIN_X_UNITS << 16, ORIGIN_Z_UNITS << 16)
    header += struct.pack(">HHBB", CHUNK_UNITS, QUANT_SCALE, LOD_COUNT, 0)
    header += struct.pack(">H", len(material_rgb555))
    header += struct.pack(">IIII", material_offset, toc_offset, blob_base, total)
    header += struct.pack(">iiI", world_min_y_units << 16, world_max_y_units << 16, crc)
    header += struct.pack(">3H3H3H", *maxima["v"], *maxima["f"], *maxima["b"])
    header += struct.pack(">H", max_boxes)
    header += struct.pack(">IIIi", ground_offset, ground_bytes, palette_offset,
                          int(round((ground.ground_y_units if ground else 0.0) * 65536)))
    header += struct.pack(">HHHHH", ground.width if ground else 0, ground.height if ground else 0,
                          palette_count, ground.units_per_dot_x if ground else 0,
                          ground.units_per_dot_z if ground else 0)
    header += bytes(0x68 - len(header))
    header += struct.pack(">IH3H", tex_palette_offset, tex_palette_count, *maxima["t"])
    header += bytes(HEADER_BYTES - len(header))
    assert len(header) == HEADER_BYTES, len(header)

    out = bytearray(total)
    out[:HEADER_BYTES] = header
    out[material_offset:toc_end] = table
    if ground is not None:
        out[ground_offset:ground_offset + ground_bytes] = ground_blob
        out[palette_offset:palette_offset + len(palette_blob)] = palette_blob
    if tex_palette_blob:
        out[tex_palette_offset:tex_palette_offset + len(tex_palette_blob)] = tex_palette_blob
    out[blob_base:blob_base + len(body)] = body
    info = {
        "total_bytes": total, "blob_base": blob_base, "toc_offset": toc_offset,
        "material_count": len(material_rgb555), "toc_crc32": crc,
        "max_vertices": maxima["v"], "max_faces": maxima["f"],
        "max_blob_bytes": maxima["b"], "max_collision_boxes": max_boxes,
        "blob_region_bytes": len(body), "blobs": len(packed),
        "ground_bytes": ground_bytes, "ground_offset": ground_offset,
        "ground_palette_offset": palette_offset, "ground_palette_count": palette_count,
        "texture_palette_offset": tex_palette_offset, "texture_palette_count": tex_palette_count,
        "max_texture_bytes": maxima["t"],
    }
    return bytes(out), info


def emit_header_h(info: dict, sha256_hex: str) -> str:
    """The generated header the runtime includes: sizes it may static-assert on."""
    lines = [
        "/* Generated by tools/city_chunker.py -- do not edit. */",
        "#ifndef CITY_DATA_H",
        "#define CITY_DATA_H",
        "",
        f"#define CITY_ARCHIVE_BYTES {info['total_bytes']}u",
        f"#define CITY_BLOB_BASE {info['blob_base']}u",
        f"#define CITY_BLOB_REGION_BYTES {info['blob_region_bytes']}u",
        f"#define CITY_MATERIAL_COUNT {info['material_count']}u",
        f"#define CITY_TOC_CRC32 0x{info['toc_crc32']:08X}u",
        f"#define CITY_GROUND_BYTES {info['ground_bytes']}u",
        f"#define CITY_GROUND_OFFSET {info['ground_offset']}u",
        f"#define CITY_GROUND_PALETTE_OFFSET {info['ground_palette_offset']}u",
        f"#define CITY_GROUND_PALETTE_COUNT {info['ground_palette_count']}u",
        f"#define CITY_SOURCE_SHA256 \"{sha256_hex}\"",
        "",
        "#endif /* CITY_DATA_H */",
        "",
    ]
    return "\n".join(lines)
