"""Check the experiment's two transfer paths against integer dot products."""

from pathlib import Path
import random
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from scu_dsp import DspSim, Memory, assemble

SOURCE = ROOT / "examples/heterogeneous_3d_bench/lighting.dsp"
DIRECTION = (383, -619, 728)


class HeterogeneousLighting(unittest.TestCase):
    def test_ports_and_stream(self):
        rng = random.Random(1049)
        limits = [(-1024, -1024, -1024), (1024, 1024, 1024), (0, 0, 0)]
        for mode, counts in (("ports", (16,)), ("stream", (16, 32, 64, 128, 256, 512, 1024, 2048))):
            words, _ = assemble(SOURCE.read_text(), 256, {"STREAM": 1} if mode == "stream" else {})
            for count in counts:
                with self.subTest(mode=mode, count=count):
                    vertices = limits + [
                        tuple(rng.randint(-1024, 1024) for _ in range(3))
                        for _ in range(count - len(limits))
                    ]
                    expected = [
                        sum(x * y for x, y in zip(v, DIRECTION)) & 0xffffffff
                        for v in vertices
                    ]
                    packed = [x & 0xffffffff for v in vertices for x in v]
                    src, dst = 0x06001000, 0x06008000
                    memory = Memory()
                    dsp = DspSim(memory)
                    entry = assemble.first_address
                    dsp.load(words[entry:], entry)
                    dsp.data[0][:3] = [x & 0xffffffff for x in DIRECTION]
                    if mode == "stream":
                        for i, x in enumerate(packed):
                            memory.write(src + 4 * i, x)
                        memory.write(dst - 4, 0xdeadbeef)
                        memory.write(dst + 4 * count, 0xdeadbeef)
                    else:
                        dsp.data[1][:48] = packed
                    # Deliberately start with stale counters, then run twice
                    # without resetting DSP registers or reloading the light.
                    dsp.ct[:] = [61, 62, 63, 60]
                    for _ in range(2):
                        if mode == "stream":
                            dsp.data[3][:4] = [src // 4, dst // 4, count // 16, 1]
                        dsp.start(entry)
                        dsp.run(max_steps=25000)
                        if mode == "stream":
                            actual = [memory.read(dst + 4 * i) for i in range(count)]
                            self.assertEqual(memory.read(dst - 4), 0xdeadbeef)
                            self.assertEqual(memory.read(dst + 4 * count), 0xdeadbeef)
                        else:
                            actual = dsp.data[2][:count]
                        self.assertEqual(actual, expected)


if __name__ == "__main__":
    unittest.main()
