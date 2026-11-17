"""Validate the real target kernels through the WRAM telemetry."""
import json
import os
from pathlib import Path
import unittest
from harness.scripts import telemetry as tel

NAMES = ("rigid", "blend", "clip_joints", "paint_range", "paint_spread",
         "paint_quantise", "bounds", "screen_cull", "program_run", "adx")
LANES = ("serial_master", "parallel_master", "parallel_slave")

def decode(words):
    if len(words) != 176:
        raise ValueError("invalid telemetry size")
    report = dict(zip(("magic", "version", "done", "init_status", "slave_ready",
                       "parallel_status", "frequency", "reserved"), words[:8]))
    report["lanes"] = {}
    for i, name in enumerate(LANES):
        offset = 8 + i * 56
        lane = dict(zip(("on_slave", "sr", "ccr", "done"), words[offset:offset+4]))
        lane["kernels"] = {}
        for j, kernel in enumerate(NAMES):
            start = offset + 4 + 5*j
            lane["kernels"][kernel] = dict(zip(
                ("cases", "errors", "first_case", "asm_ticks", "reference_ticks"),
                words[start:start+5]))
        report["lanes"][name] = lane
    report["scope"] = "Ymir target differential; inclusive FRT sums are diagnostic, not isolated benchmark speedups"
    return report

def validate(r):
    assert (r["magic"], r["version"], r["done"]) == (0x534B5631, 1, 1), r
    assert (r["init_status"], r["slave_ready"], r["parallel_status"]) == (0, 1, 0), r
    assert r["frequency"] > 0, r
    for i, name in enumerate(LANES):
        lane = r["lanes"][name]
        assert (lane["on_slave"], lane["done"]) == (int(i == 2), 1), (name, lane)
        assert not lane["sr"] & 2, (name, "SR.S set", lane)
        for kernel, row in lane["kernels"].items():
            assert (row["cases"], row["errors"]) == (128, 0), (name, kernel, row)

class KernelVerifyTest(unittest.TestCase):
    def test_target(self):
        dump = os.environ.get("LIBSATURN_SH2_KERNEL_VERIFY_DUMP")
        map_path = os.environ.get("LIBSATURN_SH2_KERNEL_VERIFY_MAP")
        if not dump or not map_path:
            self.skipTest("run harness/run-sh2-kernel-verify.ps1")
        words = tel.load_dump(dump, map_path, "_g_sh2_kernel_verify", ">176I")
        report = decode(words)
        output = Path(dump).with_name("analysis.json")
        output.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
        validate(report)

    def test_decoder_rejects_incomplete_or_wrong_cpu(self):
        words = [0]*176
        self.assertRaises(AssertionError, validate, decode(words))
        words[:8] = [0x534B5631,1,1,0,1,0,210540,0]
        for i in range(3):
            offset = 8+i*56
            words[offset:offset+4] = [int(i==2),0,1,1]
            for j in range(10):
                words[offset+4+j*5:offset+9+j*5] = [128,0,0,1,1]
        validate(decode(words))
        words[8+2*56] = 0
        self.assertRaises(AssertionError, validate, decode(words))
