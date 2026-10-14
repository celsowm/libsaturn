"""Build controlled System ID comparisons, preserving the application bytes."""
import argparse
import json
from pathlib import Path
try:
    from boot_image import digest, inspect, read_image
    from iso_to_raw import convert_iso_to_raw
except ImportError:
    from tools.boot_image import digest, inspect, read_image
    from tools.iso_to_raw import convert_iso_to_raw


def variants(source: bytes):
    for name, offsets in (('original', ()), ('reserved_e4_zero', (0xe4,)),
                          ('slave_ec_default', (0xec,)), ('both_zero', (0xe4, 0xec))):
        image = bytearray(source)
        for offset in offsets:
            image[offset:offset+4] = bytes(4)
        allowed = {i for offset in offsets for i in range(offset, offset+4)}
        assert all(a == b or i in allowed for i, (a, b) in enumerate(zip(source, image)))
        yield name, bytes(image)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    source = read_image(args.image)
    before = inspect(source)
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, image in variants(source):
        report = inspect(image)
        assert report['first_read_sha256'] == before['first_read_sha256']
        iso, raw = args.out / (name+'.iso'), args.out / (name+'.bin')
        iso.write_bytes(image)
        convert_iso_to_raw(iso, raw)
        (args.out / (name+'.cue')).write_text(
            f'FILE "{raw.name}" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n', encoding='ascii')
        manifest.append(dict(name=name, report=report, raw_sha256=digest(raw.read_bytes())))
    (args.out / 'variants.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print(f'[variants] four comparisons written to {args.out}')


if __name__ == '__main__':
    main()
