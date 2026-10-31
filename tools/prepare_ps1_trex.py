"""Run with Blender --background --python to prepare the PS1 reconstruction."""
import argparse
import sys

import bpy
import bmesh


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--triangles', type=int, default=1800)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps = 30
bpy.context.scene.frame_start = 0
bpy.context.scene.frame_end = 40
bpy.ops.import_scene.gltf(filepath=args.input, merge_vertices=True)
for obj in list(bpy.context.scene.objects):
    if obj.type == 'ARMATURE':
        obj.data.pose_position = 'REST'
for obj in list(bpy.context.scene.objects):
    if obj.type != 'MESH' or not any(m.type == 'ARMATURE' for m in obj.modifiers):
        continue
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    # glTF splits points at UV/normal seams. Decimating those disconnected
    # borders independently opens cracks. Weld the geometry first; Blender
    # stores UVs per corner, so this retains the texture seams. Different
    # skin weights (notably the upper/lower lips) must remain independent.
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    mesh.verts.ensure_lookup_table()
    groups = {}
    targets = {}
    for vertex in obj.data.vertices:
        key = (tuple(round(c, 6) for c in vertex.co),
               tuple(sorted((g.group, round(g.weight, 6)) for g in vertex.groups
                            if g.weight > 0.000001)))
        if key in groups:
            targets[mesh.verts[vertex.index]] = mesh.verts[groups[key]]
        else:
            groups[key] = vertex.index
    bmesh.ops.weld_verts(mesh, targetmap=targets)
    mesh.to_mesh(obj.data)
    mesh.free()
    obj.data.update()
    # Keep the small untextured eye/mouth details intact. An area-based
    # decimator otherwise spends their whole budget on the large body.
    preserved = None
    original_objects = set(bpy.context.scene.objects)
    for face in obj.data.polygons:
        material = obj.data.materials[face.material_index]
        face.select = 'Untextured_faces' in material.name
    if any(face.select for face in obj.data.polygons):
        bpy.context.tool_settings.mesh_select_mode = (False, False, True)
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.separate(type='SELECTED')
        bpy.ops.object.mode_set(mode='OBJECT')
        preserved, = set(bpy.context.scene.objects) - original_objects
    reserve = sum(len(f.vertices) - 2 for f in preserved.data.polygons) if preserved else 0
    triangles = sum(len(face.vertices) - 2 for face in obj.data.polygons)
    modifier = obj.modifiers.new('Saturn geometry budget', 'DECIMATE')
    modifier.ratio = min(1.0, (args.triangles - reserve) / triangles)
    modifier.use_collapse_triangulate = True
    bpy.ops.object.modifier_move_up(modifier=modifier.name)
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    if preserved:
        preserved.select_set(True)
        bpy.ops.object.join()
    print('SATURN_MESH', obj.name, len(obj.data.vertices), len(obj.data.polygons))
for obj in list(bpy.context.scene.objects):
    if obj.type == 'ARMATURE':
        obj.data.pose_position = 'POSE'
bpy.ops.export_scene.gltf(
    filepath=args.output, export_format='GLB', export_animations=True,
    export_animation_mode='NLA_TRACKS', export_skins=True, export_frame_range=True,
)
