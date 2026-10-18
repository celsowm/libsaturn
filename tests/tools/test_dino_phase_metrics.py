#!/usr/bin/env python3
import struct
import sys
import unittest
import json
import tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'harness/scripts'))
from dino_phase_metrics import decode, compare_runs, FIELDS, ROWS


def capture(count=3, change=None):
    raw=bytearray(16+4*ROWS*len(FIELDS))
    struct.pack_into('>4I',raw,0,0x44505231,1,210000,count)
    for serial in range(max(0,count-ROWS),count):
        row=dict.fromkeys(FIELDS,0)
        row.update(serial=serial,field=serial*3,collect=10,publish=100,
                   master_geometry=500,join=50,merge=20,flush=200,
                   painter=20,emit=100,present=100,work=1000,
                   master_ccr=1,master_tcr=2,worker_valid=1,
                   worker_source=50,worker_geometry=300,worker_output=10,
                   worker_total=360,worker_ccr=0,worker_tcr=2)
        if change:change(row)
        struct.pack_into('>'+str(len(FIELDS))+'I',raw,
                         16+(serial%ROWS)*4*len(FIELDS),*(row[key] for key in FIELDS))
    return raw


class PhaseTests(unittest.TestCase):
    def test_overlap_is_reported_separately(self):
        result=decode(capture())
        self.assertEqual(result['samples'],3)
        self.assertEqual(result['observed_ccr'],dict(master=[1],worker=[0]))
        self.assertAlmostEqual(result['phase_us']['join']['median'],50*1e6/210000)

    def test_next_ring_slot_can_be_partially_overwritten(self):
        raw=capture(70)
        offset=16+(70%ROWS)*4*len(FIELDS)
        raw[offset:offset+4*len(FIELDS)]=b'\xff'*(4*len(FIELDS))
        self.assertEqual(decode(raw)['samples'],63)

    def test_incomplete_worker_rejected(self):
        with self.assertRaises(ValueError):decode(capture(change=lambda r:r.update(worker_valid=0)))

    def test_possible_timer_wrap_rejected(self):
        with self.assertRaises(ValueError):decode(capture(change=lambda r:r.update(work=100000,join=70000)))

    def test_inconsistent_worker_components_rejected(self):
        with self.assertRaises(ValueError):decode(capture(change=lambda r:r.update(worker_total=359)))

    def test_clock_prescaler_mismatch_rejected(self):
        with self.assertRaises(ValueError):decode(capture(change=lambda r:r.update(worker_tcr=1)))

    def test_response_wait_outside_join_rejected(self):
        with self.assertRaises(ValueError):decode(capture(change=lambda r:r.update(join_response=51)))

    def test_inconsistent_flush_rejected(self):
        with self.assertRaises(ValueError):decode(capture(change=lambda r:r.update(emit=190)))

    def test_comparison_checks_scenario_emulator_and_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            before=Path(directory)/'before';after=Path(directory)/'after'
            metadata=dict(scenario='animated',bios_sha256='bios',probe_sha256='probe',
                          checkpoints=[dict(fields=600),dict(fields=1200)],
                          nominal_submissions_per_second=20)
            for path in (before,after):
                path.mkdir()
                (path/'analysis.json').write_text(json.dumps(metadata))
                (path/'field-1200_g_dino_pipeline.bin').write_bytes(capture())
            (after/'field-1200_g_dino_pipeline.bin').write_bytes(
                capture(change=lambda r:r.update(emit=50)))
            result=compare_runs(before,after)
            self.assertAlmostEqual(result['phase_costs']['emit']['median_change_percent'],-50)
            for key,value in (('scenario','paused'),('bios_sha256','other'),
                              ('probe_sha256','other'),('checkpoints',[dict(fields=900)])):
                (after/'analysis.json').write_text(json.dumps(dict(metadata,**{key:value})))
                with self.assertRaises(ValueError):compare_runs(before,after)
            (after/'analysis.json').write_text(json.dumps(metadata))
            (after/'field-1200_g_dino_pipeline.bin').write_bytes(
                capture(change=lambda r:r.update(worker_ccr=1)))
            with self.assertRaises(ValueError):compare_runs(before,after)


if __name__=='__main__':unittest.main()
