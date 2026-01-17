#!/usr/bin/env python3
import sys
import struct
import argparse

def parse_obj(filename):
    vertices = []
    faces = []
    uvs = []
    
    with open(filename, 'r') as f:
        for line in f:
            if line.startswith('v '):
                parts = line.split()
                x, y, z = map(float, parts[1:4])
                vertices.append((x, y, z))
            elif line.startswith('vt '):
                parts = line.split()
                u, v = map(float, parts[1:3])
                uvs.append((u, v))
            elif line.startswith('f '):
                parts = line.split()[1:]
                face = []
                for part in parts:
                    if '/' in part:
                        v_idx, vt_idx = part.split('/')[:2]
                        face.append((int(v_idx), int(vt_idx) if vt_idx else -1))
                    else:
                        face.append((int(part), -1))
                faces.append(face)
    
    return vertices, uvs, faces

def generate_c_header(vertices, uvs, faces):
    output = []
    output.append("#include \"saturn/shared.h\"")
    output.append("")
    output.append("static const Quad quads[] = {")
    
    for face in faces:
        if len(face) == 4:
            output.append("    {")
            for i, (v_idx, vt_idx) in enumerate(face):
                v = vertices[v_idx - 1]
                uv = uvs[vt_idx - 1] if vt_idx > 0 else (0.0, 0.0)
                
                output.append(f"        {{(fix16_t)({v[0] * 65536}), (fix16_t)({v[1] * 65536}), (fix16_t)({v[2] * 65536})}},")
                output.append(f"        {{(fix16_t)({uv[0] * 65536}), (fix16_t)({uv[1] * 65536})}}},")
            output.append("    },")
    
    output.append("};")
    output.append("")
    output.append(f"static const u32 quad_count = {len([f for f in faces if len(f) == 4])};")
    
    return '\n'.join(output)

def main():
    parser = argparse.ArgumentParser(description='Convert OBJ files to Saturn vertex data')
    parser.add_argument('input', help='Input .obj file')
    parser.add_argument('output', help='Output .c file')
    args = parser.parse_args()
    
    vertices, uvs, faces = parse_obj(args.input)
    
    header = generate_c_header(vertices, uvs, faces)
    
    with open(args.output, 'w') as f:
        f.write(header)
    
    print(f"Converted {len(vertices)} vertices and {len(faces)} faces to {args.output}")

if __name__ == '__main__':
    main()
