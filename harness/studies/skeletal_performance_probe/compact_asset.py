"""Create an isolated, lossless skin-record deduplication candidate.

This deliberately operates on the current generated robot C, not production
exporter behavior. Faces, palettes, texture tables and clips stay unchanged.
"""
import argparse
import json
import re
from pathlib import Path

def compact(source_prefix, output_prefix):
    base = 'robot_police_skeletal'
    source = source_prefix.with_suffix('.c').read_text()
    header = source_prefix.with_suffix('.h').read_text()
    def block(symbol):
        pattern = r'((?:static )?const [^\n]+ ' + symbol + r')\[(\d+)\] = \{\n(.*?)\n\};'
        hit = re.search(pattern, source, re.S)
        if not hit:
            raise ValueError(f'missing generated table {symbol}')
        lines = hit.group(3).splitlines()
        values = [tuple(map(int, re.findall(r'-?\d+', line))) for line in lines]
        return hit, lines, values
    rh, rl, rv = block(base + '_rigid')
    bh, bl, bv = block(base + '_blend')
    remap, selected, tables = [], [], []
    for offset, lines, values in ((0,rl,rv),(len(rv),bl,bv)):
        seen, kept = {}, []
        for i, key in enumerate(values):
            if key not in seen:
                seen[key] = len(selected)
                selected.append(offset+i)
                kept.append(lines[i])
            remap.append(seen[key])
        tables.append(kept)
    vh, vl, vv = block(base + '_bind_vertices')
    ih, il, iv = block(base + '_indices')
    old_indices = [i for row in iv for i in row]
    new_indices = [remap[i] for i in old_indices]
    edits = [(rh, tables[0]), (bh, tables[1]), (vh, [vl[i] for i in selected])]
    # Prove every new face corner refers to the same quantized deformation
    # record as before, including all weights and joint-local anchors.
    old_records = [('rigid',x) for x in rv] + [('blend',x) for x in bv]
    new_records = [old_records[i] for i in selected]
    assert all(old_records[i] == new_records[j] for i,j in zip(old_indices,new_indices))
    for hit, lines in sorted(edits, key=lambda edit:edit[0].start(), reverse=True):
        source = source[:hit.start()] + hit.group(1) + f'[{len(lines)}] = {{\n' + '\n'.join(lines) + '\n};' + source[hit.end():]
    ih = re.search(r'(const uint16_t '+base+r'_indices\[\d+\] = \{\n).*?(\n\};)',source,re.S)
    index_lines = ['    ' + ', '.join(map(str,new_indices[i:i+16])) + ',' for i in range(0,len(new_indices),16)]
    source = source[:ih.start()] + ih.group(1) + '\n'.join(index_lines) + ih.group(2) + source[ih.end():]
    source = source.replace(f', 0, {len(rv)}, {len(bv)},',f', 0, {len(tables[0])}, {len(tables[1])},')
    source = source.replace(f'{base}_bind_vertices, {len(vv)},',f'{base}_bind_vertices, {len(selected)},')
    header = header.replace(f'VERTEX_COUNT {len(vv)}',f'VERTEX_COUNT {len(selected)}')
    output_prefix.parent.mkdir(parents=True,exist_ok=True)
    output_prefix.with_suffix('.c').write_text(source)
    output_prefix.with_suffix('.h').write_text(header)
    report = {'original_vertices':len(vv),'unique_vertices':len(selected),
              'rigid':len(tables[0]),'blend':len(tables[1]),
              'removed_vertices':len(vv)-len(selected),
              'face_corner_records_equal':True,'face_count':len(old_indices)//4}
    output_prefix.with_suffix('.compact.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report))

if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path,default=Path('build/generated/skeletal_animation_3d/robot_police_skeletal'))
    parser.add_argument('--output',type=Path,default=Path('harness/build/skeletal_compact/robot_police_skeletal'))
    args=parser.parse_args()
    compact(args.source,args.output)
