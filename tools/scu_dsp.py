#!/usr/bin/env python3
"""SCU DSP assembler, disassembler and simulator.

The assembler takes the mnemonics of Sega's SCU DSP assembler (SCU User's Manual,
"SCU DSP Assembler Instruction Manual"): up to four parallel operations in one
arithmetic instruction (ALU, X-bus, Y-bus, D1-bus), MVI, DMA/DMAH, JMP, BTM/LPS,
END/ENDI, the ORG, EQU and ENDS pseudo-instructions and expressions on numbers
and labels. The simulator follows the instruction semantics of the Ymir
emulator's SCU DSP core, including the one-instruction prefetch that makes the
instruction after a jump or loop bottom execute. It exists so a DSP program can
be tested on the host and compared with the SH-2 fixed-point path before it
runs on hardware.

    python tools/scu_dsp.py asm program.dsp --c program_dsp.h --name kProgram
    python tools/scu_dsp.py dis program_dsp.h

The C output is a `static const uint32_t NAME[NAMECount]` array of the words from
the first instruction on, with NAMEAddress the program RAM address it belongs at.
"""

import argparse
import re
import sys

PROGRAM_WORDS = 256
DATA_WORDS = 64

# ---------------------------------------------------------------------------
# Encoding tables
# ---------------------------------------------------------------------------

ALU_OPS = {"NOP": 0x0, "AND": 0x1, "OR": 0x2, "XOR": 0x3, "ADD": 0x4, "SUB": 0x5,
           "AD2": 0x6, "SR": 0x8, "RR": 0x9, "SL": 0xA, "RL": 0xB, "RL8": 0xF}

# [s] operands of the X, Y and D1 buses: M0-M3 read a data RAM word, MC0-MC3
# read it and step the CT register; ALL/ALH read the ALU (D1 bus only).
SOURCES = {"M0": 0, "M1": 1, "M2": 2, "M3": 3, "MC0": 4, "MC1": 5, "MC2": 6, "MC3": 7,
           "ALL": 9, "ALH": 10}
D1_DESTS = {"MC0": 0, "MC1": 1, "MC2": 2, "MC3": 3, "RX": 4, "PL": 5, "RA0": 6, "WA0": 7,
            "LOP": 10, "TOP": 11, "CT0": 12, "CT1": 13, "CT2": 14, "CT3": 15}
# MOV to M0-M3 is accepted too: it writes without stepping CT.
IMM_DESTS = {"MC0": 0, "MC1": 1, "MC2": 2, "MC3": 3, "RX": 4, "PL": 5, "RA0": 6, "WA0": 7,
             "LOP": 10, "PC": 12}
CONDITIONS = {"NZ": 0x01, "NS": 0x02, "NZS": 0x03, "NC": 0x04, "NT0": 0x08,
              "Z": 0x21, "S": 0x22, "ZS": 0x23, "C": 0x24, "T0": 0x28}
STRIDES = {0: 0, 1: 1, 2: 2, 4: 3, 8: 4, 16: 5, 32: 6, 64: 7}
MNEMONICS = set(ALU_OPS) | {"CLR", "MOV", "MVI", "DMA", "DMAH", "JMP", "BTM", "LPS", "END", "ENDI"}


class AsmError(Exception):
    def __init__(self, message, line=None):
        super().__init__(("line %d: " % line if line else "") + message)


# ---------------------------------------------------------------------------
# Expressions: numbers ($hex, %binary, decimal), labels, $ (here), C operators
# ---------------------------------------------------------------------------

_TOKEN = re.compile(r"\s*(?:(0x[0-9A-Fa-f]+|\$[0-9A-Fa-f]+|%[01]+|\d+)|([A-Za-z_][A-Za-z0-9_]*)|(<<|>>|[-+*/%~&|^()$]))")


def evaluate(text, symbols, here):
    """Integer value of an expression. `$` alone is the address of the instruction."""
    out = []
    pos = 0
    text = text.strip()
    previous = "op"
    while pos < len(text):
        match = _TOKEN.match(text, pos)
        if not match:
            raise AsmError("cannot parse %r" % text[pos:])
        number, name, op = match.groups()
        pos = match.end()
        if number is not None:
            if number.startswith("$"):
                out.append(str(int(number[1:], 16)))
            elif number.startswith("%"):
                # a leading % is a binary literal; after a value it is the remainder operator
                if previous == "value":
                    out.append("%")
                    out.append(str(int(number[1:], 10)))
                else:
                    out.append(str(int(number[1:], 2)))
            elif number.lower().startswith("0x"):
                out.append(str(int(number, 16)))
            else:
                out.append(str(int(number, 10)))
            previous = "value"
        elif name is not None:
            key = name.upper()
            if key not in symbols:
                raise AsmError("undefined symbol %s" % name)
            out.append(str(symbols[key]))
            previous = "value"
        else:
            if op == "$":
                out.append(str(here))
                previous = "value"
            else:
                out.append({"~": "~", "^": "^", "/": "//"}.get(op, op))
                previous = "op" if op != ")" else "value"
    try:
        return int(eval("".join(out), {"__builtins__": {}}, {}))
    except Exception as error:  # noqa: BLE001 - report as an assembler error
        raise AsmError("bad expression %r (%s)" % (text, error))


# ---------------------------------------------------------------------------
# Assembler
# ---------------------------------------------------------------------------

def _strip(line):
    if ";" in line:
        line = line[:line.index(";")]
    return line.rstrip()


def _split_operations(body):
    """Groups the words of a line into operations, one per mnemonic."""
    operations = []
    for word in body.split():
        upper = word.upper()
        base = re.sub(r"^(DMAH?)\d+$", r"\1", upper)
        if base in MNEMONICS:
            operations.append([upper, ""])
        elif operations:
            operations[-1][1] = (operations[-1][1] + " " + word).strip()
        else:
            raise AsmError("unexpected %r" % word)
    return operations


def assemble(text, max_words=2048, defines=None):
    """Returns (words, symbols). Two passes so labels can be used before they are defined.
    `defines` seeds symbols (name -> value), like -D on the command line."""
    lines = [_strip(line) for line in text.splitlines()]
    seed = {k.upper(): v for k, v in (defines or {}).items()}
    symbols = dict(seed)
    for final in (False, True):
        words = {}
        address = 0
        ended = False
        conditions = []          # (this branch is live, some branch was taken) for IF nesting
        for number, raw in enumerate(lines, 1):
            if ended or not raw.strip():
                continue
            try:
                directive = raw.split(None, 1)
                keyword_if = directive[0].upper() if directive else ""
                if keyword_if in ("IF", "IFDEF", "ELSE", "ENDIF"):
                    argument_if = directive[1].strip() if len(directive) > 1 else ""
                    live = all(c[0] for c in conditions)
                    if keyword_if in ("IF", "IFDEF"):
                        if len(conditions) >= 16:
                            raise AsmError("IF nested deeper than 16")
                        if not live:
                            conditions.append((False, True))
                        elif keyword_if == "IF":
                            value = evaluate(argument_if, symbols if final else _lenient(symbols), address)
                            conditions.append((value != 0, value != 0))
                        else:
                            defined = argument_if.upper() in symbols
                            conditions.append((defined, defined))
                    elif keyword_if == "ELSE":
                        if not conditions:
                            raise AsmError("ELSE without IF")
                        was_live, taken = conditions.pop()
                        conditions.append((not taken, True))
                    else:
                        if not conditions:
                            raise AsmError("ENDIF without IF")
                        conditions.pop()
                    continue
                if not all(c[0] for c in conditions):
                    continue
                body = raw
                label = None
                match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$", body)
                if match:
                    label, body = match.group(1), match.group(2)
                elif raw[0] not in " \t":
                    # a name in the first column: a label, unless it is an EQU line
                    parts = raw.split(None, 1)
                    head_word = re.sub(r"^(DMAH?)\d+$", r"\1", parts[0].upper())
                    if head_word not in MNEMONICS | {"ORG", "ENDS", "EQU", "IF", "IFDEF", "ELSE", "ENDIF"} and \
                            not (len(parts) > 1 and re.match(r"^(EQU\b|=)", parts[1].strip(), re.I)):
                        label, body = parts[0], parts[1] if len(parts) > 1 else ""
                if label is not None:
                    symbols[label.upper()] = address
                body = body.strip()
                if not body:
                    continue
                equ = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*(?:EQU|=)\s*(.+)$", body, re.I)
                if equ:
                    symbols[equ.group(1).upper()] = evaluate(equ.group(2), symbols if final else _lenient(symbols), address)
                    continue
                head = body.split(None, 1)
                keyword = head[0].upper()
                argument = head[1] if len(head) > 1 else ""
                if keyword == "ORG":
                    address = evaluate(argument, symbols if final else _lenient(symbols), address)
                    continue
                if keyword == "ENDS":
                    ended = True
                    continue
                word = _assemble_line(body, symbols if final else _lenient(symbols), address)
                if address in words:
                    raise AsmError("address %d used twice" % address)
                if address >= max_words:
                    raise AsmError("program longer than %d instructions" % max_words)
                words[address] = word
                address += 1
            except AsmError as error:
                raise AsmError(str(error), number)
        if conditions:
            raise AsmError("IF without ENDIF")
        size = max(words) + 1 if words else 0
    assemble.first_address = min(words) if words else 0     # where the first instruction was placed
    return [words.get(i, 0) for i in range(size)], symbols


class _lenient(dict):
    """First pass: unknown labels evaluate to 0."""

    def __init__(self, base):
        super().__init__(base)

    def __contains__(self, key):
        return True

    def __getitem__(self, key):
        return dict.get(self, key, 0)


def _assemble_line(body, symbols, here):
    operations = _split_operations(body)
    if not operations:
        raise AsmError("empty instruction")
    names = [op[0] for op in operations]
    first = re.sub(r"^(DMAH?)\d+$", r"\1", names[0])
    if first == "MVI":
        return _mvi(operations[0][1], symbols, here)
    if first in ("DMA", "DMAH"):
        return _dma(operations[0][0], operations[0][1], symbols, here)
    if first == "JMP":
        return _jmp(operations[0][1], symbols, here)
    if first == "BTM":
        return 0xE0000000
    if first == "LPS":
        return 0xE8000000
    if first == "END":
        return 0xF0000000
    if first == "ENDI":
        return 0xF8000000
    return _arithmetic(operations, symbols, here)


def _bus_source(name):
    key = name.strip().upper()
    if key not in SOURCES or SOURCES[key] > 7:
        raise AsmError("%s cannot feed the X or Y bus" % name)
    return SOURCES[key]


def _arithmetic(operations, symbols, here):
    alu = None
    x_mul = load_p = load_x = False
    x_source = None
    y_clear = y_alu = load_a = load_y = False
    y_source = None
    d1 = None

    def bus_source(current, name, bus):
        value = _bus_source(name)
        if current is not None and current != value:
            raise AsmError("the %s takes one source" % bus)
        return value

    for name, operand in operations:
        if name in ALU_OPS:
            if name != "NOP" or alu is None:
                if alu is not None and alu != 0:
                    raise AsmError("two ALU operations")
                alu = ALU_OPS[name]
            continue
        if name == "CLR":
            if operand.strip().upper() != "A":
                raise AsmError("CLR takes A")
            y_clear = True
            continue
        if name != "MOV":
            raise AsmError("%s cannot share an instruction" % name)
        parts = [p.strip() for p in operand.split(",")]
        if len(parts) != 2:
            raise AsmError("MOV takes two operands")
        src, dst = parts
        su, du = src.upper(), dst.upper()
        if su == "MUL" and du == "P":
            x_mul = True
        elif du == "P" and su in SOURCES:
            x_source = bus_source(x_source, src, "X-bus")
            load_p = True
        elif du == "X" and su in SOURCES:
            x_source = bus_source(x_source, src, "X-bus")
            load_x = True
        elif su == "ALU" and du == "A":
            y_alu = True
        elif du == "A" and su in SOURCES:
            y_source = bus_source(y_source, src, "Y-bus")
            load_a = True
        elif du == "Y" and su in SOURCES:
            y_source = bus_source(y_source, src, "Y-bus")
            load_y = True
        elif du in D1_DESTS:
            if d1 is not None:
                raise AsmError("two D1-bus transfers")
            if su in SOURCES:
                d1 = ("reg", D1_DESTS[du], SOURCES[su])
            else:
                d1 = ("imm", D1_DESTS[du], evaluate(src, symbols, here))
        else:
            raise AsmError("cannot assemble MOV %s,%s" % (src, dst))
    if x_mul and load_p:
        raise AsmError("MOV MUL,P and MOV [s],P both write P")
    x_op = (4 if load_x else 0) | (2 if x_mul else 3 if load_p else 0)
    if sum([y_clear, y_alu, load_a]) > 1:
        raise AsmError("only one of CLR A, MOV ALU,A and MOV [s],A per instruction")
    y_op = (4 if load_y else 0) | (1 if y_clear else 2 if y_alu else 3 if load_a else 0)
    word = ((alu or 0) << 26) | (x_op << 23) | ((x_source or 0) << 20) | (y_op << 17) | ((y_source or 0) << 14)
    if d1 is not None:
        kind, dest, value = d1
        if kind == "imm":
            if not -128 <= value <= 127:
                raise AsmError("immediate %d does not fit 8 bits" % value)
            word |= (0b01 << 12) | (dest << 8) | (value & 0xFF)
        else:
            word |= (0b11 << 12) | (dest << 8) | (value & 0xF)
    return word


def _mvi(operand, symbols, here):
    parts = [p.strip() for p in operand.split(",")]
    if len(parts) not in (2, 3):
        raise AsmError("MVI takes Imm,[d]{,cond}")
    dest = parts[1].upper()
    if dest not in IMM_DESTS:
        raise AsmError("MVI cannot write %s" % parts[1])
    value = evaluate(parts[0], symbols, here)
    word = 0x80000000 | (IMM_DESTS[dest] << 26)
    if len(parts) == 3:
        cond = parts[2].upper()
        if cond not in CONDITIONS:
            raise AsmError("unknown condition %s" % parts[2])
        if not -(1 << 18) <= value < (1 << 18):
            raise AsmError("conditional immediate %d does not fit 19 bits" % value)
        return word | (1 << 25) | (CONDITIONS[cond] << 19) | (value & 0x7FFFF)
    # the field is 25 bits, sign-extended by the DSP; either the signed or the unsigned form is accepted
    # (an RA0/WA0 word address of Work RAM has bit 24 set)
    if not -(1 << 24) <= value < (1 << 25):
        raise AsmError("immediate %d does not fit 25 bits" % value)
    return word | (value & 0x1FFFFFF)


def _jmp(operand, symbols, here):
    parts = [p.strip() for p in operand.split(",")]
    word = 0xD0000000
    if len(parts) == 2:
        cond = parts[0].upper()
        if cond not in CONDITIONS:
            raise AsmError("unknown condition %s" % parts[0])
        word |= (1 << 25) | (CONDITIONS[cond] << 19)
        target = evaluate(parts[1], symbols, here)
    elif len(parts) == 1 and parts[0]:
        target = evaluate(parts[0], symbols, here)
    else:
        raise AsmError("JMP takes {cond,}address")
    if not 0 <= target < 256:
        raise AsmError("jump target %d is outside the 256-word program RAM" % target)
    return word | target


def _dma(name, operand, symbols, here):
    hold = name.startswith("DMAH")
    digits = re.sub(r"^DMAH?", "", name)
    stride_value = int(digits) if digits else 1
    if stride_value not in STRIDES:
        raise AsmError("address addition %d is not one of 0, 1, 2, 4, 8, 16, 32, 64" % stride_value)
    parts = [p.strip() for p in operand.split(",")]
    if len(parts) != 3:
        raise AsmError("DMA takes source,destination,count")
    first, second, count = parts
    if first.upper() == "D0":
        direction = 0
        ram = second.upper()
    elif second.upper() == "D0":
        direction = 1
        ram = first.upper()
    else:
        raise AsmError("one side of a DMA must be D0")
    if ram in ("PRG", "PR"):
        if direction != 0:
            raise AsmError("program RAM can only be a destination")
        ram_field = 4
    elif ram in SOURCES and SOURCES[ram] < 4:
        ram_field = SOURCES[ram]
    else:
        raise AsmError("DMA RAM must be M0-M3 or PRG, not %s" % ram)
    word = 0xC0000000 | (STRIDES[stride_value] << 15) | ((1 if hold else 0) << 14) | (direction << 12) | (ram_field << 8)
    counter = count.upper()
    if counter in SOURCES and SOURCES[counter] < 8:
        # the count is read from a data RAM word: bit 13 selects that form
        return word | (1 << 13) | SOURCES[counter]
    value = evaluate(count, symbols, here)
    if not 0 <= value <= 255:
        raise AsmError("DMA count %d does not fit 8 bits" % value)
    return word | value


# ---------------------------------------------------------------------------
# Disassembler
# ---------------------------------------------------------------------------

_SRC_NAMES = {v: k for k, v in SOURCES.items()}
_DEST_NAMES = {v: k for k, v in D1_DESTS.items()}
_IMM_NAMES = {v: k for k, v in IMM_DESTS.items()}
_COND_NAMES = {v: k for k, v in CONDITIONS.items()}
_ALU_NAMES = {v: k for k, v in ALU_OPS.items()}


def disassemble(word):
    cls = word >> 30
    if cls == 0:
        parts = []
        alu = (word >> 26) & 0xF
        parts.append(_ALU_NAMES.get(alu, "ALU?%d" % alu))
        x_op = (word >> 23) & 7
        x_src = _SRC_NAMES[(word >> 20) & 7]
        if (x_op & 3) == 2:
            parts.append("MOV MUL,P")
        if x_op >= 3 and (x_op & 3) == 3:
            parts.append("MOV %s,P" % x_src)
        if x_op & 4:
            parts.append("MOV %s,X" % x_src)
        y_op = (word >> 17) & 7
        y_src = _SRC_NAMES[(word >> 14) & 7]
        if (y_op & 3) == 1:
            parts.append("CLR A")
        elif (y_op & 3) == 2:
            parts.append("MOV ALU,A")
        elif y_op >= 3 and (y_op & 3) == 3:
            parts.append("MOV %s,A" % y_src)
        if y_op & 4:
            parts.append("MOV %s,Y" % y_src)
        d1 = (word >> 12) & 3
        if d1 == 1:
            imm = word & 0xFF
            imm = imm - 256 if imm > 127 else imm
            parts.append("MOV %d,%s" % (imm, _DEST_NAMES.get((word >> 8) & 0xF, "?")))
        elif d1 == 3:
            parts.append("MOV %s,%s" % (_SRC_NAMES.get(word & 0xF, "?"), _DEST_NAMES.get((word >> 8) & 0xF, "?")))
        return " ".join(parts)
    if cls == 2:
        dest = _IMM_NAMES.get((word >> 26) & 0xF, "?")
        if word & (1 << 25):
            imm = word & 0x7FFFF
            imm = imm - (1 << 19) if imm & (1 << 18) else imm
            return "MVI %d,%s,%s" % (imm, dest, _COND_NAMES.get((word >> 19) & 0x3F, "?"))
        imm = word & 0x1FFFFFF
        imm = imm - (1 << 25) if imm & (1 << 24) else imm
        return "MVI %d,%s" % (imm, dest)
    sub = (word >> 28) & 3
    if sub == 0:
        stride = [0, 1, 2, 4, 8, 16, 32, 64][(word >> 15) & 7]
        mnemonic = ("DMAH" if word & (1 << 14) else "DMA") + str(stride)
        ram = (word >> 8) & 7
        ram_name = "PRG" if ram == 4 else "M%d" % (ram & 3)
        count = ("%s" % _SRC_NAMES[word & 7]) if word & (1 << 13) else str(word & 0xFF)
        if word & (1 << 12):
            return "%s %s,D0,%s" % (mnemonic, ram_name, count)
        return "%s D0,%s,%s" % (mnemonic, ram_name, count)
    if sub == 1:
        if word & (1 << 25):
            return "JMP %s,%d" % (_COND_NAMES.get((word >> 19) & 0x3F, "?"), word & 0xFF)
        return "JMP %d" % (word & 0xFF)
    if sub == 2:
        return "LPS" if word & (1 << 27) else "BTM"
    return "ENDI" if word & (1 << 27) else "END"


# ---------------------------------------------------------------------------
# Simulator (the Ymir SCU DSP semantics)
# ---------------------------------------------------------------------------

M48 = (1 << 48) - 1
M32 = 0xFFFFFFFF


def _s32(value):
    value &= M32
    return value - (1 << 32) if value & 0x80000000 else value


class Memory:
    """Flat external memory for DMA: a dict of 32-bit words keyed by byte address."""

    def __init__(self):
        self.words = {}

    def read(self, address):
        return self.words.get(address & ~3, 0)

    def write(self, address, value):
        self.words[address & ~3] = value & M32


class DspSim:
    def __init__(self, memory=None):
        self.memory = memory or Memory()
        self.program = [0] * PROGRAM_WORDS
        self.data = [[0] * DATA_WORDS for _ in range(4)]
        self.reset()

    def reset(self):
        self.pc = 0
        self.next_instr = 0
        self.ct = [0, 0, 0, 0]
        self.inc_ct = [0, 0, 0, 0]
        self.alu = self.ac = self.p = 0
        self.rx = self.ry = 0
        self.sign = self.zero = self.carry = self.overflow = False
        self.loop_top = 0
        self.loop_count = 0
        self.looping = False
        self.running = False
        self.ended = False
        self.dma_run = False
        self.dma = None
        self.ra0 = self.wa0 = 0
        self.steps = 0

    def load(self, words, at=0):
        for offset, word in enumerate(words):
            self.program[at + offset] = word & M32

    def start(self, entry=0):
        self.pc = entry
        self.next_instr = 0
        self.running = True

    # -- flags ---------------------------------------------------------
    def _flags(self, result, carry=None, logic=False):
        self.zero = (result & M32) == 0
        self.sign = bool(result & 0x80000000)
        if logic:
            self.carry = False
        elif carry is not None:
            self.carry = carry

    def _alu(self, op):
        ac_l = self.ac & M32
        p_l = self.p & M32
        self.alu = self.ac
        if op == 0:
            return
        if op in (1, 2, 3):
            result = (ac_l & p_l, ac_l | p_l, ac_l ^ p_l)[op - 1]
            self.alu = (self.alu & ~M32) | result
            self._flags(result, logic=True)
        elif op == 4:
            total = ac_l + p_l
            result = total & M32
            self.alu = (self.alu & ~M32) | result
            self._flags(result, carry=bool(total >> 32 & 1))
            self.overflow |= bool(((~(ac_l ^ p_l)) & (ac_l ^ total)) >> 31 & 1)
        elif op == 5:
            total = (ac_l - p_l) & ((1 << 64) - 1)
            result = total & M32
            self.alu = (self.alu & ~M32) | result
            self._flags(result, carry=bool(total >> 32 & 1))
            self.overflow |= bool(((ac_l ^ p_l) & (ac_l ^ total)) >> 31 & 1)
        elif op == 6:
            total = self.ac + self.p
            result = total & M48
            self.zero = (total << 16 & ((1 << 64) - 1)) == 0
            self.sign = bool(total >> 47 & 1)
            self.carry = bool(total >> 48 & 1)
            self.overflow |= bool(((~(self.ac ^ self.p)) & (self.ac ^ total)) >> 47 & 1)
            self.alu = result
        elif op == 8:
            self.carry = bool(ac_l & 1)
            result = (_s32(ac_l) >> 1) & M32
            self.alu = (self.alu & ~M32) | result
            self._flags(result)
        elif op == 9:
            self.carry = bool(ac_l & 1)
            result = ((ac_l >> 1) | (ac_l << 31)) & M32
            self.alu = (self.alu & ~M32) | result
            self._flags(result)
        elif op == 10:
            self.carry = bool(ac_l >> 31 & 1)
            result = (ac_l << 1) & M32
            self.alu = (self.alu & ~M32) | result
            self._flags(result)
        elif op == 11:
            self.carry = bool(ac_l >> 31 & 1)
            result = ((ac_l << 1) | (ac_l >> 31)) & M32
            self.alu = (self.alu & ~M32) | result
            self._flags(result)
        elif op == 15:
            self.carry = bool(ac_l >> 24 & 1)
            result = ((ac_l << 8) | (ac_l >> 24)) & M32
            self.alu = (self.alu & ~M32) | result
            self._flags(result)

    # -- sources and destinations -----------------------------------------
    def _read_source(self, index):
        if index < 8:
            self._run_pending_dma(index & 3)
            bank = index & 3
            if index & 4:
                self.inc_ct[bank] = 1
            return self.data[bank][self.ct[bank]]
        if index == 9:
            return self.alu & M32
        if index == 10:
            return (self.alu >> 16) & M32
        return M32

    def _write_d1(self, index, value):
        value &= M32
        if index < 4:
            self._run_pending_dma(index)
            self.data[index][self.ct[index]] = value
            self.inc_ct[index] = 1
        elif index == 4:
            self.rx = _s32(value)
        elif index == 5:
            self.p = _s32(value) & M48
        elif index == 6:
            self.ra0 = (value << 2) & 0x7FFFFFC
        elif index == 7:
            self.wa0 = (value << 2) & 0x7FFFFFC
        elif index == 10:
            self.loop_count = value & 0xFFF
        elif index == 11:
            self.loop_top = value & 0xFF
        elif index >= 12:
            bank = index & 3
            self._run_pending_dma(bank)
            self.ct[bank] = value & 0x3F
            self.inc_ct[bank] = 0

    def _write_imm(self, index, value):
        if index < 4:
            self._run_pending_dma(index)
            self.data[index][self.ct[index]] = value & M32
            self.ct[index] = (self.ct[index] + 1) & 0x3F
        elif index == 4:
            self.rx = _s32(value)
        elif index == 5:
            self.p = _s32(value) & M48
        elif index == 6:
            self.ra0 = (value << 2) & 0x7FFFFFC
        elif index == 7:
            self.wa0 = (value << 2) & 0x7FFFFFC
        elif index == 10:
            self.loop_count = value & 0xFFF
        elif index == 12:
            self.loop_top = self.pc
            self.pc = value & 0xFF

    def _cond(self, cond):
        result = False
        result |= bool(cond & 1) and self.zero
        result |= bool(cond & 2) and self.sign
        result |= bool(cond & 4) and self.carry
        result |= bool(cond & 8) and self.dma_run
        invert = bool(cond & 0x20)
        return result == invert

    def _increment_pc(self):
        if self.looping:
            if self.loop_count == 0:
                self.looping = False
                self.pc = (self.pc + 1) & 0xFF
            self.loop_count = (self.loop_count - 1) & 0xFFF
        else:
            self.pc = (self.pc + 1) & 0xFF

    # -- DMA (run to completion, as Ymir does) ---------------------------------
    def _run_pending_dma(self, bank):
        if self.dma_run and bank == (self.dma["src"] if self.dma["to_d0"] else self.dma["dst"]):
            self._run_dma()

    def _run_dma(self):
        if not self.dma_run:
            return
        d = self.dma
        address = d["addr"]
        index = d["src"] if d["to_d0"] else d["dst"]
        use_data = index <= 3
        use_program = (not d["to_d0"]) and index == 4
        program_index = d["pc"]
        count = d["count"]
        while count != 0:
            if d["to_d0"]:
                value = self.data[index][self.ct[index]] if use_data else M32
                self.memory.write(address, value)
                address += d["inc"]
            else:
                value = self.memory.read(address)
                address += d["inc"]
                if use_data:
                    self.data[index][self.ct[index]] = value
                elif use_program:
                    self.program[program_index & 0xFF] = value
                    program_index += 1
            address &= 0x7FFFFFF
            if use_data:
                self.ct[index] = (self.ct[index] + 1) & 0x3F
            count -= 1
        if not d["hold"]:
            if d["inc"] == 0:
                if d["to_d0"]:
                    self.wa0 += 4
                else:
                    self.ra0 += 4
            elif d["to_d0"]:
                self.wa0 = (address - d["inc"] + 4) & ~3
            else:
                self.ra0 = address
        self.dma_run = False
        if use_program:
            self.next_instr = 0
            self.pc = self.loop_top

    # -- one instruction -------------------------------------------------------
    def step(self):
        if not self.running:
            return False
        instr = self.next_instr
        self.next_instr = self.program[self.pc]
        if self.dma_run:
            self.dma["pc"] = self.pc
            self._run_dma()
        cls = instr >> 30
        if cls == 0:
            self._operation(instr)
        elif cls == 2:
            self._load_imm(instr)
        elif cls == 3:
            self._special(instr)
        self.steps += 1
        return True

    def run(self, max_steps=100000):
        """Runs to END/ENDI. Returns the number of instructions executed."""
        start = self.steps
        while self.running and self.steps - start < max_steps:
            self.step()
        if self.running:
            raise RuntimeError("DSP program did not end within %d instructions" % max_steps)
        return self.steps - start

    def _operation(self, instr):
        self._increment_pc()
        reads = 0

        def mark(src):
            nonlocal reads
            if src < 8:
                reads |= 1 << (src & 3)

        alu_op = (instr >> 26) & 0xF
        x_op = (instr >> 23) & 7
        x_src = (instr >> 20) & 7
        y_op = (instr >> 17) & 7
        y_src = (instr >> 14) & 7
        d1_op = (instr >> 12) & 3
        d1_dest = (instr >> 8) & 0xF
        d1_imm = instr & 0xFF
        self._alu(alu_op)
        if (x_op & 3) == 2:
            self.p = (_s32(self.rx) * _s32(self.ry)) & M48
        if x_op >= 3:
            value = _s32(self._read_source(x_src))
            mark(x_src)
            if (x_op & 3) == 3:
                self.p = value & M48
            if x_op & 4:
                self.rx = value
        if (y_op & 3) == 1:
            self.ac = 0
        elif (y_op & 3) == 2:
            self.ac = self.alu
        if y_op >= 3:
            value = _s32(self._read_source(y_src))
            mark(y_src)
            if (y_op & 3) == 3:
                self.ac = value & M48
            if y_op & 4:
                self.ry = value
        if d1_op == 1:
            imm = d1_imm - 256 if d1_imm > 127 else d1_imm
            if d1_dest < 4 and reads & (1 << d1_dest):
                self.ct[d1_dest] &= ~0x3F & 0x3F   # the write is dropped and CT restarts at 0
                self.ct[d1_dest] = 0
            elif d1_dest == 4 and x_op & 4:
                pass
            elif d1_dest == 5 and x_op & 2:
                pass
            else:
                self._write_d1(d1_dest, imm & M32)
        elif d1_op == 3:
            src = d1_imm & 0xF
            mark(src)
            if d1_dest >= 4 or not reads & (1 << d1_dest):
                if d1_dest == 4 and x_op & 4:
                    pass
                elif d1_dest == 5 and x_op & 2:
                    self._read_source(src)
                else:
                    self._write_d1(d1_dest, self._read_source(src))
            elif d1_dest < 4 and 4 <= src < 8 and d1_dest != (src & 3):
                self.inc_ct[src & 3] = 1
        for bank in range(4):
            self.ct[bank] = (self.ct[bank] + self.inc_ct[bank]) & 0x3F
            self.inc_ct[bank] = 0

    def _load_imm(self, instr):
        dest = (instr >> 26) & 0xF
        write_pc = dest == 12
        if self.looping:
            if self.loop_count == 0:
                self.looping = False
                if not write_pc:
                    self.pc = (self.pc + 1) & 0xFF
            self.loop_count = (self.loop_count - 1) & 0xFFF
        elif not write_pc:
            self.pc = (self.pc + 1) & 0xFF
        if instr & (1 << 25):
            imm = instr & 0x7FFFF
            imm = imm - (1 << 19) if imm & (1 << 18) else imm
            if not self._cond((instr >> 19) & 0x3F):
                return
        else:
            imm = instr & 0x1FFFFFF
            imm = imm - (1 << 25) if imm & (1 << 24) else imm
        self._write_imm(dest, imm)

    def _special(self, instr):
        sub = (instr >> 28) & 3
        if sub == 0:
            self._increment_pc()
            if self.dma_run:
                self._run_dma()
            to_d0 = bool(instr >> 12 & 1)
            if instr >> 13 & 1:
                index = instr & 3
                inc = bool(instr >> 2 & 1)
                count = self.data[index][self.ct[index]] & 0xFF
                if inc:
                    self.ct[index] = (self.ct[index] + 1) & 0x3F
            else:
                count = instr & 0xFF
            stride = (instr >> 15) & 7
            ram = (instr >> 8) & 7
            if to_d0:
                inc_bytes = (1 << stride) & ~1
                addr = self.wa0
            else:
                inc_bytes = (1 << (stride & 2)) & ~1
                addr = self.ra0
            self.dma = {"to_d0": to_d0, "hold": bool(instr >> 14 & 1), "count": count, "src": ram if to_d0 else 0,
                        "dst": 0 if to_d0 else ram, "inc": inc_bytes, "addr": addr, "pc": self.pc}
            self.dma_run = True
        elif sub == 1:
            self._increment_pc()
            if instr & (1 << 25):
                cond = (instr >> 19) & 0x3F
                if cond != 0 and not self._cond(cond):
                    return
            self.pc = instr & 0xFF
        elif sub == 2:
            if instr & (1 << 27):
                self.looping = True
                self._increment_pc()
            else:
                if self.loop_count != 0:
                    self.pc = self.loop_top
                else:
                    self._increment_pc()
                self.loop_count = (self.loop_count - 1) & 0xFFF
        else:
            self._increment_pc()
            self.running = False
            if instr & (1 << 27):
                self.ended = True


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def _c_array(name, words, base=0):
    """`words` from `base` on: what sits below the first instruction is not emitted."""
    words = words[base:]
    lines = ["/* Generated by tools/scu_dsp.py; do not edit. */", "#include <stdint.h>", "",
             "enum { %sAddress = %d, %sCount = %d };" % (name, base, name, len(words)),
             "static const uint32_t %s[%sCount] = {" % (name, name)]
    for i in range(0, len(words), 4):
        lines.append("    " + ", ".join("0x%08Xu" % w for w in words[i:i + 4]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    asm = sub.add_parser("asm", help="assemble a source file")
    asm.add_argument("source")
    asm.add_argument("--c", dest="c_out", help="write a C array")
    asm.add_argument("--name", default="kDspProgram")
    asm.add_argument("--list", action="store_true", help="print a listing")
    asm.add_argument("-D", dest="defines", action="append", default=[], metavar="NAME[=VALUE]",
                     help="define a symbol for IF/IFDEF (value 1 when omitted)")
    dis = sub.add_parser("dis", help="disassemble the words of a C array or a hex list")
    dis.add_argument("source")
    args = parser.parse_args(argv)
    if args.command == "asm":
        with open(args.source, "r", encoding="utf-8") as stream:
            defines = {}
            for item in args.defines:
                name, _, value = item.partition("=")
                defines[name] = int(value, 0) if value else 1
            words, symbols = assemble(stream.read(), PROGRAM_WORDS, defines)
        if args.list:
            for address, word in enumerate(words):
                print("%02X: %08X  %s" % (address, word, disassemble(word)))
        if args.c_out:
            with open(args.c_out, "w", encoding="utf-8", newline="\n") as stream:
                stream.write(_c_array(args.name, words, assemble.first_address))
        return 0
    with open(args.source, "r", encoding="utf-8") as stream:
        words = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{8})u?", stream.read())]
    for address, word in enumerate(words):
        print("%02X: %08X  %s" % (address, word, disassemble(word)))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AsmError as error:
        print("error: %s" % error, file=sys.stderr)
        sys.exit(1)
