#!/usr/bin/env python3
"""Saturn VDP1 resource profile and hardware-budget gates (host-only).

Converts measured hardware/runtime constraints into a model face budget so
the simplifier report can prove the accepted topology fits with headroom.
Limits come from the vendored VDP1 documentation and the current HAL:

- command-list capacity 512 (``saturn::internal::kCmdCapacity``), of which
  2 setup commands (local-coordinate + system-clip, re-issued every frame)
  and 1 END command are runtime overhead, not model data;
- one distorted-sprite command per visible textured triangle/quad;
- texture VRAM: 512 KiB VDP1 VRAM minus the 16 KiB command area, with the
  HAL's 8-byte cursor alignment applied per texture;
- palette CRAM: 8 banks of 256 RGB555 entries;
- sort scratch: caller-owned, sized per face (see the widened 16-bit sort
  path in the runtime; this profile counts faces, not bytes).

The final acceptance model must hold safety headroom: targeting 100% of
command capacity is a FAIL here, not a pass.
"""

from __future__ import annotations

from dataclasses import dataclass


VDP1_VRAM_BYTES = 512 * 1024
VDP1_COMMAND_AREA_BYTES = 16 * 1024
VDP1_TEXTURE_BUDGET_BYTES = VDP1_VRAM_BYTES - VDP1_COMMAND_AREA_BYTES
VDP1_MAX_TEXTURE_WIDTH = 504
VDP1_MAX_TEXTURE_HEIGHT = 255
PALETTE_BANKS_TOTAL = 8


@dataclass
class SaturnProfile:
    name: str = "saturn-vdp1"
    cmd_capacity: int = 512
    setup_commands: int = 2
    end_commands: int = 1
    hud_reserve: int = 8
    min_command_headroom: int = 16
    texture_budget_bytes: int = VDP1_TEXTURE_BUDGET_BYTES
    max_texture_fraction: float = 0.90
    palette_banks_total: int = PALETTE_BANKS_TOTAL
    max_pose_stream_bytes: int = 256 * 1024


def face_command_budget(profile: SaturnProfile, extra_reserve: int = 0) -> int:
    """Worst-case model faces: every face costs one VDP1 command."""
    return (
        profile.cmd_capacity
        - profile.setup_commands
        - profile.end_commands
        - profile.hud_reserve
        - profile.min_command_headroom
        - extra_reserve
    )


def vram_for_textures(sizes: list[int]) -> int:
    """HAL-matching estimate: 8-byte cursor alignment applied per texture."""
    return sum((s + 7) & ~7 for s in sizes)


def check_resources(
    profile: SaturnProfile,
    faces: int,
    texture_payload_bytes: int,
    palette_banks_used: int = 1,
    pose_stream_bytes: int = 0,
    texture_sizes: list[int] | None = None,
    extra_reserve_commands: int = 0,
) -> dict:
    """Gate one candidate against the Saturn profile. Never silent."""
    vram = (
        vram_for_textures(texture_sizes)
        if texture_sizes is not None
        else (texture_payload_bytes + 7) & ~7
    )
    model_commands = faces  # worst case: every face visible, one command each
    reserved = (
        profile.setup_commands + profile.end_commands + profile.hud_reserve + extra_reserve_commands
    )
    total = model_commands + reserved
    headroom = profile.cmd_capacity - total
    vram_limit = int(profile.texture_budget_bytes * profile.max_texture_fraction)
    failing: list[str] = []
    if faces > face_command_budget(profile, extra_reserve_commands):
        failing.append(
            f"faces {faces} exceed model face budget "
            f"{face_command_budget(profile, extra_reserve_commands)}"
        )
    if total > profile.cmd_capacity:
        failing.append(f"command total {total} exceeds capacity {profile.cmd_capacity}")
    if headroom < profile.min_command_headroom:
        failing.append(
            f"command headroom {headroom} < minimum {profile.min_command_headroom}"
        )
    if vram > vram_limit:
        failing.append(f"texture VRAM {vram} exceeds capped budget {vram_limit}")
    if palette_banks_used > profile.palette_banks_total:
        failing.append(
            f"palette banks {palette_banks_used} exceed {profile.palette_banks_total}"
        )
    if pose_stream_bytes > profile.max_pose_stream_bytes:
        failing.append(
            f"pose stream {pose_stream_bytes} exceeds {profile.max_pose_stream_bytes}"
        )
    return {
        "profile": profile.name,
        "worst_case_model_commands": model_commands,
        "reserved_commands": reserved,
        "total_command_estimate": total,
        "command_capacity": profile.cmd_capacity,
        "command_headroom": headroom,
        "texture_payload_bytes": texture_payload_bytes,
        "texture_vram_estimate": vram,
        "texture_budget_bytes": profile.texture_budget_bytes,
        "palette_banks_used": palette_banks_used,
        "pose_stream_bytes": pose_stream_bytes,
        "passed": not failing,
        "failing_gates": failing,
    }


def format_hard_failure(
    best_triangles: int, face_budget: int, detail: str = ""
) -> str:
    """The tool says FAIL with numbers instead of silently forcing a count."""
    msg = (
        "FAIL: no model candidate satisfies the selected Saturn profile\n"
        f"best quality-valid candidate: {best_triangles} triangles\n"
        f"available model face budget: {face_budget}"
    )
    if detail:
        msg += f"\n{detail}"
    return msg
