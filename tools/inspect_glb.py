#!/usr/bin/env python3
"""Inspect source GLB geometry, rigs and clips using the canonical reader."""
import argparse
import hashlib
import json
from pathlib import Path

from model_pipeline.gltf import GltfError, parse_glb, read_accessor, extract_image_bytes


def inspect(path):
    glb = parse_glb(path)
    doc = glb.json
    accessors = doc.get('accessors', [])
    primitives = []
    for mesh in doc.get('meshes', []):
        for primitive in mesh['primitives']:
            position = primitive.get('attributes', {}).get('POSITION')
            vertices = accessors[position]['count'] if position is not None else 0
            indices = primitive.get('indices')
            count = accessors[indices]['count'] if indices is not None else vertices
            mode = primitive.get('mode', 4)
            primitives.append({'mesh': mesh.get('name'), 'vertices': vertices,
                               'triangles': count // 3 if mode == 4 else None,
                               'mode': mode, 'attributes': list(primitive.get('attributes', {})),
                               'bounds': {key: accessors[position].get(key) for key in ('min', 'max')}
                               if position is not None else None})
    clips = []
    for animation in doc.get('animations', []):
        times = [float(value) for sampler in animation['samplers']
                 for value in read_accessor(glb, sampler['input']).rows]
        clips.append({'name': animation.get('name'), 'duration_seconds': max(times, default=0),
                      'channels': len(animation['channels']),
                      'interpolation': sorted({s.get('interpolation', 'LINEAR')
                                               for s in animation['samplers']})})
    return {'source': str(path), 'sha256': hashlib.sha256(Path(path).read_bytes()).hexdigest(),
            'asset': doc.get('asset'), 'primitives': primitives,
            'skins': [{'name': s.get('name'), 'joints': len(s['joints'])}
                      for s in doc.get('skins', [])],
            'clips': clips, 'materials': doc.get('materials', []),
            'images': doc.get('images', []), 'extensions_used': doc.get('extensionsUsed', [])}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--extract-images', type=Path, help='Save embedded source images alongside the report')
    args = parser.parse_args()
    try:
        report = json.dumps(inspect(args.input), indent=2) + '\n'
        if args.extract_images:
            glb = parse_glb(args.input)
            args.extract_images.mkdir(parents=True, exist_ok=True)
            for index in range(len(glb.json.get('images', []))):
                payload, mime = extract_image_bytes(glb, index)
                suffix = '.png' if mime == 'image/png' else '.jpg'
                (args.extract_images / f'image_{index}{suffix}').write_bytes(payload)
    except (GltfError, OSError, ValueError) as error:
        parser.exit(1, f'{error}\n')
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding='utf-8')
    else:
        print(report, end='')


if __name__ == '__main__':
    main()
