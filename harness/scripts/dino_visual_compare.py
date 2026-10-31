"""Decode the lateral PS1 capture and assemble a first visual comparison.

Requires Pillow. Inputs are local emulator artifacts, never reference assets.
The two sequences have independent phases; this is not a pixel error metric.
"""
import argparse
import array
import json
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw


def decode_vram(path):
    words = array.array('H')
    words.frombytes(path.read_bytes())
    if sys.byteorder != 'little':
        words.byteswap()
    assert len(words) >= 1024 * 512, 'Incomplete PS1 VRAM'
    pixels = bytearray()
    for y in range(240):
        for word in words[y*1024:y*1024+640]:
            pixels.extend(((word & 31)*255//31,
                           ((word >> 5) & 31)*255//31,
                           ((word >> 10) & 31)*255//31))
    return Image.frombytes('RGB', (640, 240), bytes(pixels))


def panel(left, right, caption):
    result = Image.new('RGB', (1280, 520))
    result.paste(left.resize((640, 480), Image.Resampling.NEAREST), (0, 40))
    result.paste(right.resize((640, 480), Image.Resampling.NEAREST), (640, 40))
    draw = ImageDraw.Draw(result)
    draw.text((10, 5), 'PS1 original | yaw 90, pitch 0 | 4:3 display', fill='white')
    draw.text((650, 5), 'Saturn reconstruction | yaw 90, pitch 0 | 4:3 display', fill='white')
    draw.text((10, 23), caption, fill='white')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--reference-field', type=int, default=800,
                        help='PS1 field selected for the static comparison')
    args = parser.parse_args()
    directory = args.directory
    camera = (directory/'saturn-camera.bin').read_bytes()
    assert struct.unpack_from('>ii', camera, 120) == (90, 0), \
        'Saturn camera is not in profile'
    records = [json.loads(line) for line in
               (directory/'side-fields.jsonl').read_text().splitlines()]
    assert all(record['rotation'] == [0, 1024, 0] for record in records), \
        'PS1 capture is not an exact lateral view'
    saturn = sorted(directory.glob('saturn_*.png'))
    assert len(records) >= 15 and len(saturn) >= len(records)
    images = []
    for record, saturn_path in zip(records, saturn):
        ps1_path = directory / f"ps1_{record['field']:04d}.vram"
        ps1 = decode_vram(ps1_path)
        ps1.save(ps1_path.with_suffix('.png'))
        sat = Image.open(saturn_path).convert('RGB')
        assert sat.size == (640, 224), sat.size
        images.append(panel(ps1, sat,
            f"Independent phases | PS1 field {record['field']}, phase {record['phase']}"
            f" | Saturn field {saturn_path.stem.split('_')[-1]}"))
    selected = next(i for i, record in enumerate(records)
                    if record['field'] == args.reference_field)
    images[selected].save(directory/'comparison-profile.png')
    images[0].save(directory/'walk-profile.gif', save_all=True, append_images=images[1:],
                   duration=round(4000/59.94), loop=0)
    print(f'Compared {len(images)} lateral captures; phases remain independent.')


if __name__ == '__main__':
    main()
