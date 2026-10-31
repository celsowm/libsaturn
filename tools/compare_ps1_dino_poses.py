#!/usr/bin/env python3
"""Draw phase-aligned geometry studies with one orthographic camera.

These are host geometry studies, not emulator screenshots or texture checks.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from bake_ps1_dino_walk import decode_capture, read_obj, source_correspondence, closest_surface
from model_pipeline.animation import evaluate_positions
from model_pipeline.gltf import parse_model
from model_pipeline.model import from_gltf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    parser.add_argument('--obj', required=True, type=Path)
    parser.add_argument('--before', required=True, type=Path)
    parser.add_argument('--after', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    rest, walk, _ = decode_capture(args.capture)
    obj, triangles = read_obj(args.obj)
    indices, offset, _ = source_correspondence(rest, obj)
    original = walk*np.array([1., -1., 1.])/400 + offset
    original = original[:, indices]*.1
    models = [from_gltf(parse_model(path), path.stem) for path in (args.before, args.after)]
    clips = [next(c for c in model.clips if c.name == 'Walk') for model in models]
    sampled = [np.asarray([evaluate_positions(model, clip, phase/32*clip.duration)
               for phase in range(32)]) for model, clip in zip(models, clips)]
    # Give the previous reconstruction its best fixed phase alignment using
    # corresponding lower-leg surface points, rather than an arbitrary zero.
    before_faces, after_faces = [np.asarray(model.triangles) for model in models]
    before_rest = np.asarray(models[0].vertices)[before_faces].reshape(-1, 3)
    feet = before_rest[:, 1] < .45
    before_feet = sampled[0][:, before_faces].reshape(32, -1, 3)[:, feet]
    # The current asset can retain the full original topology while the
    # archived reconstruction is reduced. Compare corresponding rest-surface
    # points instead of assuming identical vertex/triangle indexing.
    foot_faces, foot_weights, _ = closest_surface(before_rest[feet],
        np.asarray(models[1].vertices)[after_faces])
    after_feet = np.einsum('fvkc,vk->fvc',
        sampled[1][:, after_faces[foot_faces]], foot_weights)
    errors = [float(np.mean((np.roll(before_feet, -shift, axis=0)-after_feet)**2))
              for shift in range(32)]
    before_offset = int(np.argmin(errors))
    args.output.with_suffix('.json').write_text(json.dumps({
        'previous_phase_offset_updates': before_offset,
        'alignment': 'minimum lower-leg surface point squared distance to transferred Walk',
        'previous_foot_rms_distance_after_alignment': errors[before_offset]**.5,
    }, indent=2)+'\n')
    columns = ['PS1 source deformation', 'Previous reconstruction', 'Transferred original Walk']
    width, height = 320, 170
    for view, screen_axes, depth_axis in [('side', (2, 1), 0),
                                          ('front', (0, 1), 2),
                                          ('back', (0, 1), 2)]:
        canvas = Image.new('RGB', (width*3, height*8+50), (20, 20, 24))
        draw = ImageDraw.Draw(canvas)
        for col, title in enumerate(columns): draw.text((col*width+8, 8), title, fill='white')
        draw.text((8, 28), f'{view}: shared camera/scale | previous phase offset {before_offset}/32 | host geometry', fill='white')
        for row, phase in enumerate(range(0, 32, 4)):
            poses = [original[phase], sampled[0][(phase+before_offset) % 32], sampled[1][phase]]
            for col, (points, faces) in enumerate(zip(poses, [triangles]+[
                    np.asarray(model.triangles) for model in models])):
                face_points = points[faces]
                depth = face_points[:, :, depth_axis].mean(axis=1)
                order = np.argsort(depth)
                if view == 'side': order = order[::-1]
                if view == 'back': order = order[::-1]
                a, b, c = face_points[:, 0], face_points[:, 1], face_points[:, 2]
                normals = np.cross(b-a, c-a)
                normals /= np.maximum(np.linalg.norm(normals, axis=1)[:, None], 1e-12)
                light = np.abs(normals @ np.array([-.6, .6, .52915]))
                x_axis, y_axis = screen_axes
                horizontal = points[:, x_axis]
                if view == 'side': horizontal = -(horizontal+.65)
                if view == 'back': horizontal = -horizontal
                projected = np.stack((col*width+width/2+horizontal*43,
                                      50+(row+1)*height-17-points[:, y_axis]*43), axis=1)
                for face in order:
                    color = tuple(int(value*(.4+.6*light[face])) for value in (190, 149, 73))
                    draw.polygon([tuple(p) for p in projected[faces[face]]], fill=color)
                draw.text((col*width+8, 50+row*height+5), f'phase {phase}/32', fill='white')
                draw.line((col*width, 50+(row+1)*height-17,
                           (col+1)*width, 50+(row+1)*height-17), fill=(65, 65, 65))
        canvas.save(args.output.with_name(args.output.stem+'-'+view+'.png'))
    print('Wrote side, front and back phase-aligned geometry studies.')


if __name__ == '__main__':
    main()
