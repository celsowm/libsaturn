"""Checks if prebuilt assets are valid (hash of original asset matches)."""

import hashlib
import json
import sys
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    if len(sys.argv) < 2:
        print("invalid", end="")
        sys.exit(1)

    manifest_path = Path(sys.argv[1])
    if not manifest_path.exists():
        print("invalid", end="")
        sys.exit(1)

    manifest = json.loads(manifest_path.read_text())
    prebuilt_dir = manifest_path.parent
    assets = manifest.get("assets", {})

    if not assets:
        print("invalid", end="")
        sys.exit(1)

    for name, info in assets.items():
        # Resolve path relative to manifest directory
        input_path = Path(info["input"])
        if not input_path.is_absolute():
            input_path = (prebuilt_dir / input_path).resolve()

        if not input_path.exists():
            print("invalid", end="")
            sys.exit(1)

        current_hash = sha256_file(input_path)
        saved_hash = info.get("hash", "")

        if current_hash != saved_hash:
            print("stale", end="")
            sys.exit(0)

        # Check if pre-converted files exist
        output_prefix = prebuilt_dir / info["output"]
        if not (
            output_prefix.with_suffix(".c").exists()
            and output_prefix.with_suffix(".h").exists()
        ):
            print("missing", end="")
            sys.exit(0)

    print("ok", end="")


if __name__ == "__main__":
    main()
