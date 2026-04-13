"""Checks if the binary does not exceed the maximum size."""

import argparse
import sys
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Check binary size")
    parser.add_argument("--bin", required=True, help="Path to binary")
    parser.add_argument(
        "--max-size", type=int, default=983040, help="Max size in bytes"
    )
    args = parser.parse_args()

    size = Path(args.bin).stat().st_size
    print(f"[check] {args.bin} size={size} bytes (max {args.max_size})")

    if size > args.max_size:
        sys.exit(1)


if __name__ == "__main__":
    main()
