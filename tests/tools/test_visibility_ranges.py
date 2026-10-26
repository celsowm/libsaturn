import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]/'harness/scripts'))
from visibility_ranges import classify


class VisibilityRangeTests(unittest.TestCase):
    def row(self, edges, cpu='master', opcode=123):
        return dict(cpu=cpu, pc='100', opcode0=str(opcode),
                    **{f'r{i}':str(v & 0xffffffff) for i,v in enumerate(edges)})

    def test_signed_boundaries_and_cpus(self):
        rows = [self.row([v]*6) for v in (-32769,-32768,-16385,-16384,16383,16384,32767,32768)]
        rows += [self.row([0]*6, cpu='slave')]
        result = classify(rows, 100, list(range(6)), 123)
        self.assertEqual(result['master'], dict(checkpoints=8,old_range=2,newly_eligible=4,wide_range=2))
        self.assertEqual(result['slave']['old_range'], 1)

    def test_rejects_missing_or_incompatible_checkpoints(self):
        for rows, registers, opcode in (([],list(range(6)),123),
                ([self.row([0]*6)],list(range(6)),124),
                ([self.row([0]*6)],[0]*6,123)):
            with self.assertRaises(ValueError): classify(rows,100,registers,opcode)


if __name__ == '__main__': unittest.main()
