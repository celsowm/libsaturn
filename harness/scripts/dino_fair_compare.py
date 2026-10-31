"""Assemble controlled PS1/Saturn renders without smoothing or color edits.

Raw inputs and the temporary capture patch are described in
docs/DINO_FAIR_COMPARISON.md. Requires NumPy and Pillow.
"""
import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageOps

from dino_visual_compare import decode_vram


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def font(size):
    try:
        return ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', size)
    except OSError:
        return ImageFont.load_default()


def assemble(directory):
    records = [json.loads(line) for line in
               (directory / 'ps1.jsonl').read_text().splitlines()]
    assert {(r['yaw'], r['phase']) for r in records} == {
        (yaw, phase) for yaw in (270, 0) for phase in (0, 8, 16, 24)}
    outputs = {}
    inputs = {}
    panels = {}
    buffer_differences = {}
    for yaw, label in ((90, 'Profile'), (0, 'Front')):
        source_yaw = 270 if yaw == 90 else 0
        sheet = Image.new('RGB', (1280, 4 * 536))
        for index, phase in enumerate((0, 8, 16, 24)):
            ps1_path = directory / f'ps1_{source_yaw}_{phase:02d}.vram'
            sat_path = directory / f'saturn_{yaw}_{phase:02d}.png'
            words = np.frombuffer(ps1_path.read_bytes(), dtype='<u2').reshape(-1, 1024)
            different = int(np.count_nonzero(words[:240, :640] != words[240:480, :640]))
            assert different == 0, 'PS1 buffers differ: pose may not be settled'
            buffer_differences[ps1_path.name] = different
            ps1 = ImageOps.mirror(decode_vram(ps1_path)).resize(
                (640, 480), Image.Resampling.NEAREST)
            with Image.open(sat_path) as raw:
                assert raw.size == (640, 448)
                sat = raw.convert('RGB').resize((640, 480), Image.Resampling.NEAREST)
            panel = Image.new('RGB', (1280, 536))
            panel.paste(ps1, (0, 56))
            panel.paste(sat, (640, 56))
            draw = ImageDraw.Draw(panel)
            draw.text((12, 5), 'PS1 | 640 x 240 | PCSX-Redux', fill='white', font=font(18))
            draw.text((652, 5), 'Saturn | 640 x 448 interlaced | Ymir', fill='white', font=font(18))
            draw.text((12, 30),
                      f'{label}, Walk pose {phase}/32 | Matched projection | '
                      'PS1 reflected horizontally | Nearest display scaling',
                      fill='#cccccc', font=font(15))
            sheet.paste(panel, (0, index * 536))
            panels[yaw, phase] = panel
            inputs[ps1_path.name] = digest(ps1_path)
            inputs[sat_path.name] = digest(sat_path)
        path = directory / f'{label.lower()}-four-poses.png'
        sheet.save(path)
        outputs[label.lower()] = str(path)
    preview = Image.new('RGB', (1280, 1072))
    preview.paste(panels[90, 0], (0, 0))
    preview.paste(panels[0, 0], (0, 536))
    preview.save(directory / 'comparison.png')
    panels[90, 0].save(directory / 'profile.png')
    panels[0, 0].save(directory / 'front.png')

    details = Image.new('RGB', (1280, 720))
    draw = ImageDraw.Draw(details)
    for row, (name, rect) in enumerate((('Head', (115, 210, 275, 310)),
                                       ('Torso', (250, 180, 410, 280)))):
        for column, name_console in enumerate(('PS1', 'Saturn')):
            panel = panels[90, 0].crop((column * 640, 56, (column + 1) * 640, 536))
            crop = panel.crop(rect).resize((480, 300), Image.Resampling.NEAREST)
            details.paste(crop, (column * 640 + 80, row * 360 + 50))
            draw.text((column * 640 + 12, row * 360 + 10),
                      f'{name_console} | {name} | Same screen rectangle, 3x nearest',
                      fill='white', font=font(17))
    details.save(directory / 'details.png')

    # Compare camera matrices using all source vertices, before raster rounding.
    poses = np.frombuffer((directory / 'walk-fixed.bin').read_bytes(), dtype='<i2')
    poses = poses.reshape(32, 1376, 4)[:, :, :3].astype(float)
    projection_records = [json.loads(line) for line in
                          (directory / 'ps1-projection.jsonl').read_text().splitlines()]
    camera_errors = {}
    for yaw, slot, state in ((90, 0, 'saturn-side-state.bin'), (0, 5, 'saturn-state.bin')):
        r = next(row for row in projection_records if row['slot'] == slot)
        c = np.asarray(r['control'], dtype='<u4')
        rotation = c[:5].view('<i2')[:9].reshape(3, 3).astype(float) / 4096
        translation = c[5:8].view('<i4').astype(float)
        gte = poses @ rotation.T + translation
        ps1_x = c[24] / 65536 + c[26] * gte[:, :, 0] / gte[:, :, 2]
        ps1_y = (c[25] / 65536) % 240 + c[26] * gte[:, :, 1] / gte[:, :, 2]
        reference = np.stack((640 - ps1_x, 2 * ps1_y), axis=-1)
        vp = np.frombuffer((directory / state).read_bytes(), dtype='>i4',
                           count=16, offset=48).reshape(4, 4) / 65536
        world = poses * np.array([1, -1, 1]) / 4000 + [0, 1.8390055, 1.5776986]
        clip = np.concatenate((world, np.ones((*world.shape[:2], 1))), axis=2) @ vp.T
        screen = np.stack((320 * (1 + clip[:, :, 0] / clip[:, :, 3]),
                           240 * (1 - clip[:, :, 1] / clip[:, :, 3])), axis=-1)
        error = np.abs(screen - reference)
        assert float(error.max()) < 0.05, 'Camera projections are not matched'
        camera_errors[str(yaw)] = {'max_display_pixel_deviation': float(error.max()),
                                  'mean_display_pixel_deviation': float(error.mean()),
                                  'source_vertices': 1376, 'walk_poses': 32}

    cadence = json.loads((directory / 'cadence-samples.json').read_text())
    samples = cadence['samples']
    duration_fixed = 32 * 65536 // 24
    times = [samples[0]['time_fixed']]
    for previous, current in zip(samples, samples[1:]):
        times.append(times[-1] + (current['time_fixed'] - previous['time_fixed']) % duration_fixed)
    fields = np.array([s['emulator_fields'] for s in samples])
    ticks = np.array([s['tick'] for s in samples])
    slope = float(np.polyfit(fields / 60, np.array(times) / 65536, 1)[0])
    tick_slope = float(np.polyfit(ticks / 60, np.array(times) / 65536, 1)[0])
    assert abs(tick_slope - 1.4) < 0.002
    measured_cycle = (duration_fixed / 65536) / slope
    source_cadence = json.loads(Path('harness/studies/dino-ps1-cadence.json').read_text())
    nominal_cycle = (duration_fixed / 65536) / 1.4
    report = {
        'purpose': 'Controlled renderer comparison, not untouched gameplay or hardware benchmarking',
        'controlled_app_sha256': (directory / 'capture.app.sha256').read_text().strip(),
        'production_app_sha256': cadence['app_sha256'],
        'pose_control': 'Same recovered Walk coordinates; independent jaw/roar channels excluded on both sides',
        'ps1_buffer_pixel_differences': buffer_differences,
        'camera_checks': camera_errors,
        'camera_check_scope': 'Ideal vertex projection before GTE reciprocal/raster rounding and Saturn pose quantization; edge coordinates, not image error',
        'image_transforms': 'PS1 horizontal reflection; full viewports mapped to 640x480 with nearest sampling; no color edits, smoothing or silhouette rescaling',
        'cadence': {
            'ps1_cycle_seconds': source_cadence['period_seconds'],
            'saturn_nominal_cycle_seconds': nominal_cycle,
            'saturn_estimated_cycle_seconds_from_field_samples': measured_cycle,
            'saturn_clock_rate_per_display_second': tick_slope,
            'nominal_cycle_difference_percent': 100 * (nominal_cycle / source_cadence['period_seconds'] - 1),
            'saturn_scene_updates_per_second_in_sample':
                (samples[-1]['rendered_scenes'] - samples[0]['rendered_scenes']) * 60 /
                (samples[-1]['emulator_fields'] - samples[0]['emulator_fields']),
            'ps1_guest_updates_per_second_from_cycle': 32 / source_cadence['period_seconds'],
            'scope': 'Emulator observations; animation period and program update rate are different metrics. Saturn sample is the unmodified front view, fields 600..810.'
        },
        'input_sha256': inputs,
        'outputs': outputs,
        'limitations': ['Different scene backgrounds and lighting are preserved.',
                        'Frozen poses do not assess motion combing or displayed frame pacing.',
                        'No physical hardware measurements.']
    }
    (directory / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'camera_checks': camera_errors, 'cadence': report['cadence']}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    assemble(parser.parse_args().directory)
