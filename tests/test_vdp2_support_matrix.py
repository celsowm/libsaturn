"""Validates docs/VDP2_SUPPORT_MATRIX.json against the real headers and manual tree.

The matrix is the machine-readable companion of the VDP2 coverage row in
docs/LIBSATURN_CURRENT_COVERAGE.md and of the open territory listed in
docs/VDP2_ADVANCED_EXAMPLES_IMPLEMENTATION_PLAN.md. It may only change
together with the code it describes; this test keeps it from drifting:

* every cited manual chapter must exist in the vendored VDP2 manual;
* every cited API header and test artifact must exist in the repository;
* evidence rules hold per status (exposed needs API and tests, partial needs
  API and a note, absent needs a plan milestone and no API);
* every recorded constant must still parse, in the cited header, to the
  recorded value;
* the feature and constant sets themselves are pinned, so a matrix edit is a
  deliberate change reviewed together with this file.
"""

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MATRIX_PATH = ROOT / "docs" / "VDP2_SUPPORT_MATRIX.json"

EXPECTED_FEATURE_IDS = frozenset({
    # exposed
    "normal_layers",
    "vram_cycle_planning",
    "line_scroll",
    "vertical_cell_scroll",
    "line_color_screen",
    "back_screen",
    "windows",
    "mosaic",
    "screen_color_calc_ratios",
    "color_offset",
    "color_calculation_ratio_add",
    "cram_modes",
    "hires_sprite_lighting",
    "rbg0_rotation_parameter_a",
    "vram_free_list_allocator",
    "cram_range_allocator",
    # partial
    "sprite_window",
    # absent (plan milestones)
    "unified_resource_planner",
    "rbg0_rotation_parameter_b",
    "rbg1",
    "hardware_blur",
    "extended_color_calculation",
    "special_priority_color_calc",
    "vdp2_shadow_sprites",
    "interlace_output",
    "pal_240_256_line_modes",
})

EXPECTED_CONSTANT_NAMES = frozenset({
    "SAT_VDP2_NBG0",
    "SAT_VDP2_NBG1",
    "SAT_VDP2_NBG2",
    "SAT_VDP2_NBG3",
    "SAT_VDP2_SCREEN_NBG0",
    "SAT_VDP2_SCREEN_NBG1",
    "SAT_VDP2_SCREEN_NBG2",
    "SAT_VDP2_SCREEN_NBG3",
    "SAT_VDP2_SCREEN_RBG0",
    "SAT_VDP2_SCREEN_SPRITE",
    "SAT_VDP2_SCREEN_COLOR_CALC",
    "SAT_VDP2_WINDOW_AREA_OFF",
    "SAT_VDP2_WINDOW_AREA_INSIDE",
    "SAT_VDP2_WINDOW_AREA_OUTSIDE",
    "SAT_VDP2_COLOR_CALC_RATIO",
    "SAT_VDP2_COLOR_CALC_ADD",
    "SAT_VDP2_LAYER_NBG0",
    "SAT_VDP2_LAYER_NBG1",
    "SAT_VDP2_LAYER_NBG2",
    "SAT_VDP2_LAYER_NBG3",
    "SAT_VDP2_LAYER_RBG0",
    "SAT_VDP2_LAYER_BACK",
    "SAT_VDP2_LAYER_SPRITE",
    "SAT_VDP2_LAYER_ALL",
    "SAT_VDP2_VRAM_NO_BASE",
    "SAT_VDP2_VRAM_WORD_CAPACITY",
    "SAT_VDP2_CRAM_WORD_CAPACITY",
})

_NUMERIC_LITERAL = re.compile(r"0[xX][0-9a-fA-F]+|\d+")


def _parse_c_literal(token: str) -> int:
    return int(token.rstrip("uU"), 0)


def _find_enum_value(name: str, header_text: str):
    match = re.search(rf"\b{re.escape(name)}\s*=\s*(\d+)", header_text)
    return _parse_c_literal(match.group(1)) if match else None


def _find_define_value(name: str, header_text: str):
    for line in header_text.splitlines():
        match = re.match(rf"#define\s+{re.escape(name)}\s+(.*)$", line)
        if match:
            literal = _NUMERIC_LITERAL.search(match.group(1))
            if literal:
                return _parse_c_literal(literal.group(0))
    return None


class TestVdp2SupportMatrix(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.matrix = json.loads(MATRIX_PATH.read_text(encoding="utf-8"))
        cls.features = {f["id"]: f for f in cls.matrix["features"]}
        cls.constants = {c["name"]: c for c in cls.matrix["constants"]}
        cls.manual_root = ROOT / cls.matrix["manual_root"]

    def test_statuses_are_declared_and_used(self):
        declared = set(self.matrix["statuses"])
        self.assertEqual(declared, {"exposed", "partial", "absent"})
        for feature in self.matrix["features"]:
            self.assertIn(feature["status"], declared, feature["id"])

    def test_feature_ids_are_pinned(self):
        self.assertEqual(set(self.features), EXPECTED_FEATURE_IDS)

    def test_constant_names_are_pinned(self):
        self.assertEqual(set(self.constants), EXPECTED_CONSTANT_NAMES)

    def test_cited_manual_chapters_exist(self):
        for feature in self.matrix["features"]:
            self.assertTrue(feature["manual"], feature["id"])
            for chapter in feature["manual"]:
                self.assertTrue(
                    (self.manual_root / chapter).is_file(),
                    f"{feature['id']}: missing manual chapter {chapter}")

    def test_cited_api_headers_exist(self):
        for feature in self.matrix["features"]:
            self.assertTrue(feature["api"] or feature["status"] == "absent", feature["id"])
            for entry in feature["api"]:
                header = entry.split(":", 1)[0]
                self.assertTrue(
                    (ROOT / header).is_file(),
                    f"{feature['id']}: missing API header {header}")

    def test_cited_test_artifacts_exist(self):
        for feature in self.matrix["features"]:
            for entry in feature["tests"]:
                self.assertTrue(
                    (ROOT / entry).exists(),
                    f"{feature['id']}: missing test artifact {entry}")

    def test_evidence_rules_per_status(self):
        for feature in self.matrix["features"]:
            status = feature["status"]
            if status == "exposed":
                self.assertTrue(feature["api"], feature["id"])
                self.assertTrue(feature["tests"], feature["id"])
            elif status == "partial":
                self.assertTrue(feature["api"], feature["id"])
                self.assertTrue(feature.get("notes"), feature["id"])
            else:  # absent
                self.assertFalse(feature["api"], feature["id"])
                self.assertFalse(feature["tests"], feature["id"])
                self.assertTrue(feature.get("plan_milestone"), feature["id"])

    def test_constants_match_the_headers(self):
        for name, constant in self.constants.items():
            header_path = ROOT / constant["header"]
            self.assertTrue(header_path.is_file(), f"{name}: missing header")
            header_text = header_path.read_text(encoding="utf-8")
            if constant["kind"] == "enum":
                found = _find_enum_value(name, header_text)
            elif constant["kind"] == "define":
                found = _find_define_value(name, header_text)
            else:
                self.fail(f"{name}: unknown kind {constant['kind']}")
            self.assertIsNotNone(found, f"{name}: not found in {constant['header']}")
            self.assertEqual(
                found, constant["value"],
                f"{name}: header says {found}, matrix says {constant['value']}")


if __name__ == "__main__":
    unittest.main()
