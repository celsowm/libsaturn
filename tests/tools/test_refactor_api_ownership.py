#!/usr/bin/env python3
"""Architectural regression gate for example-driven public API migration.

This is intentionally source-level: the cross-build validates semantics, while
these checks prevent a future game from reintroducing known duplicate generic
rendering/camera/hardware implementations after a migrated slice.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
GAMES = (
    "skybridge_3d",
    "infinite_explorer",
    "basic_3d_texture",
    "basic_3d_animation",
)
RBG0_DEMOS = ("vdp2_rbg0_ground", "vdp2_nbg0_rbg0_combo")


def source(example: str) -> str:
    return (ROOT / "examples" / example / "main.c").read_text(encoding="utf-8")


def assert_no_example_to_example_dependency() -> None:
    for name in GAMES + RBG0_DEMOS:
        text = source(name)
        assert not re.search(
            r'^\s*#\s*include\s*["<]examples/[^">]+[">]',
            text,
            re.MULTILINE,
        ), f"{name}: example-to-example include has leaked into game code"


def assert_games_do_not_address_vdp2_mmio() -> None:
    for name in ("skybridge_3d", "infinite_explorer") + RBG0_DEMOS:
        text = source(name)
        assert "0x25E00000" not in text, (
            f"{name}: use sat_vdp2_vram_write_words / "
            "sat_vdp2_bitmap_upload_indexed8, not direct VDP2 MMIO"
        )


def assert_shared_orbit_camera_is_used() -> None:
    for name in ("basic_3d_texture", "basic_3d_animation"):
        text = source(name)
        assert '#include "saturn/orbit_camera3d.h"' in text, name
        assert "sat_orbit_camera3d_apply_pad(" in text, name
        assert "sat_orbit_camera3d_fit_bounds(" in text, name
        assert "sat_mat4_look_at(" not in text, (
            f"{name}: copied orbit camera matrices into the viewer"
        )
        assert "static void compute_camera(" not in text, (
            f"{name}: copied per-example camera implementation"
        )


def assert_skybridge_uses_renderer_and_overlay_budget() -> None:
    text = source("skybridge_3d")
    for symbol in (
        "sat_mesh_build_octahedron(",
        "sat_draw_indexed_solid_mesh3(",
        "sat_draw_indexed_tiled_quad3(",
        "sat_upload_indexed8_quadrants(",
        "sat_vdp1_reserve_overlay_commands(",
        "sat_vdp1_overlay_begin(",
        "sat_vdp2_bitmap_upload_indexed8(",
    ):
        assert symbol in text, f"Skybridge must use library API {symbol}"
    assert "sat_clip_quad_near(" not in text, (
        "Skybridge must not implement independent near-plane face clipping"
    )
    assert "sat_clip_quad_screen(" not in text, (
        "Skybridge must not implement independent screen-space clipping"
    )


def main() -> None:
    assert_no_example_to_example_dependency()
    assert_games_do_not_address_vdp2_mmio()
    assert_shared_orbit_camera_is_used()
    assert_skybridge_uses_renderer_and_overlay_budget()
    print("PASS: test_refactor_api_ownership.py (4 architecture gates)")


if __name__ == "__main__":
    main()
