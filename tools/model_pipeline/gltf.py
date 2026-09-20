#!/usr/bin/env python3
"""Deterministic glTF 2.0 / GLB reader for the LibSaturn host pipeline.

Host-side only: this module never runs on the Saturn. It parses the glTF
subset needed for robust animated-character import and exposes raw,
uninterpreted accessor data plus small decoding helpers. Interpretation
(skinning, animation sampling, Saturn compilation) lives in the sibling
``model`` and ``animation`` modules so parsing stays separate from
Saturn-specific conventions.

Supported (``.glb`` mandatory, textual ``.gltf`` with an embedded or
sidecar BIN accepted where practical):

- GLB header / JSON chunk / BIN chunk, buffers, bufferViews, accessors
- component types 5120/5121/5122/5123/5125/5126, normalized integer
  accessors, accessor byte offsets, interleaved byte strides
- nodes (matrix or TRS), meshes, triangle-list primitives, materials,
  textures, embedded-image bufferViews
- POSITION, NORMAL, TEXCOORD_0, JOINTS_0, WEIGHTS_0, 8/16/32-bit indices
- skins, inverse bind matrices, animations, samplers, channels,
  translation/rotation/scale targets, STEP and LINEAR interpolation

CUBICSPLINE is rejected with a precise diagnostic until a correct
implementation lands. Non-triangle primitive modes are rejected unless a
deterministic conversion is implemented by the caller.
"""

from __future__ import annotations

import json
import struct
from dataclasses import dataclass, field
from pathlib import Path


class GltfError(Exception):
    """Precise diagnostic for an unsupported or malformed glTF asset."""


# Chunk types and magic.
_GLB_MAGIC = b"glTF"
_JSON_CHUNK_TYPE = 0x4E4F534A
_BIN_CHUNK_TYPE = 0x004E4942

# componentType -> (struct format char, byte size).
_COMPONENT_TYPES = {
    5120: ("b", 1),  # BYTE
    5121: ("B", 1),  # UNSIGNED_BYTE
    5122: ("h", 2),  # SHORT
    5123: ("H", 2),  # UNSIGNED_SHORT
    5125: ("I", 4),  # UNSIGNED_INT
    5126: ("f", 4),  # FLOAT
}

# accessor type -> component count.
_ACCESSOR_COUNTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
    "MAT2": 4,
    "MAT3": 9,
    "MAT4": 16,
}

# glTF primitive modes.
PRIM_POINTS = 0
PRIM_LINES = 1
PRIM_LINE_LOOP = 2
PRIM_LINE_STRIP = 3
PRIM_TRIANGLES = 4
PRIM_TRIANGLE_STRIP = 5
PRIM_TRIANGLE_FAN = 6


@dataclass
class GlbData:
    """Parsed GLB container: JSON document plus raw BIN bytes."""

    json: dict
    bin: bytes = b""
    source_path: Path | None = None


def srgb_to_linear(c: float) -> float:
    """sRGB-encoded 0..1 channel to linear light."""
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(c: float) -> float:
    """Linear-light 0..1 channel back to an sRGB-encoded channel."""
    c = min(max(c, 0.0), 1.0)
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1.0 / 2.4) - 0.055


def _u32(data: bytes, offset: int, what: str, path: str) -> int:
    if offset + 4 > len(data):
        raise GltfError(f"{path}: truncated file while reading {what}")
    return struct.unpack_from("<I", data, offset)[0]


def parse_glb(path: Path | str) -> GlbData:
    """Parse a ``.glb`` file into its JSON document and BIN chunk."""
    path = Path(path)
    try:
        data = path.read_bytes()
    except FileNotFoundError:
        raise GltfError(f"GLB not found: {path}")
    except OSError as exc:
        raise GltfError(f"Cannot read GLB {path}: {exc}")
    return parse_glb_bytes(data, source_path=path)


def parse_glb_bytes(data: bytes, source_path: Path | None = None) -> GlbData:
    """Parse in-memory GLB bytes (used by tests and tooling alike)."""
    path = str(source_path) if source_path is not None else "<bytes>"
    if len(data) < 12:
        raise GltfError(f"{path}: truncated GLB header ({len(data)} bytes)")
    magic = data[0:4]
    if magic != _GLB_MAGIC:
        raise GltfError(f"{path}: bad GLB magic {magic!r} (expected b'glTF')")
    version = _u32(data, 4, "version", path)
    if version != 2:
        raise GltfError(f"{path}: unsupported glTF version {version} (need 2)")
    total = _u32(data, 8, "total length", path)
    if total != len(data):
        raise GltfError(
            f"{path}: GLB length field {total} disagrees with file size {len(data)}"
        )
    offset = 12
    json_doc: dict | None = None
    bin_chunk = b""
    seen_json = False
    while offset < len(data):
        chunk_len = _u32(data, offset, "chunk length", path)
        if offset + 8 > len(data):
            raise GltfError(f"{path}: truncated GLB chunk header")
        chunk_type = _u32(data, offset + 4, "chunk type", path)
        start = offset + 8
        end = start + chunk_len
        if end > len(data):
            raise GltfError(f"{path}: truncated GLB chunk ({chunk_len} bytes)")
        payload = data[start:end]
        if chunk_type == _JSON_CHUNK_TYPE:
            if seen_json:
                raise GltfError(f"{path}: duplicate JSON chunk")
            seen_json = True
            try:
                text = payload.decode("utf-8")
            except UnicodeDecodeError as exc:
                raise GltfError(f"{path}: JSON chunk is not UTF-8: {exc}")
            try:
                parsed = json.loads(text)
            except json.JSONDecodeError as exc:
                raise GltfError(f"{path}: malformed GLB JSON chunk: {exc}")
            if not isinstance(parsed, dict):
                raise GltfError(f"{path}: GLB JSON chunk must be an object")
            json_doc = parsed
        elif chunk_type == _BIN_CHUNK_TYPE:
            if bin_chunk:
                raise GltfError(f"{path}: duplicate BIN chunk")
            bin_chunk = payload
        else:
            raise GltfError(f"{path}: unknown GLB chunk type 0x{chunk_type:08X}")
        offset = end
    if json_doc is None:
        raise GltfError(f"{path}: GLB has no JSON chunk")
    return GlbData(json=json_doc, bin=bytes(bin_chunk), source_path=source_path)


def parse_gltf(path: Path | str) -> GlbData:
    """Parse a textual ``.gltf`` file plus its buffer(s).

    Only buffer index 0 is used; it may be an embedded data URI or an
    external ``.bin`` file next to the ``.gltf`` document. This keeps the
    desktop-viewer workflow working without compromising the ``.glb``
    acceptance path.
    """
    import base64

    path = Path(path)
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise GltfError(f"glTF not found: {path}")
    except json.JSONDecodeError as exc:
        raise GltfError(f"{path}: malformed glTF JSON: {exc}")
    except OSError as exc:
        raise GltfError(f"Cannot read glTF {path}: {exc}")
    if not isinstance(doc, dict):
        raise GltfError(f"{path}: glTF JSON must be an object")
    buffers = doc.get("buffers", [])
    if len(buffers) > 1:
        raise GltfError(f"{path}: only single-buffer glTF is supported")
    bin_data = b""
    if buffers:
        uri = buffers[0].get("uri")
        if uri is None:
            raise GltfError(f"{path}: buffer without uri needs a .glb container")
        if uri.startswith("data:"):
            try:
                bin_data = base64.b64decode(uri.split(",", 1)[1])
            except (ValueError, IndexError) as exc:
                raise GltfError(f"{path}: malformed data URI: {exc}")
        else:
            bin_path = path.parent / uri
            try:
                bin_data = bin_path.read_bytes()
            except FileNotFoundError:
                raise GltfError(f"{path}: buffer file not found: {bin_path}")
    return GlbData(json=doc, bin=bin_data, source_path=path)


def parse_model(path: Path | str) -> GlbData:
    """Parse ``.glb`` (or textual ``.gltf``) based on file suffix."""
    path = Path(path)
    if path.suffix.lower() == ".gltf":
        return parse_gltf(path)
    if path.suffix.lower() != ".glb":
        raise GltfError(f"{path}: expected a .glb (or .gltf) file")
    return parse_glb(path)


# ----------------------------------------------------------------------
# Buffer / accessor decoding
# ----------------------------------------------------------------------


def get_buffer_view_bytes(glb: GlbData, view_index: int) -> bytes:
    """Return the raw bytes of a bufferView (without accessor slicing)."""
    views = glb.json.get("bufferViews", [])
    if view_index < 0 or view_index >= len(views):
        raise GltfError(f"bufferView {view_index} out of range ({len(views)} views)")
    view = views[view_index]
    if view.get("buffer", 0) != 0:
        raise GltfError(f"bufferView {view_index}: only buffer 0 is supported")
    start = int(view.get("byteOffset", 0))
    length = int(view.get("byteLength", 0))
    if start < 0 or length < 0 or start + length > len(glb.bin):
        raise GltfError(
            f"bufferView {view_index}: range [{start}, {start + length}) "
            f"outside BIN ({len(glb.bin)} bytes)"
        )
    return glb.bin[start : start + length]


def _normalized_to_float(value: int | float, fmt: str, normalized: bool) -> float:
    if not normalized:
        return float(value)
    if fmt == "b":
        return max(float(value) / 127.0, -1.0)
    if fmt == "B":
        return float(value) / 255.0
    if fmt == "h":
        return max(float(value) / 32767.0, -1.0)
    if fmt == "H":
        return float(value) / 65535.0
    return float(value)


@dataclass
class AccessorData:
    """Decoded accessor payload: rows of components plus metadata."""

    rows: list
    component_type: int
    accessor_type: str
    count: int
    normalized: bool


def read_accessor(glb: GlbData, accessor_index: int) -> AccessorData:
    """Decode one accessor into rows of Python numbers.

    Honors ``byteOffset`` (accessor and bufferView), interleaved
    ``byteStride``, and ``normalized`` integer mappings. Sparse accessors
    are rejected: silently misreading them would corrupt skinning.
    """
    accessors = glb.json.get("accessors", [])
    if accessor_index < 0 or accessor_index >= len(accessors):
        raise GltfError(
            f"accessor {accessor_index} out of range ({len(accessors)} accessors)"
        )
    acc = accessors[accessor_index]
    if "sparse" in acc:
        raise GltfError(f"accessor {accessor_index}: sparse accessors are not supported")
    component_type = acc.get("componentType")
    if component_type not in _COMPONENT_TYPES:
        raise GltfError(
            f"accessor {accessor_index}: unknown componentType {component_type!r}"
        )
    accessor_type = acc.get("type")
    if accessor_type not in _ACCESSOR_COUNTS:
        raise GltfError(f"accessor {accessor_index}: unknown type {accessor_type!r}")
    count = int(acc.get("count", 0))
    if count < 0:
        raise GltfError(f"accessor {accessor_index}: negative count {count}")
    if "bufferView" not in acc:
        raise GltfError(f"accessor {accessor_index}: accessors without bufferView need application data the importer does not implement")
    view_index = int(acc["bufferView"])
    views = glb.json.get("bufferViews", [])
    if view_index < 0 or view_index >= len(views):
        raise GltfError(f"accessor {accessor_index}: bufferView {view_index} out of range")
    view = views[view_index]
    if view.get("buffer", 0) != 0:
        raise GltfError(f"accessor {accessor_index}: only buffer 0 is supported")
    fmt, size = _COMPONENT_TYPES[component_type]
    n_comp = _ACCESSOR_COUNTS[accessor_type]
    elem_size = size * n_comp
    stride = int(view.get("byteStride", elem_size))
    if stride < elem_size:
        raise GltfError(
            f"accessor {accessor_index}: byteStride {stride} < element size {elem_size}"
        )
    base = int(view.get("byteOffset", 0)) + int(acc.get("byteOffset", 0))
    normalized = bool(acc.get("normalized", False))
    end = base + (count - 1) * stride + elem_size if count else base
    if base < 0 or end > len(glb.bin):
        raise GltfError(
            f"accessor {accessor_index}: range [{base}, {end}) "
            f"outside BIN ({len(glb.bin)} bytes)"
        )
    rows: list = []
    for i in range(count):
        off = base + i * stride
        raw = struct.unpack_from("<" + fmt * n_comp, glb.bin, off)
        if n_comp == 1:
            rows.append(_normalized_to_float(raw[0], fmt, normalized))
        else:
            rows.append(
                tuple(_normalized_to_float(v, fmt, normalized) for v in raw)
            )
    return AccessorData(
        rows=rows,
        component_type=component_type,
        accessor_type=accessor_type,
        count=count,
        normalized=normalized,
    )


def read_indices(glb: GlbData, accessor_index: int) -> list[int]:
    """Decode an index accessor (8/16/32-bit) into plain ints."""
    data = read_accessor(glb, accessor_index)
    if data.accessor_type != "SCALAR":
        raise GltfError(f"accessor {accessor_index}: indices must be SCALAR")
    if data.component_type not in (5121, 5123, 5125):
        raise GltfError(
            f"accessor {accessor_index}: illegal index componentType "
            f"{data.component_type} (need 8/16/32-bit uint: 5121/5123/5125)"
        )
    return [int(v) for v in data.rows]


def extract_image_bytes(glb: GlbData, image_index: int) -> tuple[bytes, str]:
    """Return ``(bytes, mimeType)`` for an image with a bufferView source."""
    images = glb.json.get("images", [])
    if image_index < 0 or image_index >= len(images):
        raise GltfError(f"image {image_index} out of range ({len(images)} images)")
    image = images[image_index]
    if "bufferView" not in image:
        raise GltfError(
            f"image {image_index}: only embedded bufferView images are supported "
            "(external URIs would make builds depend on sidecar files)"
        )
    payload = get_buffer_view_bytes(glb, int(image["bufferView"]))
    return bytes(payload), str(image.get("mimeType", "application/octet-stream"))


def decode_image_rgba(payload: bytes, what: str = "texture") -> tuple[int, int, list]:
    """Decode PNG/JPEG bytes to ``(width, height, [(r,g,b,a), ...])`` rows."""
    try:
        from PIL import Image
    except ModuleNotFoundError as exc:
        raise GltfError("Image support needs Pillow: pip install pillow") from exc
    import io

    try:
        img = Image.open(io.BytesIO(payload)).convert("RGBA")
    except OSError as exc:
        raise GltfError(f"Cannot decode {what} image: {exc}")
    w, h = img.size
    if w <= 0 or h <= 0:
        raise GltfError(f"{what} image has invalid size {w}x{h}")
    get_flat = getattr(img, "get_flattened_data", None)
    data = list(get_flat()) if callable(get_flat) else list(img.getdata())
    return w, h, [(r, g, b, a) for (r, g, b, a) in data]


# ----------------------------------------------------------------------
# Node transforms
# ----------------------------------------------------------------------


def node_local_matrix(node: dict, what: str = "node") -> list[float]:
    """Return a node's local transform as a 16-float column-major matrix."""
    if "matrix" in node:
        m = node["matrix"]
        if len(m) != 16:
            raise GltfError(f"{what}: matrix must have 16 elements")
        return [float(v) for v in m]
    t = node.get("translation", [0.0, 0.0, 0.0])
    r = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    s = node.get("scale", [1.0, 1.0, 1.0])
    if len(t) != 3 or len(r) != 4 or len(s) != 3:
        raise GltfError(f"{what}: malformed TRS transform")
    return trs_to_matrix(
        (float(t[0]), float(t[1]), float(t[2])),
        (float(r[0]), float(r[1]), float(r[2]), float(r[3])),
        (float(s[0]), float(s[1]), float(s[2])),
    )


def trs_to_matrix(
    t: tuple[float, float, float],
    r: tuple[float, float, float, float],
    s: tuple[float, float, float],
) -> list[float]:
    """Compose a column-major 4x4 matrix from translation/quaternion/scale."""
    x, y, z, w = r
    x2, y2, z2 = x + x, y + y, z + z
    xx, xy, xz = x * x2, x * y2, x * z2
    yy, yz, zz = y * y2, y * z2, z * z2
    wx, wy, wz = w * x2, w * y2, w * z2
    sx, sy, sz = s
    return [
        (1.0 - (yy + zz)) * sx,
        (xy + wz) * sx,
        (xz - wy) * sx,
        0.0,
        (xy - wz) * sy,
        (1.0 - (xx + zz)) * sy,
        (yz + wx) * sy,
        0.0,
        (xz + wy) * sz,
        (yz - wx) * sz,
        (1.0 - (xx + yy)) * sz,
        0.0,
        float(t[0]),
        float(t[1]),
        float(t[2]),
        1.0,
    ]


def check_primitive_mode(primitive: dict, mesh_index: int = 0) -> None:
    """Reject non-triangle primitives with a precise diagnostic."""
    mode = int(primitive.get("mode", PRIM_TRIANGLES))
    if mode == PRIM_TRIANGLES:
        return
    names = {
        PRIM_POINTS: "POINTS",
        PRIM_LINES: "LINES",
        PRIM_LINE_LOOP: "LINE_LOOP",
        PRIM_LINE_STRIP: "LINE_STRIP",
        PRIM_TRIANGLE_STRIP: "TRIANGLE_STRIP",
        PRIM_TRIANGLE_FAN: "TRIANGLE_FAN",
    }
    raise GltfError(
        f"mesh {mesh_index}: primitive mode {names.get(mode, mode)} is not "
        "supported (only TRIANGLES; strips/fans need deterministic "
        "de-indexing the importer does not implement)"
    )


def check_interpolation(interp: str, sampler_index: int) -> None:
    """Reject animation interpolation the evaluator cannot sample exactly."""
    if interp in ("STEP", "LINEAR"):
        return
    raise GltfError(
        f"animation sampler {sampler_index}: interpolation {interp!r} is not "
        "supported (CUBICSPLINE needs a correct tangent implementation; "
        "re-author the clip with LINEAR sampling)"
    )
