#!/usr/bin/env python3
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.ikemen_sff import air


def main() -> int:
    source = """[Begin Action 0]
0,0,0,0,1,,A
LoopStart
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
    assert actions[0].loop_start == 1
    print("[test] ikemen AIR drawtypes OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


def test_clsn_header_decides_box_type() -> None:
    # KFM's blocking actions list `Clsn2[n]` lines under a `Clsn1: n` header;
    # upstream reads them as Clsn1 boxes.
    source = """[Begin Action 1300]
Clsn1: 2
 Clsn2[0] =   0,  1, 34,-84
 Clsn2[1] =  13,-97, 47,-65
Clsn2: 1
 Clsn2[0] =   0,  1, 34,-84
1300,0, 0,0, 2
"""
    with TemporaryDirectory() as tmp:
        path = Path(tmp) / "kfm.air"
        path.write_text(source, encoding="utf-8")
        frame = air.parse(path)[1300].frames[0]
    assert len(frame.clsn1) == 2
    assert len(frame.clsn2) == 1
