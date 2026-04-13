"""Generates .cue file for an ISO."""

import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Generate CUE sheet for ISO")
    parser.add_argument(
        "--iso-name", required=True, help="ISO filename (e.g., mvp.iso)"
    )
    parser.add_argument("--cue-output", required=True, help="Output .cue path")
    args = parser.parse_args()

    cue_content = (
        f'FILE "{args.iso_name}" BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n'
    )

    Path(args.cue_output).write_text(cue_content)


if __name__ == "__main__":
    main()
