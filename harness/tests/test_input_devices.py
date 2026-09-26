r"""Acceptance for input devices and SMPC services (examples/input_devices_demo).

Run through the wrapper, which builds the demo, runs Ymir twice with different
devices plugged in, and dumps Work RAM:

    .\harness\run-input-devices.ps1 -Bios .\bios\saturn_bios_us.bin

Scenario A: a 3D Control Pad (analog mode) on port 1 and a Shuttle Mouse on
port 2, both moved by a device script; START+A at frame 300 runs the RTC and
SMEM round trips. Scenario B: the same pad in digital mode, port 2 empty.
"""

import os
import re
import struct
import unittest

MAGIC = 0x49444531  # "IDE1"
WRAM_HIGH = 0x06000000
PORT_FIELDS = (
    "kind peripheral_id data_size axis_count multitap_id tap_count held x y l r "
    "has_triggers mouse_dx_total mouse_dy_total mouse_buttons "
    "axis_history0 axis_history1 axis_history2 axis_history3 axis_history4 axis_history5 "
    "axis_history_count"
).split()
TAIL_FIELDS = (
    "axis_events button_events connect_events mutate_done status_ok rtc_ok rtc_restored "
    "smem_ok smem_restored bad_date_refused year month day weekday hour minute second "
    "rtc_set_flag area_code reset_enable_ok "
    "tap_kind0 tap_kind1 tap_kind2 tap_kind3 tap_kind4 tap_kind5"
).split()
SIGNED = {"mouse_dx_total", "mouse_dy_total"}

KIND_NONE, KIND_PAD, KIND_ANALOG, KIND_MOUSE = 0, 1, 2, 3


def _load(env_name):
    dump = os.environ.get(env_name)
    map_path = os.environ.get("LIBSATURN_INPUT_MAP")
    if not dump or not map_path:
        raise unittest.SkipTest("run harness/run-input-devices.ps1 first")
    if not os.path.exists(dump) or not os.path.exists(map_path):
        raise AssertionError("missing input: %s / %s" % (dump, map_path))
    with open(map_path, "r", encoding="utf-8", errors="replace") as stream:
        found = re.search(r"(?m)^\s*0x([0-9A-Fa-f]+)\s+_g_input_demo\s*$", stream.read())
    if not found:
        raise AssertionError("_g_input_demo is absent from " + map_path)
    offset = int(found.group(1), 16) - WRAM_HIGH
    words = 3 + 2 * len(PORT_FIELDS) + len(TAIL_FIELDS)
    with open(dump, "rb") as stream:
        data = stream.read()[offset:offset + 4 * words]
    raw = struct.unpack(">%dI" % words, data)
    out = {"magic": raw[0], "polls": raw[1], "poll_errors": raw[2], "ports": []}
    at = 3
    for _ in range(2):
        port = {}
        for name in PORT_FIELDS:
            value = raw[at]
            if name in SIGNED and value >= 1 << 31:
                value -= 1 << 32
            port[name] = value
            at += 1
        port["history"] = [port["axis_history%d" % i] for i in range(port["axis_history_count"])]
        out["ports"].append(port)
    for name in TAIL_FIELDS:
        out[name] = raw[at]
        at += 1
    return out


def _pack(x, y, l, r):
    return (x << 24) | (y << 16) | (l << 8) | r


class ScenarioA(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load("LIBSATURN_INPUT_DUMP_A")
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_input_demo not initialised: %r" % cls.r)

    def test_polling_never_failed(self):
        self.assertEqual(self.r["poll_errors"], 0)
        self.assertGreater(self.r["polls"], 300)

    def test_three_d_pad_is_an_analog_device_with_triggers(self):
        p = self.r["ports"][0]
        self.assertEqual(p["kind"], KIND_ANALOG)
        self.assertEqual(p["peripheral_id"], 0x16)
        self.assertEqual((p["data_size"], p["axis_count"], p["has_triggers"]), (6, 4, 1))
        self.assertEqual((p["multitap_id"], p["tap_count"]), (0xF, 1))

    def test_axes_and_triggers_follow_the_script(self):
        p = self.r["ports"][0]
        self.assertEqual((p["x"], p["y"], p["l"], p["r"]), (0xF0, 0x10, 0xC8, 0x64))
        self.assertEqual(p["history"], [_pack(0x80, 0x80, 0, 0), _pack(0xF0, 0x10, 0xC8, 0x64)])

    def test_shuttle_mouse_reports_signed_motion_and_buttons(self):
        m = self.r["ports"][1]
        self.assertEqual(m["kind"], KIND_MOUSE)
        self.assertEqual(m["peripheral_id"], 0xE3)
        # 5 counts right and 3 up (dy negative down the screen) per poll from
        # frame 100 to the end: the totals stay in exact 5 : -3 proportion.
        self.assertGreater(m["mouse_dx_total"], 0)
        self.assertEqual(m["mouse_dx_total"] % 5, 0)
        self.assertEqual(m["mouse_dx_total"] * 3, -m["mouse_dy_total"] * 5)
        polls_moving = m["mouse_dx_total"] // 5
        self.assertTrue(250 <= polls_moving <= 330, polls_moving)
        self.assertEqual(m["mouse_buttons"], 1)   # SAT_MOUSE_LEFT
        self.assertEqual(m["held"], 1 << 2)       # left click rides on pad bit A

    def test_events_were_queued_for_the_axes(self):
        # Four axes moved once; the mouse raises two axis events per poll.
        self.assertGreaterEqual(self.r["axis_events"], 4)
        self.assertGreater(self.r["button_events"], 0)
        self.assertGreaterEqual(self.r["connect_events"], 2)

    def test_status_block_and_rtc_round_trip(self):
        r = self.r
        self.assertEqual(r["mutate_done"], 1, "START+A never reached the demo")
        self.assertEqual(r["status_ok"], 1)
        self.assertEqual(r["bad_date_refused"], 1)
        self.assertEqual(r["rtc_ok"], 1)
        self.assertEqual(r["rtc_restored"], 1)
        self.assertGreaterEqual(r["year"], 1980)
        self.assertTrue(1 <= r["month"] <= 12 and 1 <= r["day"] <= 31)
        self.assertTrue(r["hour"] < 24 and r["minute"] < 60 and r["second"] < 60)

    def test_smem_and_reset_enable_round_trip(self):
        self.assertEqual(self.r["smem_ok"], 1)
        self.assertEqual(self.r["smem_restored"], 1)
        self.assertEqual(self.r["reset_enable_ok"], 1)


class ScenarioB(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = _load("LIBSATURN_INPUT_DUMP_B")
        if cls.r["magic"] != MAGIC:
            raise AssertionError("g_input_demo not initialised: %r" % cls.r)

    def test_three_d_pad_in_digital_mode_is_a_plain_pad(self):
        p = self.r["ports"][0]
        self.assertEqual(p["kind"], KIND_PAD)
        self.assertEqual(p["data_size"], 2)
        self.assertEqual(p["axis_count"], 0)
        self.assertEqual(p["held"], 1 << 4)       # button B

    def test_empty_port_is_none(self):
        p = self.r["ports"][1]
        self.assertEqual(p["kind"], KIND_NONE)
        self.assertEqual(p["tap_count"], 0)
        self.assertEqual(p["peripheral_id"], 0xFF)

    def test_single_device_leaves_the_other_taps_empty(self):
        self.assertEqual([self.r["tap_kind%d" % i] for i in range(6)], [KIND_PAD, 0, 0, 0, 0, 0])

    def test_no_mutation_without_the_chord(self):
        self.assertEqual(self.r["mutate_done"], 0)
        self.assertEqual(self.r["poll_errors"], 0)


if __name__ == "__main__":
    unittest.main()
