#!/usr/bin/env python3
"""SCSP DSP microprogram builder and simulator.

The SCSP has a small DSP that runs a 128-step microprogram once per output
sample (44.1 kHz): it reads the mix of the slots routed to it (MIXS), keeps
delay lines in a ring buffer in Sound RAM and returns the result through EFREG.
This module encodes microprogram steps (one 64-bit word each, fields as in the
SCSP User's Manual and the Ymir core), simulates a program on the host with the
Ymir semantics, and builds two presets, an echo and a four-comb reverb, whose
data is written to src/hal/scsp/scsp_dsp_presets.h for the SH-2 loader.

    python tools/scsp_dsp.py header [--check]

Only linear (NOFL) memory access is supported: 16-bit samples go through the
ring buffer as they are, so no floating conversion is simulated.
"""

import argparse
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
HEADER = os.path.join(REPO, "src", "hal", "scsp", "scsp_dsp_presets.h")

STEPS = 128
COEF_ONE = 0x0FFF          # 13-bit signed, 12 fractional bits: the largest gain below 1.0

# (name, lowest bit, width) of every field of a step.
FIELDS = {
    "NXADR": (0, 1), "ADREB": (1, 1), "MASA": (2, 5), "NOFL": (8, 1), "CRA": (9, 6),
    "BSEL": (16, 1), "ZERO": (17, 1), "NEGB": (18, 1), "YRL": (19, 1), "SHFT": (20, 2),
    "FRCL": (22, 1), "ADRL": (23, 1), "EWA": (24, 4), "EWT": (28, 1), "MRD": (29, 1),
    "MWT": (30, 1), "TABLE": (31, 1), "IWA": (32, 5), "IWT": (37, 1), "IRA": (38, 6),
    "YSEL": (45, 2), "XSEL": (47, 1), "TWA": (48, 7), "TWT": (55, 1), "TRA": (56, 7),
}

IRA_MIXS = 0x20          # IRA 0x20-0x2F read MIXS 0-15
COEF_ZERO_INDEX = 63     # a coefficient kept at 0, for steps that must add nothing


def step(**fields):
    """One microprogram word from named fields."""
    word = 0
    for name, value in fields.items():
        low, width = FIELDS[name]
        if not 0 <= value < (1 << width):
            raise ValueError("%s=%d does not fit %d bits" % (name, value, width))
        word |= value << low
    return word


def field(word, name):
    low, width = FIELDS[name]
    return (word >> low) & ((1 << width) - 1)


def coefficient(gain):
    """A 13-bit coefficient for a gain in [-1, 1): 12 fractional bits."""
    value = int(round(gain * 4096.0))
    value = max(-4096, min(0x0FFF, value))
    return value & 0x1FFF


def sign_extend(value, bits):
    value &= (1 << bits) - 1
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


class Dsp:
    """The DSP as the Ymir core runs it, for programs that use linear memory access."""

    def __init__(self, program, coef, madrs, ring_length=0, ring_base_words=0):
        # The core stops after the last used step plus one NOP; the steps after it do nothing.
        self.program = list(program) + [0]
        self.coef = list(coef) + [0] * (64 - len(coef))
        self.madrs = list(madrs) + [0] * (32 - len(madrs))
        self.rbl = (0x2000 << ring_length) - 1
        self.rbp = ring_base_words          # RBP << 12, in 16-bit words
        self.ram = {}                        # word address -> signed 16-bit value
        self.temp = [0] * 128
        self.mems = [0] * 32
        self.effect = [0] * 16
        self.mdec = 0
        self.sft = 0
        self.frc = 0
        self.y = 0
        self.adrs = 0
        self.inputs = 0
        self.read_pending = False
        self.read_value = 0
        self.write_pending = False
        self.write_value = 0
        self.rw_addr = 0

    def run_sample(self, mixs):
        """Runs the 128 steps once. `mixs` maps a MIXS index to a signed 20-bit value."""
        for word in self.program:
            self._step(word, mixs)
        self.mdec = (self.mdec - 1) & 0xFFFF

    def _read_ram(self):
        return self.ram.get(self.rw_addr & 0x7FFFF, 0)

    def _step(self, w, mixs):
        f = lambda name: field(w, name)
        if f("NOFL") == 0 and (f("MRD") or f("MWT")):
            raise NotImplementedError("floating conversion is not simulated (use NOFL)")
        ira = f("IRA")
        if ira <= 0x1F:
            self.inputs = self.mems[ira]
        elif ira <= 0x2F:
            self.inputs = sign_extend(mixs.get(ira & 0xF, 0), 20) << 4
        elif ira <= 0x31:
            self.inputs = 0
        temp_read = (f("TRA") + self.mdec) & 0x7F
        temp_write = (f("TWA") + self.mdec) & 0x7F
        temp = self.temp[temp_read]
        xval = self.inputs if f("XSEL") else temp
        ysel = f("YSEL")
        if ysel == 0:
            yval = self.frc
        elif ysel == 1:
            yval = self.coef[f("CRA")]
        elif ysel == 2:
            yval = (self.y >> 11) & 0x1FFF
        else:
            yval = (self.y >> 4) & 0x1FFF
        if f("YRL"):
            self.y = self.inputs & 0xFFFFFF
        shft = f("SHFT")
        shifter = (sign_extend(self.sft, 26) << ((shft & 1) ^ (shft >> 1))) & 0xFFFFFFFF
        shifter = sign_extend(shifter, 32)
        if (shft >> 1) == 0:
            shifter = max(-0x800000, min(0x7FFFFF, shifter))
        else:
            shifter = sign_extend(shifter, 24)
        if f("FRCL"):
            self.frc = (shifter & 0xFFF) if shft == 3 else ((shifter >> 11) & 0x1FFF)
        if f("ZERO"):
            sga = 0
        else:
            sga = self.sft if f("BSEL") else temp
            if f("NEGB"):
                sga = -sga
        product = (sign_extend(yval, 13) * xval) >> 12
        self.sft = (product + sga) & 0x3FFFFFF
        if f("EWT"):
            self.effect[f("EWA")] = sign_extend(shifter >> 8, 16)
        if f("TWT"):
            self.temp[temp_write] = shifter
        if f("IWT"):
            self.mems[f("IWA")] = sign_extend(self.read_value, 24)
        if self.read_pending:
            self.read_value = (self._read_ram() & 0xFFFF) << 8
            self.read_value = sign_extend(self.read_value, 24) & 0xFFFFFF
            self.read_pending = False
        elif self.write_pending:
            self.ram[self.rw_addr & 0x7FFFF] = self.write_value
            self.write_pending = False
        addr = (self.madrs[f("MASA")] + f("NXADR")) & 0xFFFF
        if f("ADREB"):
            addr = (addr + sign_extend(self.adrs, 12)) & 0xFFFF
        if not f("TABLE"):
            addr = (addr + self.mdec) & self.rbl
        self.rw_addr = (addr + self.rbp) & 0x7FFFF
        if f("MRD"):
            self.read_pending = True
        if f("MWT"):
            self.write_pending = True
            self.write_value = sign_extend(shifter >> 8, 16)
        if f("ADRL"):
            self.adrs = ((shifter >> 12) & 0xFFF) if shft == 3 else ((self.inputs >> 16) & 0xFFF)


# ---------------------------------------------------------------------------
# Presets
#
# The programs are fixed. What changes at run time is only the data: the
# coefficients (gains) and the memory address registers (delays). The layouts
# below are shared with src/hal/scsp/dsp_logic.hpp through the generated header.
# ---------------------------------------------------------------------------

COEF_INPUT = 0
COEF_FEEDBACK = 1
COEF_WET = 2
MADRS_ECHO_READ = 0
MADRS_ECHO_WRITE = 1
REVERB_COMBS = 4
REVERB_SPACING = 4096          # words between the write positions of two combs
REVERB_RING_LENGTH = 1         # 16K words
REVERB_DELAYS = (1687, 2053, 2411, 2851)
ECHO_DEFAULT_DELAY = 2205      # 50 ms
MEMS_ECHO = 0
TEMP_ACCUMULATOR = 8


def echo_program():
    """One tap. temp0 = in*g_in; d = line[t-delay]; line[t] = d*fb + in*g_in; out = d*wet."""
    zero = COEF_ZERO_INDEX
    return [
        # 0: scale the input, ask for the delayed sample
        step(IRA=IRA_MIXS, XSEL=1, YSEL=1, CRA=COEF_INPUT, ZERO=1, MRD=1, NOFL=1, MASA=MADRS_ECHO_READ),
        # 1: the scaled input waits in temp 0; the delayed sample is read
        step(TWT=1, TWA=0, ZERO=1, YSEL=1, CRA=zero),
        # 2: the delayed sample arrives in MEMS 0
        step(IWT=1, IWA=MEMS_ECHO, ZERO=1, YSEL=1, CRA=zero),
        # 3: d * feedback + scaled input
        step(IRA=MEMS_ECHO, XSEL=1, YSEL=1, CRA=COEF_FEEDBACK, TRA=0),
        # 4: write that back; start d * wet
        step(IRA=MEMS_ECHO, XSEL=1, YSEL=1, CRA=COEF_WET, ZERO=1, MWT=1, NOFL=1, MASA=MADRS_ECHO_WRITE),
        # 5: the wet output (the write completes on this step)
        step(EWT=1, EWA=0, ZERO=1, YSEL=1, CRA=zero),
    ]


def reverb_program():
    """Four parallel feedback combs (each like the echo) whose wet outputs are summed on EFREG 0.
    Comb i reads MADRS[i] and writes MADRS[4+i], keeps its delayed sample in MEMS i and its scaled
    input in temp i; temp 8 collects the sum."""
    zero = COEF_ZERO_INDEX
    steps = []
    for i in range(REVERB_COMBS):
        last = i == REVERB_COMBS - 1
        steps += [
            step(IRA=IRA_MIXS, XSEL=1, YSEL=1, CRA=COEF_INPUT, ZERO=1, MRD=1, NOFL=1, MASA=i),
            step(TWT=1, TWA=i, ZERO=1, YSEL=1, CRA=zero),
            step(IWT=1, IWA=i, ZERO=1, YSEL=1, CRA=zero),
            step(IRA=i, XSEL=1, YSEL=1, CRA=COEF_FEEDBACK, TRA=i),
        ]
        if i == 0:
            steps.append(step(IRA=i, XSEL=1, YSEL=1, CRA=COEF_WET, ZERO=1, MWT=1, NOFL=1, MASA=REVERB_COMBS + i))
        else:
            steps.append(step(IRA=i, XSEL=1, YSEL=1, CRA=COEF_WET, TRA=TEMP_ACCUMULATOR, MWT=1, NOFL=1,
                              MASA=REVERB_COMBS + i))
        if last:
            steps.append(step(EWT=1, EWA=0, ZERO=1, YSEL=1, CRA=zero))
        else:
            steps.append(step(TWT=1, TWA=TEMP_ACCUMULATOR, ZERO=1, YSEL=1, CRA=zero))
    return steps


def echo_data(delay=ECHO_DEFAULT_DELAY, feedback=0.55, wet=0.6, input_gain=0.5):
    """COEF and MADRS arrays for the echo; `delay` in samples (1..65535)."""
    if not 1 <= delay <= 65535:
        raise ValueError("delay must be 1..65535 samples")
    coef = [0] * 64
    coef[COEF_INPUT], coef[COEF_FEEDBACK], coef[COEF_WET] = (coefficient(input_gain), coefficient(feedback),
                                                             coefficient(wet))
    madrs = [0] * 32
    madrs[MADRS_ECHO_READ] = delay
    madrs[MADRS_ECHO_WRITE] = 0
    return coef, madrs


def echo_ring_length(delay):
    """Smallest ring (length code 0-3: 8K, 16K, 32K, 64K words) longer than the delay."""
    for code in range(4):
        if delay < (0x2000 << code):
            return code
    raise ValueError("delay does not fit the largest ring")


def reverb_data(delays=REVERB_DELAYS, feedback=0.78, wet=0.25, input_gain=0.35):
    if len(delays) != REVERB_COMBS or any(not 1 <= d < REVERB_SPACING for d in delays):
        raise ValueError("four delays of 1..4095 samples")
    coef = [0] * 64
    coef[COEF_INPUT], coef[COEF_FEEDBACK], coef[COEF_WET] = (coefficient(input_gain), coefficient(feedback),
                                                             coefficient(wet))
    madrs = [0] * 32
    for i, delay in enumerate(delays):
        madrs[i] = i * REVERB_SPACING + delay
        madrs[REVERB_COMBS + i] = i * REVERB_SPACING
    return coef, madrs


def render_header():
    lines = ["/* Generated by tools/scsp_dsp.py; do not edit. */", "#include <stdint.h>", ""]
    for name, program in (("Echo", echo_program()), ("Reverb", reverb_program())):
        lines.append("enum { kScspDsp%sSteps = %d };" % (name, len(program)))
        lines.append("static const uint16_t kScspDsp%sProgram[%d][4] = {" % (name, len(program)))
        for w in program:
            lines.append("    {0x%04X, 0x%04X, 0x%04X, 0x%04X}," % (w >> 48 & 0xFFFF, w >> 32 & 0xFFFF,
                                                                  w >> 16 & 0xFFFF, w & 0xFFFF))
        lines.append("};")
        lines.append("")
    lines.append("enum {")
    for label, value in (("CoefInput", COEF_INPUT), ("CoefFeedback", COEF_FEEDBACK), ("CoefWet", COEF_WET),
                         ("CoefZero", COEF_ZERO_INDEX), ("MadrsEchoRead", MADRS_ECHO_READ),
                         ("MadrsEchoWrite", MADRS_ECHO_WRITE), ("ReverbCombs", REVERB_COMBS),
                         ("ReverbSpacing", REVERB_SPACING), ("ReverbRingLength", REVERB_RING_LENGTH),
                         ("EchoDefaultDelay", ECHO_DEFAULT_DELAY)):
        lines.append("    kScspDsp%s = %d," % (label, value))
    lines.append("};")
    lines.append("static const uint16_t kScspDspReverbDelays[%d] = {%s};" %
                 (REVERB_COMBS, ", ".join(str(d) for d in REVERB_DELAYS)))
    lines.append("")
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    header = sub.add_parser("header")
    header.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    text = render_header()
    if args.check:
        with open(HEADER, "r", encoding="utf-8", newline="") as stream:
            current = stream.read().replace("\r\n", "\n")
        if current != text:
            print("%s is stale" % HEADER, file=sys.stderr)
            return 1
        return 0
    with open(HEADER, "w", encoding="utf-8", newline="\n") as stream:
        stream.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
