r"""Acceptance for Slave scheduling (examples/parallel_sched_demo) on Ymir's two
real SH-2s. Run through the wrapper:

    .\harness\run-parallel-sched.ps1 -Bios .\bios\saturn_bios_us.bin

The guest submits tasks with priorities and dependencies, and splits a loop
across both CPUs; the struct read here is g_sched_demo.
"""

import os
import re
import struct
import unittest

MAGIC = 0x53434831  # "SCH1"
WRAM_HIGH = 0x06000000
FIELDS = (
    "magic", "init_status", "slave_ready",
    "prio_low", "prio_high", "prio_normal", "prio_urgent",
    "prio_on_slave", "prio_signals", "prio_batched",
    "dep_low_ordinal", "dep_urgent_ordinal", "dep_other_ordinal",
    "master_dep_slave_ordinal", "master_dep_master_ordinal", "master_dep_on_slave",
    "failed_dep_wait_result", "failed_dep_state", "dependency_cancels",
    "for_single_ticks", "for_parallel_ticks", "for_match", "for_slave_elements",
    "for_small_slave_elements", "for_status", "errors", "frames",
)
SAT_ERR_NOT_FOUND = 6         # as -result
STATE_CANCELLED = 5


def _load():
    dump = os.environ.get("LIBSATURN_SCHED_DUMP")
    map_path = os.environ.get("LIBSATURN_SCHED_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-parallel-sched.ps1 first")
    if not os.path.exists(dump) or not os.path.exists(map_path):
        raise AssertionError("missing input: %s / %s" % (dump, map_path))
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_sched_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_sched_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    with open(dump, "rb") as stream:
        raw = struct.unpack(">%dI" % len(FIELDS), stream.read()[offset:offset + 4 * len(FIELDS)])
    return dict(zip(FIELDS, raw))


class ParallelSchedTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load()
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_sched_demo not initialised: %r" % cls.r)

    def test_the_slave_runs_and_nothing_failed(self):
        r = self.r
        self.assertEqual(r["init_status"], 0)
        self.assertEqual(r["slave_ready"], 1, "the Slave never started: %r" % r)
        self.assertEqual(r["errors"], 0, r)
        self.assertGreater(r["frames"], 10)

    def test_queued_tasks_run_by_priority_in_one_signal(self):
        r = self.r
        self.assertLess(r["prio_urgent"], r["prio_high"])
        self.assertLess(r["prio_high"], r["prio_normal"])
        self.assertLess(r["prio_normal"], r["prio_low"])
        self.assertEqual(r["prio_on_slave"], 4)
        # one signal for the blocker, one for the batch of four
        self.assertEqual(r["prio_signals"], 2)
        self.assertEqual(r["prio_batched"], 4)

    def test_a_dependency_runs_before_its_dependent_whatever_the_priority(self):
        r = self.r
        self.assertLess(r["dep_low_ordinal"], r["dep_urgent_ordinal"])
        # the unrelated high-priority task is not held back by them
        self.assertLess(r["dep_other_ordinal"], r["dep_low_ordinal"])

    def test_a_master_task_can_wait_for_a_slave_task(self):
        r = self.r
        self.assertLess(r["master_dep_slave_ordinal"], r["master_dep_master_ordinal"])
        self.assertEqual(r["master_dep_on_slave"], 0)

    def test_a_failed_dependency_cancels_the_dependent(self):
        r = self.r
        self.assertEqual(r["failed_dep_wait_result"], SAT_ERR_NOT_FOUND)
        self.assertEqual(r["failed_dep_state"], STATE_CANCELLED)
        self.assertEqual(r["dependency_cancels"], 1)

    def test_parallel_for_is_exact_and_uses_both_cpus(self):
        r = self.r
        self.assertEqual(r["for_status"], 0)
        self.assertEqual(r["for_match"], 1)
        self.assertEqual(r["for_slave_elements"], 4096)       # the second half
        self.assertEqual(r["for_small_slave_elements"], 0)    # below the grain: Master only

    def test_parallel_for_is_faster_than_one_cpu(self):
        r = self.r
        self.assertGreater(r["for_single_ticks"], 1000)
        self.assertLess(r["for_parallel_ticks"], 0.8 * r["for_single_ticks"],
                        "single %d, parallel %d ticks" % (r["for_single_ticks"], r["for_parallel_ticks"]))


if __name__ == "__main__":
    unittest.main()
