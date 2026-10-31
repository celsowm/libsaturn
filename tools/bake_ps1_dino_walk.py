#!/usr/bin/env python3
"""Bake a locally captured PS1 Dino walk with original or prepared geometry.

The output is ordinary glTF skin animation, with one translation joint per
unique point. Only the host importer evaluates these joints; Saturn receives
the existing baked vertex format. No proprietary animation runtime is added.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import struct
from pathlib import Path

import numpy as np

from model_pipeline import animation
from model_pipeline.gltf import parse_model
from model_pipeline.model import from_gltf


def decode_capture(directory):
    ram = (directory/'ram.bin').read_bytes()
    records = [json.loads(line) for line in
               (directory/'frames.jsonl').read_text().splitlines()]
    def words(address, count=1):
        offset = address-0x80000000
        assert 0 <= offset <= len(ram)-count*4
        return struct.unpack_from('<'+'I'*count, ram, offset)
    objects, backup, _, table = words(0x8003ddb8, 4)
    assert objects == 1, 'Capture uses an unsupported TMD layout'
    count = words(table+4)[0]
    assert 1 <= count <= 10000
    def vectors(address, count):
        assert 0 <= address-0x80000000 <= len(ram)-count*8
        return np.frombuffer(ram, dtype='<i2', offset=address-0x80000000,
                             count=count*4).reshape(-1, 4)[:, :3].astype(np.int64)
    rest = vectors(backup, count)
    channels, descriptors, _, _ = words(0x8003ddc8, 4)
    assert channels == 9
    deltas = np.zeros((channels, count, 3), dtype=np.int64)
    for i in range(channels):
        pointer, start, length, obj = words(descriptors+16*i, 4)
        assert obj == 0 and start+length <= count
        deltas[i, start:start+length] = vectors(pointer, length)
    assert len(records) >= 64
    observed = np.frombuffer((directory/'vertices.bin').read_bytes(), dtype='<i2')
    observed = observed.reshape(len(records), count, 4)[:, :, :3]
    weights = np.asarray([record['weights'] for record in records], dtype=np.int64)
    reproduced = []
    for row, record in zip(weights, records):
        assert record['counts'] == [count]
        pose = rest.copy()
        for delta, weight in zip(deltas, row):
            # GTE GPF, sf=12, signed IR saturation; destination is an SVECTOR.
            pose = (pose+np.clip(delta*weight >> 12, -32768, 32767)).astype(
                np.int16).astype(np.int64)
        reproduced.append(pose)
    error = int(np.max(np.abs(np.asarray(reproduced)-observed)))
    assert error == 0, f'VDF replay differs from emulator by {error} units'
    assert all(b['frame'] == a['frame']+1 for a, b in zip(records, records[1:]))
    assert np.array_equal(weights[:32, :4], weights[32:64, :4]), \
        'Walk weights do not repeat after 32 updates'
    walk = []
    for phase in range(32):
        i = next(i for i, row in enumerate(records) if row['phase'] == phase)
        pose = rest.copy()
        for delta, weight in zip(deltas[:4], weights[i, :4]):
            pose = (pose+np.clip(delta*weight >> 12, -32768, 32767)).astype(
                np.int16).astype(np.int64)
        walk.append(pose)
    return rest, np.asarray(walk), {
        'vertices': count, 'verified_frames': len(records),
        'emulator_replay_max_error_units': error,
        'walk_channels': [0, 1, 2, 3],
        'excluded_channels': [4, 5, 6, 7, 8],
        'ram_sha256': hashlib.sha256(ram).hexdigest(),
    }


def read_obj(path):
    vertices, triangles = [], []
    for line in path.read_text().splitlines():
        fields = line.split()
        if fields and fields[0] == 'v':
            vertices.append(tuple(map(float, fields[1:4])))
        elif fields and fields[0] == 'f':
            face = [int(v.split('/')[0])-1 for v in fields[1:]]
            assert min(face) >= 0
            triangles.extend((face[0], face[i], face[i+1]) for i in range(1, len(face)-1))
    return np.asarray(vertices), np.asarray(triangles)


def source_correspondence(rest, obj):
    transformed = rest*np.array([1., -1., 1.])/400
    offset = np.array([0., obj[:, 1].max()-transformed[:, 1].max(),
                       obj[:, 2].max()-transformed[:, 2].max()])
    transformed += offset
    distances = ((obj[:, None, :]-transformed[None, :, :])**2).sum(axis=2)
    indices = distances.argmin(axis=1)
    error = np.sqrt(distances[np.arange(len(obj)), indices])
    assert len(set(indices)) == len(rest), 'OBJ does not cover every TMD point'
    assert error.max() < 1e-5, 'OBJ is not the captured source mesh'
    return indices, offset, float(error.max())


def closest_surface(points, triangles):
    """Exact closest triangle point, returning face index and barycentric weights."""
    a, b, c = triangles[:, 0], triangles[:, 1], triangles[:, 2]
    ab, ac = b-a, c-a
    aa = (ab*ab).sum(1); bb = (ab*ac).sum(1); cc = (ac*ac).sum(1)
    determinant = aa*cc-bb*bb
    valid = determinant > 1e-20
    determinant = np.where(valid, determinant, 1)
    faces, weights, errors = [], [], []
    for point in points:
        ap = point-a
        d = (ap*ab).sum(1); e = (ap*ac).sum(1)
        u = (cc*d-bb*e)/determinant
        v = (aa*e-bb*d)/determinant
        bary = np.stack((1-u-v, u, v), axis=1)
        nearest = a+u[:, None]*ab+v[:, None]*ac
        distance = ((point-nearest)**2).sum(1)
        distance[~valid | (bary.min(1) < 0)] = np.inf
        for i, j in ((0, 1), (1, 2), (2, 0)):
            start, end = triangles[:, i], triangles[:, j]
            direction = end-start
            denominator = (direction*direction).sum(1)
            t = np.clip(((point-start)*direction).sum(1)/np.maximum(denominator, 1e-30), 0, 1)
            candidate = start+t[:, None]*direction
            candidate_distance = ((point-candidate)**2).sum(1)
            update = candidate_distance < distance
            distance[update] = candidate_distance[update]
            bary[update] = 0
            bary[update, i] = 1-t[update]
            bary[update, j] = t[update]
        face = int(distance.argmin())
        assert np.isfinite(distance[face])
        faces.append(face); weights.append(bary[face]); errors.append(distance[face]**.5)
    return np.asarray(faces), np.asarray(weights), np.asarray(errors)


def original_surface(path, template):
    """Keep the OBJ's topology and corner UVs, using the existing textures."""
    positions, uvs, records = [], [], []
    materials = {m['name'].removeprefix('PS1 | '): i
                 for i, m in enumerate(template.materials)}
    material = None
    for line in path.read_text().splitlines():
        fields = line.split()
        if not fields: continue
        if fields[0] == 'v': positions.append(tuple(float(c)*.1 for c in fields[1:4]))
        elif fields[0] == 'vt': uvs.append(tuple(map(float, fields[1:3])))
        elif fields[0] == 'usemtl': material = materials[' '.join(fields[1:])]
        elif fields[0] == 'f':
            corners = [tuple(int(c)-1 if c else -1 for c in f.split('/'))
                       for f in fields[1:]]
            for i in range(1, len(corners)-1):
                records.append((material, (corners[0], corners[i], corners[i+1])))
    model = copy.copy(template)
    model.vertices, model.uvs, model.normals = [], [], []
    model.triangles, model.tri_materials = [], []
    source_vertices = []
    for material, corners in sorted(records, key=lambda r: r[0]):
        first = len(model.vertices)
        points = np.asarray([positions[c[0]] for c in corners])
        normal = np.cross(points[1]-points[0], points[2]-points[0])
        normal /= max(float(np.linalg.norm(normal)), 1e-30)
        for corner in corners:
            source_vertices.append(corner[0])
            model.vertices.append(positions[corner[0]])
            model.uvs.append(uvs[corner[1]] if len(corner)>1 and corner[1]>=0 else (0., 0.))
            model.normals.append(tuple(normal))
        model.triangles.append((first, first+1, first+2))
        model.tri_materials.append(material)
    return model, np.asarray(source_vertices)


def write_glb(path, original, model, unique, vertex_joint, walk, roar):
    doc = copy.deepcopy(original.json)
    binary = bytearray(original.bin)
    def accessor(values, dtype, kind, target=None):
        values = np.asarray(values, dtype=dtype)
        binary.extend(bytes((-len(binary)) % 4))
        offset = len(binary); binary.extend(values.tobytes())
        view = {'buffer': 0, 'byteOffset': offset, 'byteLength': values.nbytes}
        if target: view['target'] = target
        doc['bufferViews'].append(view)
        row = {'bufferView': len(doc['bufferViews'])-1, 'componentType':
               {'<f4': 5126, '<u2': 5123, '<u4': 5125}[dtype],
               'count': len(values), 'type': kind}
        if kind in ('VEC3', 'SCALAR') and dtype == '<f4':
            row['min'] = np.atleast_1d(values.min(axis=0)).tolist()
            row['max'] = np.atleast_1d(values.max(axis=0)).tolist()
        doc['accessors'].append(row)
        return len(doc['accessors'])-1
    positions = np.asarray(model.vertices)
    joints = np.zeros((len(positions), 4), dtype=np.uint16); joints[:, 0] = vertex_joint
    weights = np.zeros((len(positions), 4)); weights[:, 0] = 1
    triangles = np.asarray(model.triangles)
    materials = np.asarray(model.tri_materials)
    primitives = []
    for material in sorted(set(model.tri_materials)):
        indices = triangles[materials == material].reshape(-1)
        used, local = np.unique(indices, return_inverse=True)
        attrs = {'POSITION': accessor(positions[used], '<f4', 'VEC3', 34962),
                 'NORMAL': accessor(np.asarray(model.normals)[used], '<f4', 'VEC3', 34962),
                 # SourceModel uses bottom-left UVs; glTF uses top-left.
                 'TEXCOORD_0': accessor(np.asarray([(u, 1-v) for u, v in model.uvs])[used],
                                        '<f4', 'VEC2', 34962),
                 'JOINTS_0': accessor(joints[used], '<u2', 'VEC4', 34962),
                 'WEIGHTS_0': accessor(weights[used], '<f4', 'VEC4', 34962)}
        primitives.append({'attributes': attrs, 'indices': accessor(local, '<u4', 'SCALAR', 34963),
                           'material': material, 'mode': 4})
    doc['meshes'] = [{'name': 'PS1 Dino motion transfer', 'primitives': primitives}]
    doc['nodes'] = [{'name': 'Dino', 'mesh': 0, 'skin': 0}] + [
        {'name': f'point_{i}', 'translation': point.tolist()} for i, point in enumerate(unique)]
    doc['scenes'] = [{'nodes': list(range(len(doc['nodes'])))}]; doc['scene'] = 0
    inverse = np.tile(np.eye(4).reshape(1, 16), (len(unique), 1))
    inverse[:, 12:15] = -unique
    doc['skins'] = [{'joints': list(range(1, len(unique)+1)),
                     'inverseBindMatrices': accessor(inverse, '<f4', 'MAT4')}]
    doc['animations'] = []
    for name, poses in [('Walk', walk), ('Roar', roar)]:
        times = np.linspace(0, 40/30, len(poses))
        time_accessor = accessor(times, '<f4', 'SCALAR')
        samplers, channels = [], []
        for i in range(len(unique)):
            samplers.append({'input': time_accessor, 'output': accessor(poses[:, i], '<f4', 'VEC3'),
                             'interpolation': 'LINEAR'})
            channels.append({'sampler': i, 'target': {'node': i+1, 'path': 'translation'}})
        doc['animations'].append({'name': name, 'samplers': samplers, 'channels': channels})
    doc['buffers'] = [{'byteLength': len(binary)}]
    doc['asset'] = {'version': '2.0', 'generator': 'LibSaturn PS1 Dino motion transfer',
                    'copyright': 'Dinosaur geometry, textures and original Walk: Sony / SCE. Reconstructed Roar: celsowm.'}
    doc.pop('extensionsRequired', None)
    metadata = json.dumps(doc, separators=(',', ':'), sort_keys=True).encode()
    metadata += b' '*((-len(metadata)) % 4)
    binary.extend(bytes((-len(binary)) % 4))
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 28+len(metadata)+len(binary))+
                     struct.pack('<II', len(metadata), 0x4e4f534a)+metadata+
                     struct.pack('<II', len(binary), 0x004e4942)+binary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    parser.add_argument('--obj', required=True, type=Path)
    parser.add_argument('--mesh', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--original-topology', action='store_true',
                        help='Preserve original OBJ geometry/UVs; transfer reconstructed Roar onto it')
    args = parser.parse_args()
    rest, original_walk, report = decode_capture(args.capture)
    obj, triangles = read_obj(args.obj)
    source_index, offset, error = source_correspondence(rest, obj)
    glb = parse_model(args.mesh); model = from_gltf(glb, 'dino')
    vertices = np.asarray(model.vertices)
    roar_clip = next(clip for clip in model.clips if clip.name == 'Roar')
    roar_source = np.asarray(animation.bake_clip_poses(model, roar_clip,
                             [i*roar_clip.duration/40 for i in range(41)]))
    if args.original_topology:
        unique = (rest*np.array([1., -1., 1.])/400+offset)*.1
        source_faces = np.asarray(model.triangles)
        faces, barycentric, roar_distance = closest_surface(unique, vertices[source_faces])
        roar_delta = roar_source[::2]-vertices[None]
        roar = unique[None]+np.einsum('fvkc,vk->fvc',
            roar_delta[:, source_faces[faces]], barycentric)
        model, obj_vertices = original_surface(args.obj, model)
        inverse = source_index[obj_vertices]
        walk = (original_walk*np.array([1., -1., 1.])/400+offset)*.1
        surface_error = np.zeros(len(unique))
        report['topology'] = 'Original OBJ, without decimation or reconstructed mouth additions'
        report['roar_surface_distance_max'] = float(roar_distance.max())
    else:
        # Mouth points with different reconstructed Roar motion stay separate.
        keys = np.concatenate((vertices, roar_source.transpose(1, 0, 2).reshape(len(vertices), -1)), axis=1)
        _, first, inverse = np.unique(np.round(keys, 7), axis=0, return_index=True, return_inverse=True)
        unique = vertices[first]
        faces, barycentric, surface_error = closest_surface(unique, obj[triangles]*.1)
        delta = (original_walk-rest)*np.array([1., -1., 1.])*.00025
        original_triangles = source_index[triangles[faces]]
        walk = unique[None, :, :]+np.einsum('fijk,ij->fik', delta[:, original_triangles], barycentric)
        roar = roar_source[::2, first]
    walk = np.concatenate((walk, walk[:1]), axis=0)
    # Retain the existing 20-pose/15 Hz Roar budget; increase only Walk.
    write_glb(args.output, glb, model, unique, inverse, walk, roar)
    loaded = from_gltf(parse_model(args.output), 'roundtrip')
    uv_error = float(np.max(np.abs(
        np.asarray(loaded.uvs)[np.asarray(loaded.triangles)] -
        np.asarray(model.uvs)[np.asarray(model.triangles)])))
    assert uv_error < 1e-6, f'UV roundtrip differs by {uv_error}'
    max_roundtrip_error = 0.
    for clip, poses in zip(loaded.clips, (walk, roar)):
        for i, pose in enumerate(poses):
            actual = np.asarray(animation.evaluate_positions(
                loaded, clip, i/(len(poses)-1)*clip.duration))[np.asarray(loaded.triangles)]
            expected = pose[inverse][np.asarray(model.triangles)]
            max_roundtrip_error = max(max_roundtrip_error, float(np.abs(actual-expected).max()))
    assert max_roundtrip_error < 2e-6, 'Emitted GLB does not preserve transferred poses'
    report.update({'obj_correspondence_max_error': error, 'obj_offset': offset.tolist(),
                   'transfer_points': len(unique), 'walk_poses': 32,
                   'surface_distance_max': float(surface_error.max()),
                   'surface_distance_p95': float(np.percentile(surface_error, 95)),
                   'glb_pose_roundtrip_max_error': max_roundtrip_error,
                   'glb_uv_roundtrip_max_error': uv_error,
                   'source_obj_sha256': hashlib.sha256(args.obj.read_bytes()).hexdigest(),
                   'source_mesh_sha256': hashlib.sha256(args.mesh.read_bytes()).hexdigest(),
                   'output_sha256': hashlib.sha256(args.output.read_bytes()).hexdigest()})
    args.report.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
