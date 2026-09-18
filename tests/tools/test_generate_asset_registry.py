#!/usr/bin/env python3
"""Host coverage for the deterministic compiled asset registry generator."""

from __future__ import annotations

import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "tools" / "generate_asset_registry.py"


def main() -> int:
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        manifest = work / "manifest.json"
        source = work / "registry.c"
        header = work / "registry.h"
        manifest.write_text(
            json.dumps(
                {
                    "version": 1,
                    "assets": [
                        {
                            "logical_path": "audio/theme.satstream",
                            "kind": "stream",
                            "embedded_symbol": "theme_pcm",
                            "size": 16,
                            "format": "pcm_s16",
                            "sample_rate": 11025,
                            "sample_count": 8,
                            "channels": 1,
                        },
                        {
                            "logical_path": "assets/player.bin",
                            "kind": "data",
                            "physical_path": "ASSETS/PLAYER.BIN",
                            "size": 64,
                        },
                    ],
                }
            ),
            encoding="utf-8",
        )
        subprocess.run(
            [
                "python",
                str(GENERATOR),
                "--input",
                str(manifest),
                "--output",
                str(source),
                "--header-output",
                str(header),
                "--function",
                "register_fixture_assets",
            ],
            check=True,
            cwd=ROOT,
        )
        generated = source.read_text(encoding="utf-8")
        assert generated.index('"assets/player.bin"') < generated.index(
            '"audio/theme.satstream"'
        )
        assert 'asset_0.source_path = "ASSETS/PLAYER.BIN";' in generated
        assert "asset_1.data = (const void*)theme_pcm;" in generated
        assert "SAT_AUDIO_PCM_S16" in generated
        assert "sat_result_t register_fixture_assets(void);" in header.read_text(
            encoding="utf-8"
        )
    print("asset registry generator: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
