"""Extract local security/region inputs, never inherited application boot code.

Extraction establishes byte identity, not redistribution rights. Outputs belong
in ignored build storage. The native composer requires the resulting manifest.
"""
import argparse
import json
from pathlib import Path
from boot_image import digest, parse_ip


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--template', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    data = args.template.read_bytes()
    info = parse_ip(data)
    if 'invalid_regions' in info['issues'] or len(data) < info['aip_offset']:
        raise ValueError('invalid source region layout')
    args.out.mkdir(parents=True, exist_ok=True)
    def save(name, payload):
        (args.out / name).write_bytes(payload)
        return dict(file=name, sha256=digest(payload),
                    origin=f'{args.template.name}; SHA256 {digest(data)}',
                    license='Local diagnostic input; redistribution rights unverified')
    manifest = dict(schema_version=1, redistributable=False,
                    security=save('security.bin', data[0x100:0xe00]), regions=[])
    for i, symbol in enumerate(info['regions']):
        manifest['regions'].append(dict(symbol=symbol,
            **save('area_'+symbol+'.bin', data[0xe00+i*32:0xe20+i*32])))
    (args.out / 'payloads.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    print(f'[native] local inputs: {args.out / "payloads.json"}; NOT cleared for redistribution')


if __name__ == '__main__':
    main()
