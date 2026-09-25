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


def all_sources(example: str) -> str:
    """Every .c file of an example split across modules."""
    return "\n".join(
        path.read_text(encoding="utf-8")
        for path in sorted((ROOT / "examples" / example).glob("*.c"))
    )


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
        "sat_scene_submit_instance(",
        "sat_scene_submit_quad(",
        "sat_scene_submit_tiled_quad(",
        "sat_scene_submit_box(",
        "sat_scene_flush(",
        "sat_scene_begin(",
        "sat_scene_depth(",
        "sat_scene3d_solid_pool_register(",
        "sat_anim_prepare_model_instance(",
        "sat_upload_indexed8_grid(",
        "sat_vdp1_overlay_begin(",
        "sat_vdp2_bitmap_upload_indexed8(",
    ):
        assert symbol in text, f"Skybridge must use library API {symbol}"
    assert '#include "saturn/follow_camera3d.h"' in text
    assert "sat_follow_camera3d_step(" in text
    assert "g_camera_anchor" not in text
    assert "sat_clip_quad_near(" not in text, (
        "Skybridge must not implement independent near-plane face clipping"
    )
    assert "sat_clip_quad_screen(" not in text, (
        "Skybridge must not implement independent screen-space clipping"
    )
    assert "sat_scene3d_queue_" not in text, (
        "Skybridge must submit directly to its single face-scene planner"
    )
    assert "g_fade_textures[" not in text and "g_pig_textures[" not in text, (
        "Scene colour textures must share the canonical deduplicating pool"
    )
    assert "sat_draw_mesh(" not in text and "sat_draw_indexed_solid_mesh3(" not in text, (
        "Skybridge must submit all model faces to the shared painter"
    )
    assert "sat_draw_indexed_box3(" not in text, (
        "Box faces belong to the shared face painter"
    )
    assert "pquad(&q,rx,y,bz" not in text, (
        "Box face winding belongs to the renderer, not to Skybridge"
    )
    assert "shades=&anim->face_shades[" not in text, (
        "Animated face material mapping belongs to the instance runtime"
    )


def assert_2d_state_and_manifest_owners_are_used() -> None:
    runtime_2d = source("runtime_2d")
    assert '#include "saturn/sprite_anim.h"' in runtime_2d
    assert "sat_sprite_region_anim_init(" in runtime_2d
    assert "sat_sprite_region_anim_source(" in runtime_2d
    assert "((now / 180u) & 3u) * 16u" not in runtime_2d
    assert "sat_input_poll(" not in runtime_2d
    assert "sat_pad_poll_port(" not in runtime_2d

    jukebox = source("cd_streaming_jukebox")
    assert "sat_asset_register_manifest(" in jukebox

    runtime_3d = source("runtime_3d")
    assert '#include "saturn/hud.h"' in runtime_3d
    assert "sat_hud_text(" in runtime_3d and "sat_hud_value(" in runtime_3d
    assert '#include "saturn/orbit_camera3d.h"' in runtime_3d
    assert "sat_orbit_camera3d_fit_bounds(" in runtime_3d
    assert "sat_orbit_camera3d_apply_pad(" in runtime_3d
    assert "sat_camera3d_init(" not in runtime_3d
    assert "sat_mat4_look_at(" not in runtime_3d

    explorer = source("infinite_explorer")
    assert "sat_vdp2_ground_environment_init(" in explorer
    assert "sat_vdp2_ground_environment_commit_params(" in explorer
    assert "sat_vdp2_vram_write_words(" not in explorer
    assert "sat_sort_indices_desc(" in explorer
    assert "while (j >= 0 && g_render[j].p.depth < item.p.depth)" not in explorer


def assert_legacy_scene_routes_are_gone_from_code() -> None:
    for path in (
        ROOT / "include" / "saturn" / "scene3d.h",
        ROOT / "src" / "graphics" / "3d" / "scene" / "api.cpp",
        ROOT / "include" / "saturn" / "mesh3d.h",
        ROOT / "src" / "graphics" / "3d" / "geometry" / "mesh.cpp",
    ):
        text = path.read_text(encoding="utf-8")
        assert "sat_scene3d_queue_" not in text, path
        assert "sat_draw_mesh" not in text, path
        assert "sat_scene3d_t" not in text, path
        assert "sat_scene3d_draw_model" not in text, path


def assert_pacman_uses_persistent_actor_meshes() -> None:
    text = all_sources("pacman_3d")
    # Meshes are built once at init (pac_model.c / ghost_model.c) and posed
    # per frame through an instance world matrix, never rebuilt per frame.
    assert "p3d_pac_init" in text and "p3d_ghost_init" in text
    assert "sat_mesh_build_sphere_wedge" in text
    assert "instance.world = &world" in text
    assert "build_pac(actor" not in text
    assert "sat_view_cache_begin" in text
    assert "sat_view_cache_sort" in text
    assert "sat_scene_replay_view_item(" in text
    assert "sat_draw_quad2_polygon(" not in text


def assert_skybridge_uses_shared_surface_math() -> None:
    text = source("skybridge_3d")
    game = (ROOT / "examples" / "skybridge_3d" / "game.h").read_text(encoding="utf-8")
    assert "sat_surface3d_height" in game
    assert "sat_surface3d_split" in game
    assert "sat_surface3d_supports_footprint" in game
    assert "sat_surface3d_" not in text or "sat_surface3d" in game


def assert_vdp2_environment_layout_is_validated() -> None:
    for name in ("skybridge_3d", "infinite_explorer"):
        text = source(name)
        assert "sat_vdp2_ground_environment_validate_layout" in text


def assert_cd_jukebox_uses_source_manifest() -> None:
    text = source("cd_streaming_jukebox")
    assert "sat_cdfs_register_source_manifest" in text
    assert "sat_cdfs_lookup(&g_volume" not in text


def assert_physics3d_uses_canonical_scene() -> None:
    text = source("physics_3d")
    assert "sat_scene_init(" in text
    assert "sat_scene_begin(" in text
    assert "sat_scene_submit_quad(" in text
    assert "sat_scene_submit_instance(" in text
    assert "sat_scene_flush(" in text
    assert "sat_draw_world_polygon(" not in text


def assert_distance_fade_uses_canonical_scene() -> None:
    text = source("distance_fade_3d")
    assert "sat_scene_init(" in text
    assert "sat_scene_begin(" in text
    assert "sat_scene_depth(" in text
    assert "sat_scene_submit_quad(" in text
    assert "sat_scene_flush(" in text
    assert "sat_project_quad(" not in text
    assert "sat_draw_sprite_distorted_color_calc(" not in text


def main() -> None:
    assert_no_example_to_example_dependency()
    assert_games_do_not_address_vdp2_mmio()
    assert_shared_orbit_camera_is_used()
    assert_skybridge_uses_renderer_and_overlay_budget()
    assert_2d_state_and_manifest_owners_are_used()
    assert_legacy_scene_routes_are_gone_from_code()
    assert_pacman_uses_persistent_actor_meshes()
    assert_skybridge_uses_shared_surface_math()
    assert_vdp2_environment_layout_is_validated()
    assert_cd_jukebox_uses_source_manifest()
    assert_physics3d_uses_canonical_scene()
    assert_distance_fade_uses_canonical_scene()
    explorer = source("infinite_explorer")
    assert "sat_anim_prepare_model_instance(" in explorer
    assert "for (i = 0; i < EGGMAN_VERTEX_COUNT;" not in explorer
    print("PASS: test_refactor_api_ownership.py (14 architecture gates)")


if __name__ == "__main__":
    main()
