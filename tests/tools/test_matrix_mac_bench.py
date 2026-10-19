#!/usr/bin/env python3
"""Reject incomplete or internally inconsistent target math qualifications."""
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'harness/scripts'))
from matrix_mac_bench import decode


def capture():
    words=[0x4d4d4231,1,210540,1,0,33792,1,16]
    for index in range(16):
        chain=18 if index%2 else 1
        words.extend([index//4,(index%4)//2,chain,8 if index%2 else 64,
                      *([100-index//4]*5),0x12345678+index%4])
    return bytearray(struct.pack('>168I',*words))

class MatrixMacTests(unittest.TestCase):
    def test_full_qualification_and_paired_cost(self):
        result=decode(capture())
        self.assertEqual(result['checks'],33792)
        self.assertEqual(len(result['comparisons']),16)
        self.assertEqual(result['comparisons'][0]['change_percent'],0)
        self.assertAlmostEqual(result['comparisons'][12]['change_percent'],-3)

    def test_reject_truncated_incomplete_or_failed(self):
        with self.assertRaises(ValueError):decode(capture()[:-4])
        for index,value in [(0,0),(1,2),(2,0),(3,0),(4,1),(5,33791),(7,8)]:
            raw=capture();struct.pack_into('>I',raw,index*4,value)
            with self.subTest(index=index),self.assertRaises(ValueError):decode(raw)

    def test_reject_wrong_case_zero_clock_and_checksum(self):
        for offset,value in [(0,1),(1,1),(2,18),(3,8),(4,0)]:
            raw=capture();struct.pack_into('>I',raw,32+offset*4,value)
            with self.subTest(offset=offset),self.assertRaises(ValueError):decode(raw)
        raw=capture();struct.pack_into('>I',raw,32+4*40+9*4,0)
        with self.assertRaises(ValueError):decode(raw)

if __name__=='__main__':unittest.main()
