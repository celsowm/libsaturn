#!/usr/bin/env python3
from __future__ import annotations

import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.ikemen_sff import __main__ as cli  # noqa: E402
from tools.ikemen_sff import air as air_mod  # noqa: E402
from tools.ikemen_sff import emit as emit_mod  # noqa: E402
from tools.ikemen_sff import sff as sff_mod  # noqa: E402


NODE = sff_mod.SpriteNode(
    group=10,
    number=20,
    width=1,
    height=1,
    xoff=3,
    yoff=4,
    palette_link=0,
    fmt=sff_mod.FORMAT_RAW,
    coldepth=8,
    data_offset=0,
    data_length=1,
    palette_index=0,
    flags=0,
)


class FakeContainer:
    palette_count = 1

    def sprite_nodes(self):
        yield NODE

    def sprite_data(self, node):
        assert node is NODE
        return b"\x00"


def _action():
    return {
        0: air_mod.AirAction(
            0,
            frames=[air_mod.AirFrame(10, 20, 0, 0, 1)],
        )
    }


def _sprite_asset():
    return emit_mod.SpriteAsset(
        width=1,
        height=1,
        padded_width=8,
        left_pad=3,
        format=sff_mod.FORMAT_RAW,
        data=b"\x00",
    )


def test_fx_uses_indexed_nodes() -> None:
    container = FakeContainer()
    calls = []

    originals = (
        cli.sff_mod.load,
        cli.air_mod.parse,
        cli._runtime_sprite_asset,
        cli.pal_mod.materialize_index,
        cli.emit_mod.emit_frames,
    )
    try:
        cli.sff_mod.load = lambda _: container
        cli.air_mod.parse = lambda _: _action()

        def runtime(container_arg, node, all_nodes, node_index):
            assert container_arg is container
            assert node is NODE
            assert all_nodes == [NODE]
            assert node_index == 0
            calls.append(node_index)
            return _sprite_asset()

        cli._runtime_sprite_asset = runtime
        cli.pal_mod.materialize_index = lambda *_: [0] * 256
        cli.emit_mod.emit_frames = (
            lambda *_args, **_kwargs: ("/* c */", "/* h */", b"\x7f")
        )

        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            args = SimpleNamespace(
                sff="dummy.sff",
                air="dummy.air",
                actions=[0],
                out_prefix=str(root / "fightfx"),
                symbol="fightfx",
                sprite_bin=str(root / "FIGHTFX.BIN"),
            )
            assert cli.cmd_fx(args) == 0
            assert calls == [0]
            assert (root / "FIGHTFX.BIN").read_bytes() == b"\x7f"
    finally:
        (
            cli.sff_mod.load,
            cli.air_mod.parse,
            cli._runtime_sprite_asset,
            cli.pal_mod.materialize_index,
            cli.emit_mod.emit_frames,
        ) = originals


def test_char_png_unpacks_indexed_node() -> None:
    container = FakeContainer()
    png_nodes = []

    originals = (
        cli.sff_mod.load,
        cli.air_mod.parse,
        cli._load_palette,
        cli._runtime_sprite_asset,
        cli.emit_mod.emit_frames,
        cli.decode,
        cli._png_dump_indexed,
    )
    try:
        cli.sff_mod.load = lambda _: container
        cli.air_mod.parse = lambda _: _action()
        cli._load_palette = lambda *_: (0, [0] * 256)
        cli._runtime_sprite_asset = (
            lambda *_args, **_kwargs: _sprite_asset()
        )
        cli.emit_mod.emit_frames = (
            lambda *_args, **_kwargs: ("/* c */", "/* h */", b"\x01")
        )
        cli.decode = lambda node, data: (
            png_nodes.append(node) or b"\x00"
        )
        cli._png_dump_indexed = lambda *_args, **_kwargs: None

        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            args = SimpleNamespace(
                sff="dummy.sff",
                air="dummy.air",
                actions=[0],
                palettes=[(1, 1)],
                out_prefix=str(root / "kfm"),
                symbol="kfm",
                sprite_bin=str(root / "KFM_SPR.BIN"),
                png_dir=str(root / "png"),
                png_limit=1,
            )
            assert cli.cmd_char(args) == 0
            assert png_nodes == [NODE]
    finally:
        (
            cli.sff_mod.load,
            cli.air_mod.parse,
            cli._load_palette,
            cli._runtime_sprite_asset,
            cli.emit_mod.emit_frames,
            cli.decode,
            cli._png_dump_indexed,
        ) = originals


def main() -> int:
    test_fx_uses_indexed_nodes()
    test_char_png_unpacks_indexed_node()
    print("ikemen SFF CLI indexing: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
