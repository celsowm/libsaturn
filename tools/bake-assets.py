"""Pre-generates converted assets with SHA-256 hash cache."""

import hashlib
import json
import sys
from pathlib import Path

# Add tools to path to import convert_indexed8
sys.path.insert(0, str(Path(__file__).parent))
from convert_indexed8 import convert_asset


def sha256_file(path: Path) -> str:
    """Computes SHA-256 of a file."""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


def bake_example(example_dir: Path) -> None:
    """Processes assets for an example, reconverts if hash changed."""
    prebuilt_dir = example_dir / "prebuilt"
    manifest_path = prebuilt_dir / "manifest.json"

    if not manifest_path.exists():
        return

    manifest = json.loads(manifest_path.read_text())
    assets = manifest.get("assets", {})

    if not assets:
        return

    print(f"\n[ bake] {example_dir.name}/")
    for name, info in assets.items():
        # Resolver caminho relativo ao prebuilt_dir
        input_path = Path(info["input"])
        if not input_path.is_absolute():
            input_path = (prebuilt_dir / input_path).resolve()
        if not input_path.exists():
            print(f"  [skip] {info['input']} not found")
            continue

        # Calculate current hash
        current_hash = sha256_file(input_path)
        saved_hash = info.get("hash", "")

        if current_hash == saved_hash:
            print(f"  [ ok] {name} (hash unchanged)")
            continue

        # Reconvert
        resize = tuple(info["resize"]) if info.get("resize") else None
        output_prefix = str(prebuilt_dir / info["output"])
        Path(output_prefix).parent.mkdir(parents=True, exist_ok=True)

        print(f"  [conv] {name} ({info['input']})")
        convert_asset(
            in_path=input_path,
            out_prefix=Path(output_prefix),
            palette_path=None,
            width_arg=None,
            height_arg=None,
            palette_index=0,
            resize_to=resize,
        )

        # Update hash in manifest
        info["hash"] = current_hash

    # Save updated manifest
    manifest["assets"] = assets
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"  [save] manifest.json updated")


def main():
    """Processes all examples with assets."""
    repo_root = Path(__file__).parent.parent
    examples_dir = repo_root / "examples"

    converted = 0
    for example_dir in sorted(examples_dir.iterdir()):
        if not example_dir.is_dir() or example_dir.name == "common":
            continue
        prebuilt = example_dir / "prebuilt" / "manifest.json"
        if prebuilt.exists():
            bake_example(example_dir)
            converted += 1

    if converted == 0:
        print("[bake] No examples with prebuilt/manifest.json found")
    else:
        print(f"\n[bake] {converted} example(s) processed")


if __name__ == "__main__":
    main()
