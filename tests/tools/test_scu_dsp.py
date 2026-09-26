"""Host tests for tools/scu_dsp.py: the assembler's encodings, the simulator's
instruction semantics and the batch transform program against the SH-2 fixed
point reference."""

import os
import random
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))

import scu_dsp as dsp  # noqa: E402


def words(source):
    return dsp.assemble(source)[0]


def wrap32(value):
    value &= 0xFFFFFFFF
    return value - (1 << 32) if value & 0x80000000 else value


class EncodingTests(unittest.TestCase):
    def test_special_instructions(self):
        self.assertEqual(words("END"), [0xF0000000])
        self.assertEqual(words("ENDI"), [0xF8000000])
        self.assertEqual(words("BTM"), [0xE0000000])
        self.assertEqual(words("LPS"), [0xE8000000])
        self.assertEqual(words("NOP"), [0])

    def test_load_immediate(self):
        self.assertEqual(words("MVI $123,MC0"), [0x80000123])
        self.assertEqual(words("MVI 5,LOP"), [0x80000000 | (10 << 26) | 5])
        self.assertEqual(words("MVI -1,RX"), [0x80000000 | (4 << 26) | 0x1FFFFFF])
        self.assertEqual(words("MVI 7,PC"), [0x80000000 | (12 << 26) | 7])
        # conditional: bit 25 set, the condition in bits 24-19, a 19-bit immediate
        self.assertEqual(words("MVI 3,RA0,NZ"), [0x80000000 | (6 << 26) | (1 << 25) | (0x01 << 19) | 3])
        self.assertEqual(words("MVI -2,MC1,Z"), [0x80000000 | (1 << 26) | (1 << 25) | (0x21 << 19) | 0x7FFFE])

    def test_jump(self):
        self.assertEqual(words("JMP 5"), [0xD0000005])
        self.assertEqual(words("JMP NZ,9"), [0xD0000000 | (1 << 25) | (0x01 << 19) | 9])
        self.assertEqual(words("L: NOP\n JMP L"), [0, 0xD0000000])
        self.assertEqual(words("JMP $+2\nNOP\nNOP"), [0xD0000002, 0, 0])

    def test_dma(self):
        # D0 -> M1, 6 longwords, address addition 1 (the default)
        self.assertEqual(words("DMA D0,M1,6"), [0xC0000000 | (1 << 15) | (1 << 8) | 6])
        self.assertEqual(words("DMA16 M2,D0,10"), [0xC0000000 | (5 << 15) | (1 << 12) | (2 << 8) | 10])
        self.assertEqual(words("DMAH D0,PRG,255"), [0xC0000000 | (1 << 15) | (1 << 14) | (4 << 8) | 255])
        # the count read from a data RAM word: bit 13, source in bits 2-0
        self.assertEqual(words("DMA0 D0,M0,MC2"), [0xC0000000 | (1 << 13) | 6])
        with self.assertRaises(dsp.AsmError):
            words("DMA3 D0,M0,4")            # 3 is not an address addition
        with self.assertRaises(dsp.AsmError):
            words("DMA M0,M1,4")             # one side must be D0

    def test_arithmetic_fields(self):
        # ALU 26-29, X op 23-25 / source 20-22, Y op 17-19 / source 14-16, D1 12-13
        w = words("AD2 MOV MUL,P MOV MC0,X MOV MC1,Y")[0]
        self.assertEqual(w >> 26, 6)                       # AD2
        self.assertEqual((w >> 23) & 7, 0b110)             # MUL,P and [s],X
        self.assertEqual((w >> 20) & 7, 4)                 # MC0
        self.assertEqual((w >> 17) & 7, 0b100)             # [s],Y
        self.assertEqual((w >> 14) & 7, 5)                 # MC1
        w = words("CLR A MOV MUL,P MOV MC0,X MOV MC3,Y")[0]
        self.assertEqual((w >> 17) & 7, 0b101)             # CLR A and [s],Y
        w = words("AD2 MOV ALU,A MOV MUL,P MOV 0,CT3")[0]
        self.assertEqual((w >> 17) & 7, 0b010)
        self.assertEqual((w >> 12) & 3, 0b01)
        self.assertEqual((w >> 8) & 0xF, 15)
        w = words("MOV ALL,MC2")[0]
        self.assertEqual(((w >> 12) & 3, (w >> 8) & 0xF, w & 0xF), (0b11, 2, 9))
        w = words("MOV M3,LOP")[0]
        self.assertEqual(((w >> 12) & 3, (w >> 8) & 0xF, w & 0xF), (0b11, 10, 3))
        w = words("MOV -3,MC0")[0]
        self.assertEqual(w & 0xFF, 0xFD)
        # one P writer, one source per bus
        with self.assertRaises(dsp.AsmError):
            words("MOV MUL,P MOV M0,P")
        with self.assertRaises(dsp.AsmError):
            words("MOV M0,X MOV M1,P")
        with self.assertRaises(dsp.AsmError):
            words("MOV 200,MC0")

    def test_pseudo_instructions_and_expressions(self):
        source = """
COUNT   EQU 4
SIZE    = COUNT*3+1
        ORG $10
START:  MVI SIZE,LOP        ; 13
        MOV COUNT-1,CT2
        MVI %101,RX
        MVI ~0&$FF,RX
        ENDS
        NOP
"""
        w, symbols = dsp.assemble(source)
        self.assertEqual(symbols["START"], 0x10)
        self.assertEqual(w[0x10], 0x80000000 | (10 << 26) | 13)
        self.assertEqual(w[0x11] & 0xFF, 3)
        self.assertEqual(w[0x12] & 0x1FFFFFF, 5)
        self.assertEqual(w[0x13] & 0x1FFFFFF, 0xFF)
        self.assertEqual(len(w), 0x14)             # ENDS ignores what follows

    def test_round_trip_through_the_disassembler(self):
        source = """
        MOV 0,CT0
        MVI 9,LOP
        AD2 MOV ALU,A MOV MUL,P MOV MC0,X MOV MC3,Y
        CLR A MOV MC1,Y
        MOV MC1,MC3
        AND MOV M0,P
        DMAH4 D0,M2,17
        JMP ZS,3
        MVI -7,WA0,S
        LPS
        ENDI
"""
        original = words(source)
        again = words("\n".join("        " + dsp.disassemble(w) for w in original))
        self.assertEqual(original, again)

    def test_errors_carry_a_line_number(self):
        with self.assertRaises(dsp.AsmError) as raised:
            words("NOP\nMOV 300,CT0\n")
        self.assertIn("line 2", str(raised.exception))
        with self.assertRaises(dsp.AsmError):
            words("JMP nowhere")
        with self.assertRaises(dsp.AsmError):
            dsp.assemble("\n".join(["NOP"] * 300), 256)


def run(source, data=None, setup=None, max_steps=100000):
    sim = dsp.DspSim()
    sim.load(words(source))
    for bank, values in (data or {}).items():
        for i, value in enumerate(values):
            sim.data[bank][i] = value & 0xFFFFFFFF
    if setup:
        setup(sim)
    sim.start(0)
    sim.run(max_steps)
    return sim


class SimulatorTests(unittest.TestCase):
    def test_immediate_loads_and_ct_increment(self):
        sim = run("MVI 11,MC0\nMVI 22,MC0\nMVI 33,MC1\nENDI\n")
        self.assertEqual(sim.data[0][:2], [11, 22])
        self.assertEqual(sim.data[1][0], 33)
        self.assertEqual(sim.ct[0], 2)
        self.assertTrue(sim.ended)

    def test_mac_and_flags(self):
        # P = 6*7, accumulated twice through the 48-bit adder, read as ALL
        source = """
        MVI 6,RX
        MVI 7,PL
        MOV 7,CT1
        MOV M1,Y
        MOV MUL,P
        MOV MUL,P
        AD2 MOV ALU,A
        AD2 MOV ALU,A
        MOV ALL,MC2
        ENDI
"""
        sim = run(source, data={1: [0] * 7 + [7]})
        self.assertEqual(sim.data[2][0], 84)         # RX*RY = 42, added twice
        self.assertFalse(sim.zero)
        sim = run("MVI -5,PL\nCLR A\nADD\nENDI")
        self.assertTrue(sim.sign)

    def test_loop_bottom_runs_the_instruction_after_it(self):
        # count three vectors: the counter word steps 3 times; the NOP after BTM is the delay slot
        source = """
        MOV 0,CT0
        MVI 2,LOP
        MOV LOOP,TOP
LOOP:   MVI 1,MC0
        BTM
        MVI 9,MC1
        ENDI
"""
        sim = run(source)
        self.assertEqual(sim.data[0][:4], [1, 1, 1, 0])
        self.assertEqual(sim.data[1][0], 9)
        # the slot instruction ran on every pass: LOP+1 passes for the body, one slot each
        sim = run(source.replace("MVI 9,MC1", "MOV 5,MC1"))
        self.assertEqual(sim.data[1][:4], [5, 5, 5, 0])

    def test_single_instruction_repeat(self):
        source = """
        MOV 0,CT0
        MOV 0,CT1
        MOV 4,LOP
        LPS
        MOV MC0,MC1
        ENDI
"""
        sim = run(source, data={0: [10, 20, 30, 40, 50, 60]})
        self.assertEqual(sim.data[1][:6], [10, 20, 30, 40, 50, 0])     # LOP=4 -> 5 repeats

    def test_conditional_jump_and_load(self):
        source = """
        MVI 0,PL
        CLR A
        ADD
        JMP Z,SKIP
        NOP
        MVI 1,MC0
SKIP:   MVI 7,MC1,NZ
        MVI 8,MC1,Z
        ENDI
"""
        sim = run(source)
        self.assertEqual(sim.data[0][0], 0)          # jumped over (the slot NOP ran)
        self.assertEqual(sim.data[1][0], 8)          # ALU result was zero: Z holds, NZ does not

    def test_dma_moves_words(self):
        memory = dsp.Memory()
        for i in range(4):
            memory.write(0x06010000 + 4 * i, 100 + i)
        sim = dsp.DspSim(memory)
        sim.load(words("""
        MOV 0,CT0
        MVI $01804000,RA0
        DMA2 D0,M0,4
        NOP
        MOV 0,CT0
        MVI $01804400,WA0
        DMA2 M0,D0,4
        NOP
        ENDI
"""))
        sim.start(0)
        sim.run()
        self.assertEqual(sim.data[0][:4], [100, 101, 102, 103])
        self.assertEqual([memory.read(0x06011000 + 4 * i) for i in range(4)], [100, 101, 102, 103])

    def test_program_ends_and_reports(self):
        sim = run("END")
        self.assertFalse(sim.ended)                   # END raises no interrupt
        sim = run("ENDI")
        self.assertTrue(sim.ended)


TRANSFORM = os.path.join(REPO, "src", "hal", "scu", "dsp_transform.dsp")


def reference(matrix, vertices):
    """The SH-2 fixed-point path: int64 products, summed, kept in 32 bits."""
    out = []
    for x, y, z in vertices:
        for row in range(3):
            total = matrix[row * 3] * x + matrix[row * 3 + 1] * y + matrix[row * 3 + 2] * z
            out.append(wrap32(total))
    return out


class TransformTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open(TRANSFORM, "r", encoding="utf-8") as stream:
            cls.program, _ = dsp.assemble(stream.read(), dsp.PROGRAM_WORDS)

    def run_transform(self, matrix, vertices):
        sim = dsp.DspSim()
        sim.load(self.program)
        for i, value in enumerate(matrix):
            sim.data[0][i] = value & 0xFFFFFFFF
        for i, (x, y, z) in enumerate(vertices):
            sim.data[1][3 * i:3 * i + 3] = [x & 0xFFFFFFFF, y & 0xFFFFFFFF, z & 0xFFFFFFFF]
        sim.data[3][63] = len(vertices) - 1
        sim.start(0)
        steps = sim.run()
        return [wrap32(v) for v in sim.data[2][:3 * len(vertices)]], steps

    def test_fits_the_program_ram(self):
        self.assertLessEqual(len(self.program), dsp.PROGRAM_WORDS)
        self.assertEqual(len(self.program), 6 + 15 + 1 + 1 + 1 + 6)   # setup, body head, rows, BTM, slot, ENDI

    def test_identity_and_scale(self):
        one = 0x10000
        out, _ = self.run_transform([one, 0, 0, 0, one, 0, 0, 0, one], [(1, 2, 3), (-4, 5, -6)])
        self.assertEqual(out, [one * 1, one * 2, one * 3, -4 * one, 5 * one, -6 * one])
        out, _ = self.run_transform([2 * one, 0, 0, 0, one // 2, 0, 0, 0, -one], [(10, 20, 30)])
        self.assertEqual(out, [20 * one, 10 * one, -30 * one])

    def test_matches_the_fixed_point_reference_on_random_data(self):
        rng = random.Random(5)
        for count in (1, 2, 7, 20):
            matrix = [rng.randint(-0x10000, 0x10000) for _ in range(9)]
            vertices = [(rng.randint(-200, 200), rng.randint(-200, 200), rng.randint(-200, 200))
                        for _ in range(count)]
            out, _ = self.run_transform(matrix, vertices)
            self.assertEqual(out, reference(matrix, vertices), "count %d" % count)

    def test_wraps_like_the_sh2_path(self):
        matrix = [0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 1, 2, 3, -0x80000000, 5, 6]
        vertices = [(1000000, -2000000, 3000000)]
        out, _ = self.run_transform(matrix, vertices)
        self.assertEqual(out, reference(matrix, vertices))

    def test_instruction_count_per_vector(self):
        matrix = [0x10000, 0, 0, 0, 0x10000, 0, 0, 0, 0x10000]
        _, one = self.run_transform(matrix, [(1, 2, 3)])
        _, ten = self.run_transform(matrix, [(1, 2, 3)] * 10)
        self.assertEqual((ten - one) // 9, 23)     # 22 in the body plus the delay-slot NOP


TRANSFORM_DMA = os.path.join(REPO, "src", "hal", "scu", "dsp_transform_dma.dsp")


class TransformDmaTests(unittest.TestCase):
    """The DMA variant: the DSP moves its own data (address addition 2, four bytes per word)."""

    def test_moves_vectors_and_matches_the_reference(self):
        with open(TRANSFORM_DMA, "r", encoding="utf-8") as stream:
            program, _ = dsp.assemble(stream.read(), dsp.PROGRAM_WORDS)
        rng = random.Random(11)
        for count in (1, 5, 21):
            matrix = [rng.randint(-0x18000, 0x18000) for _ in range(9)]
            vectors = [(rng.randint(-300, 300), rng.randint(-300, 300), rng.randint(-300, 300)) for _ in range(count)]
            memory = dsp.Memory()
            src, dst = 0x06040000, 0x06050000
            for i, (x, y, z) in enumerate(vectors):
                for j, value in enumerate((x, y, z)):
                    memory.write(src + 4 * (3 * i + j), value & 0xFFFFFFFF)
            sim = dsp.DspSim(memory)
            sim.load(program)
            for i, value in enumerate(matrix):
                sim.data[0][i] = value & 0xFFFFFFFF
            sim.data[3][0] = src >> 2
            sim.data[3][1] = dst >> 2
            sim.data[3][2] = sim.data[3][3] = 3 * count
            sim.data[3][63] = count - 1
            sim.start(32)
            sim.run()
            got = [wrap32(memory.read(dst + 4 * i)) for i in range(3 * count)]
            self.assertEqual(got, reference(matrix, vectors), "count %d" % count)
            self.assertEqual(memory.read(dst + 4 * 3 * count), 0)      # nothing written past the end

    def test_fits_beside_the_plain_program(self):
        with open(TRANSFORM_DMA, "r", encoding="utf-8") as stream:
            dsp.assemble(stream.read(), dsp.PROGRAM_WORDS)
            first = dsp.assemble.first_address
        self.assertEqual(first, 32)


class ConditionalAssemblyTests(unittest.TestCase):
    def test_if_ifdef_else_endif(self):
        source = "IF MODE\n MVI 1,MC0\nELSE\n MVI 2,MC0\nENDIF\nIFDEF X\n MVI 3,MC0\nENDIF\n"
        self.assertEqual(dsp.assemble(source, defines={"MODE": 1})[0], [0x80000001])
        self.assertEqual(dsp.assemble(source, defines={"MODE": 0, "X": 1})[0], [0x80000002, 0x80000003])
        with self.assertRaises(dsp.AsmError):
            dsp.assemble("IF 1\nNOP\n")               # no ENDIF
        with self.assertRaises(dsp.AsmError):
            dsp.assemble("ENDIF\n")


class GeneratedHeadersTests(unittest.TestCase):
    """The checked-in headers must be what the tool makes from their sources."""

    def check(self, source, header, name, defines=None):
        with open(os.path.join(REPO, source), "r", encoding="utf-8") as stream:
            words, _ = dsp.assemble(stream.read(), dsp.PROGRAM_WORDS, defines)
        base = dsp.assemble.first_address
        expected = dsp._c_array(name, words, base)
        with open(os.path.join(REPO, header), "r", encoding="utf-8", newline="") as stream:
            actual = stream.read().replace("\r\n", "\n")
        self.assertEqual(actual, expected, "%s is stale: run tools/scu_dsp.py asm %s" % (header, source))

    def test_transform_headers_are_current(self):
        self.check("src/hal/scu/dsp_transform.dsp", "src/hal/scu/dsp_transform_words.h", "kDspTransformProgram")
        self.check("src/hal/scu/dsp_transform_dma.dsp", "src/hal/scu/dsp_transform_dma_words.h",
                   "kDspTransformDmaProgram")

    def test_dma_probe_headers_are_current(self):
        for rs in (1, 2):
            for ws in (1, 2):
                self.check("examples/scu_dsp_demo/dma_probe.dsp",
                           "examples/scu_dsp_demo/dma_probe_r%dw%d.h" % (rs, ws),
                           "kDmaProbeR%dW%d" % (rs, ws), {"RS%d" % rs: 1, "WS%d" % ws: 1})


if __name__ == "__main__":
    unittest.main()
