#!/usr/bin/env python3
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.ikemen_sff import air


def main() -> int:
    source = """[Begin Action 0]
0,0,0,0,1,,A
0,1,0,0,1,H,S
"""
    with TemporaryDirectory() as tmp:
        path = Path(tmp) / "fx.air"
        path.write_text(source, encoding="utf-8")
        actions = air.parse(path)
    frames = actions[0].frames
    assert len(frames) == 2
    assert frames[0].blend_mode == air.AIR_BLEND_ADD
    assert not frames[0].flip_h
    assert frames[1].blend_mode == air.AIR_BLEND_SUBTRACT
    assert frames[1].flip_h
    print("[test] ikemen AIR drawtypes OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
