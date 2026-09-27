"""Generates a single-track raw MODE1/2352 CUE sheet."""

import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Generate CUE sheet for raw CD BIN")
    parser.add_argument(
        "--bin-name", required=True, help="Raw BIN filename (e.g., mvp.bin)"
    )
    parser.add_argument("--cue-output", required=True, help="Output .cue path")
    args = parser.parse_args()

    cue_content = (
        f'FILE "{args.bin_name}" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n'
    )

    Path(args.cue_output).write_text(cue_content)


if __name__ == "__main__":
    main()
