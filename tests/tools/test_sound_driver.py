"""The 68000 sound driver: the source, the checked-in image and the SH-2 side agree."""

import os
import re
import subprocess
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE = os.path.join(REPO, "src", "hal", "scsp", "sound_driver.m68k")
LOGIC = os.path.join(REPO, "src", "hal", "scsp", "driver_logic.hpp")


def equates():
    """name -> (base symbol or None, offset) from the .equ lines that live in the mailbox."""
    found = {}
    with open(SOURCE, "r", encoding="utf-8") as stream:
        for line in stream:
            match = re.match(r"\s*\.equ\s+(M_\w+),\s*MBOX\+(0x[0-9A-Fa-f]+)", line)
            if match:
                found[match.group(1)] = int(match.group(2), 16)
    return found


class SoundDriverLayoutTests(unittest.TestCase):
    def test_the_assembly_and_the_sh2_agree_on_the_mailbox(self):
        with open(LOGIC, "r", encoding="utf-8") as stream:
            logic = stream.read()
        mailbox = int(re.search(r"kMailbox = (0x[0-9A-Fa-f]+)u", logic).group(1), 16)
        with open(SOURCE, "r", encoding="utf-8") as stream:
            self.assertEqual(int(re.search(r"\.equ\s+MBOX,\s+(0x[0-9A-Fa-f]+)", stream.read()).group(1), 16), mailbox)
        eq = equates()
        pairs = {"M_MAGIC": "kMagicOffset", "M_VERSION": "kVersionOffset", "M_FLAGS": "kFlagsOffset",
                 "M_HEARTBEAT": "kHeartbeatOffset", "M_TICK": "kTickOffset", "M_HEAD": "kHeadOffset",
                 "M_TAIL": "kTailOffset", "M_EXECUTED": "kExecutedOffset", "M_MAXLATE": "kMaxLateOffset",
                 "M_CONTROL": "kControlOffset", "M_LATE_LAST": "kLastLateOffset", "M_RING": "kRingOffset",
                 "M_LOG": "kLogOffset"}
        for asm_name, cpp_name in pairs.items():
            found = re.search(r"%s = kMailbox \+ (0x[0-9A-Fa-f]+)u" % cpp_name, logic)
            self.assertIsNotNone(found, cpp_name)
            self.assertEqual(eq[asm_name], int(found.group(1), 16), asm_name)
        self.assertIn("RING_MASK,   63", open(SOURCE, encoding="utf-8").read())
        self.assertIn("kRingEntries = 64u", logic)

    def test_the_checked_in_image_is_current(self):
        result = subprocess.run([sys.executable, os.path.join(REPO, "tools", "build_sound_driver.py"), "--check"],
                                capture_output=True, text=True)
        if result.returncode == 2:
            self.skipTest("no 68000 assembler on this machine")
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
