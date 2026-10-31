"""Decode a complete audio source to sector-aligned 44.1 kHz stereo CD-DA PCM.

No gain, time stretching, remixing, lossy encoding or crossfade is applied.
The final sector is padded with silence; the source is never trimmed.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def convert(source, output, ffmpeg):
    if source.resolve() == output.resolve():
        raise ValueError('Input and output paths must differ')
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + '.tmp')
    try:
        subprocess.run([ffmpeg, '-v', 'error', '-y', '-i', str(source),
                        '-map', '0:a:0', '-ar', '44100', '-ac', '2',
                        '-c:a', 'pcm_s16le', '-f', 's16le', str(temporary)], check=True)
        size = temporary.stat().st_size
        if not size or size % 4:
            raise ValueError('Decoded stereo PCM is empty or incomplete')
        padding = (-size) % 2352
        with temporary.open('ab') as pcm:
            pcm.write(bytes(padding))
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    report = {'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'pcm_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
              'source_pcm_frames': size // 4, 'padding_frames': padding // 4,
              'sample_rate': 44100, 'channels': 2, 'bits': 16, 'byte_order': 'little',
              'duration_seconds': (size + padding) / (44100 * 4),
              'processing': 'Decode and CD sample-rate conversion only; preserve full reference duration'}
    output.with_suffix(output.suffix + '.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--ffmpeg', default='ffmpeg')
    args = parser.parse_args()
    print('[cd-audio-convert]', convert(args.input, args.output, args.ffmpeg))
