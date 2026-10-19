#!/usr/bin/env python3
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'harness/scripts'))
from division_latency import decode


def capture():
    words=[0x444c5031,1,210540,1,0,2,1,256]
    for bits in (32,64):
        for gap in (0,8,16,32,48,64):
            words.extend([bits,gap,*([50+2*gap]*5),2726963 if bits==32 else 613566757])
    return bytearray(struct.pack('>104I',*words))

class DivisionLatencyTests(unittest.TestCase):
    def test_curve_preserves_early_and_late_cost(self):
        report=decode(capture())
        self.assertEqual(report['curves'],[dict(bits=b,early_gap_ticks=32,late_gap_ticks=32) for b in (32,64)])
        self.assertEqual(len(report['cases']),12)

    def test_failed_truncated_and_unclocked_rejected(self):
        raw=capture()
        with self.assertRaises(ValueError):decode(raw[:-4])
        for index,value in [(0,0),(1,2),(2,0),(3,0),(4,1),(5,1),(7,128)]:
            raw=capture();struct.pack_into('>I',raw,index*4,value)
            with self.assertRaises(ValueError):decode(raw)

    def test_case_order_quotient_and_intervals_checked(self):
        for offset,value in [(0,64),(1,8),(2,65536),(7,0)]:
            raw=capture();struct.pack_into('>I',raw,32+4*offset,value)
            with self.assertRaises(ValueError):decode(raw)

if __name__=='__main__':unittest.main()
