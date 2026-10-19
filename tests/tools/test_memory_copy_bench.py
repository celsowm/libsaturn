#!/usr/bin/env python3
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'harness/scripts'))
from memory_copy_bench import decode,compare,ROWS,LENGTHS


def capture():
    words=[0x4d434231,1,210540,ROWS,1,0,1,ROWS]
    for sr in range(2):
        for dr in range(2):
            for so in range(4):
                for do in range(4):
                    for length in LENGTHS:
                        words.extend([sr,dr,so,do,length,512 if length<=32 else 32,
                                      98,100,102,100,200,length+so+do,0])
    return bytearray(struct.pack('>'+str(len(words))+'I',*words))

class CopyBenchTests(unittest.TestCase):
    def test_decode_complete_matrix_uses_median(self):
        result=decode(capture())
        self.assertEqual(len(result['cases']),512)
        self.assertEqual(result['cases'][0]['median_ticks'],100)
        self.assertAlmostEqual(result['cases'][0]['median_us_per_call'],100e6/210540/512)

    def test_reject_partial_failed_and_bad_frequency(self):
        raw=capture()
        with self.assertRaises(ValueError):decode(raw[:-4])
        for index,value in [(2,0),(3,511),(4,0),(5,1),(7,513)]:
            changed=raw.copy();struct.pack_into('>I',changed,index*4,value)
            with self.assertRaises(ValueError):decode(changed)

    def test_reject_bad_case_and_duplicates(self):
        for offset,value in [(0,2),(3,4),(4,19),(5,1),(6,65536),(12,1)]:
            raw=capture();struct.pack_into('>I',raw,32+offset*4,value)
            with self.assertRaises(ValueError):decode(raw)
        raw=capture();raw[84:136]=raw[32:84]
        with self.assertRaises(ValueError):decode(raw)

    def test_compare_checks_identity_and_values(self):
        with tempfile.TemporaryDirectory() as directory:
            paths=[Path(directory)/name for name in ('before','after')]
            meta=dict(bios_sha256='bios',probe_sha256='probe',fields=2400)
            for p in paths:
                p.mkdir();(p/'capture.json').write_text(json.dumps(meta))
                (p/'telemetry.bin').write_bytes(capture())
            report=compare(*paths)
            self.assertEqual(len(report['groups']),96)
            self.assertTrue(all(r['change_percent']==0 for r in report['cases']))
            for key in meta:
                changed=dict(meta,**{key:'different'})
                (paths[1]/'capture.json').write_text(json.dumps(changed))
                with self.assertRaises(ValueError):compare(*paths)
            (paths[1]/'capture.json').write_text(json.dumps(meta))
            raw=capture();struct.pack_into('>I',raw,6*4,0)
            (paths[1]/'telemetry.bin').write_bytes(raw)
            with self.assertRaises(ValueError):compare(*paths)
            raw=capture();struct.pack_into('>I',raw,32+11*4,1)
            (paths[1]/'telemetry.bin').write_bytes(raw)
            with self.assertRaises(ValueError):compare(*paths)

if __name__=='__main__':unittest.main()
