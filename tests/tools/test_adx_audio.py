#!/usr/bin/env python3
"""Exercise the ADX asset converter with synthetic, redistributable audio."""
import math
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    if not shutil.which("ffmpeg"):
        print("ADX converter: skipped (FFmpeg not installed)")
        return 0
    with tempfile.TemporaryDirectory() as tmp:
        folder = Path(tmp)
        source = folder / "tone.wav"
        payload = folder / "tone.adxbin"
        metadata = folder / "tone_adx.h"
        frames = 1000
        with wave.open(str(source), "wb") as wav:
            wav.setnchannels(2)
            wav.setsampwidth(2)
            wav.setframerate(22050)
            pcm = bytearray()
            for i in range(frames):
                left = int(12000 * math.sin(2 * math.pi * 440 * i / 22050))
                right = int(9000 * math.sin(2 * math.pi * 660 * i / 22050))
                pcm += struct.pack("<hh", left, right)
            wav.writeframes(bytes(pcm))
        subprocess.run([
            sys.executable, str(ROOT / "tools/convert_audio.py"),
            "--input", str(source), "--output-audio", str(payload),
            "--output-header", str(metadata), "--name", "adx_test",
            "--sample-rate", "22050", "--channels", "2", "--format", "adx",
        ], check=True, stdout=subprocess.DEVNULL)
        content = payload.read_bytes()
        header = metadata.read_text()
        assert len(content) == 4 + ((frames + 31) // 32) * 36
        assert "SAT_AUDIO_ADX_ADPCM" in header
        assert "SAT_AUDIO_ADX_TEST_SAMPLE_COUNT 1000u" in header
        assert "SAT_AUDIO_ADX_TEST_CHANNELS 2u" in header
        coeffs = struct.unpack(">hh", content[:4])
        assert 0 < coeffs[0] < 8192
        assert -4096 < coeffs[1] < 0
        assert not (content[4] & 0x80)   # first frame is not an EOF marker
        assert "SAT_AUDIO_ADX_TEST_BYTES " + str(len(content)) + "u" in header
        print("ADX asset converter: OK")


if __name__ == "__main__":
    sys.exit(main())
