#!/usr/bin/env python3
"""Print the CHANGELOG.md section for one version, for release notes.

Usage:
    python scripts/release-notes.py 0.1.0
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def section(changelog: str, version: str) -> str:
    heading = re.compile(rf"^## \[{re.escape(version)}\][^\n]*$", re.M)
    match = heading.search(changelog)
    if not match:
        raise SystemExit(f"CHANGELOG.md has no section for {version}")
    following = re.search(r"^## \[", changelog[match.end():], re.M)
    end = match.end() + following.start() if following else len(changelog)
    return changelog[match.start():end].strip() + "\n"


def main() -> int:
    if len(sys.argv) != 2:
        print((__doc__ or "").strip(), file=sys.stderr)
        return 2
    sys.stdout.write(section((ROOT / "CHANGELOG.md").read_text(encoding="utf-8"),
                             sys.argv[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
