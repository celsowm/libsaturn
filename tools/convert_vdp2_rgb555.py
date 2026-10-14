#!/usr/bin/env python3
"""Compile a VDP2 RGB555 bitmap into an SLZ1-compressed, big-endian asset."""
import argparse
import json
from pathlib import Path
from PIL import Image
from satlz import compress, decompress
from saturn_asset_common import format_byte_array, sanitize_identifier


def convert(source, prefix, symbol):
    image = Image.open(source).convert('RGB')
    width, height = image.size
    if (width, height) not in ((512, 256), (512, 512), (1024, 256), (1024, 512)):
        raise ValueError('VDP2 bitmap dimensions must be 512/1024 by 256/512')
    raw = bytearray()
    rgb = image.tobytes()
    for offset in range(0, len(rgb), 3):
        r, g, b = rgb[offset:offset + 3]
        word = 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
        raw.extend(word.to_bytes(2, 'big'))
    packed = compress(bytes(raw))
    if decompress(packed) != raw:
        raise ValueError('SLZ1 bitmap round-trip mismatch')
    name = sanitize_identifier(symbol)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    prefix.with_suffix('.h').write_text(
        '#pragma once\n#include <stdint.h>\n'
        f'#define {name.upper()}_WIDTH {width}u\n#define {name.upper()}_HEIGHT {height}u\n'
        f'#define {name.upper()}_BYTES {len(raw)}u\n#define {name.upper()}_PACKED_BYTES {len(packed)}u\n'
        f'extern const uint8_t {name}_packed[{len(packed)}];\n', encoding='utf-8')
    prefix.with_suffix('.c').write_text(
        f'#include "{prefix.name}.h"\nconst uint8_t {name}_packed[{len(packed)}] = {{\n'
        + format_byte_array(list(packed)) + '\n};\n', encoding='utf-8')
    report = {'width': width, 'height': height, 'decoded_bytes': len(raw), 'packed_bytes': len(packed)}
    prefix.with_suffix('.report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--out-prefix', type=Path, required=True)
    parser.add_argument('--symbol', default='bitmap')
    args = parser.parse_args()
    convert(args.input, args.out_prefix, args.symbol)
