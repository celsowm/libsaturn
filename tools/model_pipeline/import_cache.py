"""Input fingerprinting and incremental import cache management."""
from __future__ import annotations
import hashlib
import importlib.metadata
import json
import os
import sys
import tempfile
from pathlib import Path
from . import gltf as gltf_mod
from .obj_import import parse_mtl, parse_obj
IMPORT_SIGNATURE_SCHEMA = 1

def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _canonical_path(path: Path) -> str:
    try:
        return str(path.resolve())
    except OSError:
        return str(path.absolute())


def _referenced_input_paths(input_path: Path) -> list[Path]:
    """Find local files consumed in addition to the primary model input.

    This is deliberately best-effort: malformed inputs still reach the normal
    importer, which owns the user-facing diagnostic. Existing external assets
    are hashed; missing ones are recorded as missing in the signature.
    """
    paths: list[Path] = []
    suffix = input_path.suffix.lower()
    try:
        if suffix == ".obj":
            model = parse_obj(input_path)
            for mtl_name in model.mtllibs:
                mtl_path = Path(mtl_name)
                if not mtl_path.is_absolute():
                    mtl_path = input_path.parent / mtl_path
                paths.append(mtl_path)
                try:
                    materials = parse_mtl(mtl_path)
                except ImportError:
                    continue
                for material in materials.values():
                    texture_name = material.get("map_Kd")
                    if texture_name:
                        texture_path = Path(texture_name)
                        if not texture_path.is_absolute():
                            texture_path = input_path.parent / texture_path
                        paths.append(texture_path)
        elif suffix in (".glb", ".gltf"):
            if suffix == ".glb":
                doc = gltf_mod.parse_glb(input_path).json
            else:
                doc = json.loads(input_path.read_text(encoding="utf-8"))
            for group in ("buffers", "images"):
                for item in doc.get(group, []):
                    uri = item.get("uri") if isinstance(item, dict) else None
                    if not isinstance(uri, str) or uri.startswith("data:"):
                        continue
                    referenced = Path(uri)
                    if not referenced.is_absolute():
                        referenced = input_path.parent / referenced
                    paths.append(referenced)
    except (ImportError, OSError, ValueError, TypeError, KeyError, gltf_mod.GltfError):
        pass

    unique: dict[str, Path] = {}
    for path in paths:
        unique.setdefault(_canonical_path(path), path)
    return [unique[key] for key in sorted(unique)]


def _import_signature(args: argparse.Namespace) -> dict:
    input_path = Path(args.input)
    dependencies = [input_path, *_referenced_input_paths(input_path)]
    input_records = []
    for path in dependencies:
        record = {"path": _canonical_path(path)}
        if path.is_file():
            record["sha256"] = _sha256_file(path)
        else:
            record["missing"] = True
        input_records.append(record)

    tools_dir = Path(__file__).resolve().parents[1]
    tool_paths = [tools_dir / "import_model.py", tools_dir / "saturn_asset_common.py"]
    tool_paths.extend(sorted((tools_dir / "model_pipeline").glob("*.py")))
    tool_records = [
        {"path": _canonical_path(path), "sha256": _sha256_file(path)}
        for path in tool_paths if path.is_file()
    ]
    try:
        pillow_version = importlib.metadata.version("Pillow")
    except importlib.metadata.PackageNotFoundError:
        pillow_version = "not-installed"

    options = {
        key: value for key, value in vars(args).items()
        if key not in ("incremental", "force_import", "signature_only", "signature_file")
    }
    for key in ("input", "out_prefix", "report"):
        value = options.get(key)
        if value is not None:
            options[key] = _canonical_path(Path(value))

    payload = {
        "schema_version": IMPORT_SIGNATURE_SCHEMA,
        "options": options,
        "inputs": input_records,
        "tools": tool_records,
        "runtime": {"python": sys.version, "pillow": pillow_version},
    }
    canonical = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    payload["signature"] = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
    return payload


def _atomic_write_json(path: Path, payload: dict, preserve_if_equal: bool = False) -> None:
    encoded = (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode("utf-8")
    if preserve_if_equal:
        try:
            if path.read_bytes() == encoded:
                return
        except OSError:
            pass
    path.parent.mkdir(parents=True, exist_ok=True)
    temp_name = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=path.parent, prefix=path.name + ".", suffix=".tmp",
            delete=False,
        ) as stream:
            temp_name = stream.name
            stream.write(encoded)
        os.replace(temp_name, path)
    finally:
        if temp_name and os.path.exists(temp_name):
            os.unlink(temp_name)


def _model_output_paths(args: argparse.Namespace) -> dict[str, Path]:
    prefix = str(args.out_prefix)
    outputs = {"source": Path(prefix + ".c"), "header": Path(prefix + ".h")}
    if args.report and Path(args.input).suffix.lower() in (".glb", ".gltf"):
        outputs["report"] = Path(args.report)
    return outputs


def _cache_manifest_path(args: argparse.Namespace) -> Path:
    return Path(str(args.out_prefix) + ".import.json")


def _incremental_cache_hit(args: argparse.Namespace, signature: dict) -> bool:
    manifest_path = _cache_manifest_path(args)
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return False
    if not isinstance(manifest, dict):
        return False
    if (manifest.get("schema_version") != IMPORT_SIGNATURE_SCHEMA or
            manifest.get("signature") != signature["signature"]):
        return False
    recorded_outputs = manifest.get("outputs", {})
    if not isinstance(recorded_outputs, dict):
        return False
    for role, path in _model_output_paths(args).items():
        recorded = recorded_outputs.get(role, {})
        if not isinstance(recorded, dict):
            return False
        if not path.is_file() or recorded.get("path") != _canonical_path(path):
            return False
        try:
            if recorded.get("sha256") != _sha256_file(path):
                return False
        except OSError:
            return False
    return True


def _write_incremental_manifest(args: argparse.Namespace, signature: dict) -> None:
    outputs = {}
    for role, path in _model_output_paths(args).items():
        outputs[role] = {"path": _canonical_path(path), "sha256": _sha256_file(path)}
    manifest = {
        "schema_version": IMPORT_SIGNATURE_SCHEMA,
        "signature": signature["signature"],
        "outputs": outputs,
    }
    _atomic_write_json(_cache_manifest_path(args), manifest)


