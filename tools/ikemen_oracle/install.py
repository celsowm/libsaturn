#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
from pathlib import Path

BEGIN_MARKER = "// LIBSATURN_ORACLE_BEGIN"
END_MARKER = "// LIBSATURN_ORACLE_END"

BEFORE_SETUP = """	// Setup characters
	s.SetupCharRoundStart()
"""

PATCHED_SETUP = f"""	{BEGIN_MARKER}
	if libsaturnOracleEnabled() {{
		if err := libsaturnOracleBeginMatch(s); err != nil {{
			panic(err)
		}}
	}}
	{END_MARKER}

""" + BEFORE_SETUP

AFTER_ACTION = """		// Update game state
		s.action()
"""

PATCHED_ACTION = AFTER_ACTION + f"""
		{BEGIN_MARKER}
		if libsaturnOracleEnabled() &&
			libsaturnOracleCaptureFrame(s) {{
			s.fightLoopEnd = true
			s.endMatch = true
		}}
		{END_MARKER}
"""

def remove_marked_blocks(text: str) -> str:
    while BEGIN_MARKER in text:
        start = text.index(BEGIN_MARKER)
        line_start = text.rfind("\n", 0, start) + 1
        end = text.index(END_MARKER, start) + len(END_MARKER)
        line_end = text.find("\n", end)
        if line_end < 0:
            line_end = len(text)
        else:
            line_end += 1
        text = text[:line_start] + text[line_end:]
    return text

def install(root: Path, source_hook: Path) -> None:
    system_go = root / "src" / "system.go"
    hook_dst = root / "src" / "libsaturn_oracle.go"
    if not system_go.is_file():
        raise SystemExit(f"Ikemen GO system.go not found: {system_go}")

    text = remove_marked_blocks(system_go.read_text(encoding="utf-8"))
    if BEFORE_SETUP not in text:
        raise SystemExit("Ikemen GO setup anchor changed; refusing unsafe patch")
    if AFTER_ACTION not in text:
        raise SystemExit("Ikemen GO action anchor changed; refusing unsafe patch")

    text = text.replace(BEFORE_SETUP, PATCHED_SETUP, 1)
    text = text.replace(AFTER_ACTION, PATCHED_ACTION, 1)
    system_go.write_text(text, encoding="utf-8")
    shutil.copyfile(source_hook, hook_dst)
    print(f"installed oracle hook in {root}")

def uninstall(root: Path) -> None:
    system_go = root / "src" / "system.go"
    hook_dst = root / "src" / "libsaturn_oracle.go"
    if system_go.is_file():
        text = system_go.read_text(encoding="utf-8")
        text = remove_marked_blocks(text)
        system_go.write_text(text, encoding="utf-8")
    if hook_dst.exists():
        hook_dst.unlink()
    print(f"removed oracle hook from {root}")

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--ikemen-dir",
        type=Path,
        default=Path(".external/Ikemen-GO"),
    )
    parser.add_argument(
        "--uninstall",
        action="store_true",
    )
    args = parser.parse_args()

    tool_dir = Path(__file__).resolve().parent
    root = args.ikemen_dir.resolve()
    if args.uninstall:
        uninstall(root)
    else:
        install(root, tool_dir / "oracle_hook.go")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
