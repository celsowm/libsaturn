#!/usr/bin/env python3
"""Source-level regressions for Skybridge's single-owner task lifecycle."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN = (ROOT / "examples" / "skybridge_3d" / "main.c").read_text(
    encoding="utf-8"
)
DOC = (ROOT / "docs" / "SKYBRIDGE_PARALLEL_INTEGRATION.md").read_text(
    encoding="utf-8"
)


def test_animation_job_is_not_stack_owned():
    assert "sat_anim_decode_job_t job;" not in MAIN
    assert "static sat_anim_decode_job_t g_pig_anim_job;" in MAIN
    assert "static sat_anim_state_t g_pig_anim_job_state;" in MAIN
    assert "sat_anim_decode_async(&g_pig_anim_job" in MAIN
    assert "g_pig_anim_job_state=g_pig_anim;" in MAIN


def test_pig_is_prepared_when_geometry_uses_slave():
    assert "if(g_gem_slave_pending) prepare_pig_animation_master();" in MAIN
    assert "else submit_pig_animation();" in MAIN
    assert "g_pig_render_anim=g_pig_anim_job_state;" in MAIN


def test_failed_geometry_submission_never_publishes_unprepared_batch():
    fallback = MAIN.split(
        "const sat_result_t gem_submit=sat_scene_prepare_batch_async", 1
    )[1].split(
        "} else {", 1
    )[0]
    assert "sat_scene3d_prepare_batch_execute(&g_gem_slave_batch)==SAT_OK" in fallback
    assert "g_gem_slave_ready=1u" in fallback
    assert "g_gem_master_ready=1u" not in fallback
    assert "g_gem_master_ready=1u" in MAIN


def test_running_tasks_keep_ownership_after_recovery_failure():
    assert "if(state==SAT_PARALLEL_RUNNING)" in MAIN
    assert "parallel_recovery_failed();" in MAIN
    assert "g_gem_slave_pending=0u;" in MAIN
    assert "g_pig_anim_pending=0u;" in MAIN
    assert "g_parallel_recovery_blocked=1u;" in MAIN


def test_timing_units_are_separate_and_documented():
    assert "master_wait_frt_ticks" in MAIN
    assert "g_metrics.wait_ms=stats.master_wait_ticks" not in MAIN
    assert "raw SH-2 FRT delta" in MAIN
    assert "millisecond timer" in DOC
    assert "FRT" in DOC


if __name__ == "__main__":
    for test in (
        test_animation_job_is_not_stack_owned,
        test_pig_is_prepared_when_geometry_uses_slave,
        test_failed_geometry_submission_never_publishes_unprepared_batch,
        test_running_tasks_keep_ownership_after_recovery_failure,
        test_timing_units_are_separate_and_documented,
    ):
        test()
    print("PASS: test_skybridge_parallel_followup.py (5 lifecycle gates)")
