#!/usr/bin/env python3
"""Compare retained static cube runs, including configuration and model pixels."""
import argparse
import hashlib
import json
from pathlib import Path


def compare_case(before, after, normalize_calibration=False):
    for key in ('stage', 'divisions', 'hud', 'vertices_per_actor',
                'faces_per_actor'):
        if before['header'][key] != after['header'][key]:
            raise ValueError('cube configuration differs: '+key)
    if after['header']['stage'] == 9:
        raise ValueError('animated cubes require matched poses, not field screenshots')
    if not normalize_calibration and before['header']['frequency'] != after['header']['frequency']:
        raise ValueError('FRT calibration differs; explicitly normalize after checking clock policy')
    for key in ('probe', 'bios'):
        if not before['provenance'].get(key) or (
                before['provenance'][key] != after['provenance'].get(key)):
            raise ValueError('cube provenance differs: '+key)
    for key in ('working_ram', 'commands', 'faces'):
        if before[key] != after[key]:
            raise ValueError('cube workload differs: '+key)
    for run in (before, after):
        if run['header']['errors'] or run['header']['timeouts'] or run['header']['frequency'] <= 0:
            raise ValueError('cube guest failed')
        if run['measured_fields'] < 600:
            raise ValueError('cube interval is too short')
    costs = {}
    for key in before['stage_us']:
        old = before['stage_us'][key]['median']
        new = after['stage_us'][key]['median']
        costs[key] = dict(before_us=old, after_us=new,
                         reduction_percent=100*(old-new)/old if old else None)
    return dict(costs=costs,
                before_frt_hz=before['header']['frequency'],
                after_frt_hz=after['header']['frequency'],
                before_submit_s=before['submissions_per_nominal_second'],
                after_submit_s=after['submissions_per_nominal_second'],
                before_working_bytes=sum(v['size'] for v in before['working_buffers'].values()),
                after_working_bytes=sum(v['size'] for v in after['working_buffers'].values()))


def load_reports(paths):
    cases = {}
    for path in paths:
        report = json.loads(path.read_text())
        if report.get('completed') is not True:
            raise ValueError('incomplete cube run: '+str(path))
        for name, case in report['cases'].items():
            if name in cases:
                raise ValueError('duplicate cube case: '+name)
            directory = path.parent/name
            elf = directory/'cube_pipeline_bench.elf'
            if hashlib.sha256(elf.read_bytes()).hexdigest() != case['elf_sha256']:
                raise ValueError('retained ELF differs: '+str(elf))
            cases[name] = (case, directory)
    return cases


def compare(baseline, candidate, normalize_calibration=False):
    from PIL import Image, ImageChops
    before = load_reports(baseline)
    after = load_reports([candidate])
    if not after or not after.keys() <= before.keys():
        raise ValueError('baseline must include every candidate cube case')
    cases = {}
    for name, (new, new_dir) in after.items():
        old, old_dir = before[name]
        result = compare_case(old, new, normalize_calibration)
        images = []
        for directory in (old_dir, new_dir):
            with Image.open(directory/'screen.png') as picture:
                if picture.size != (320, 224):
                    raise ValueError('unexpected cube screenshot dimensions')
                images.append(picture.convert('RGB').crop((0, 48, 320, 192)))
        pixels = ImageChops.difference(*images).tobytes()
        result['different_model_pixels'] = sum(
            pixels[i:i+3] != b'\0\0\0' for i in range(0, len(pixels), 3))
        result['before_elf_sha256'] = old['elf_sha256']
        result['after_elf_sha256'] = new['elf_sha256']
        cases[name] = result
    return dict(passed=all(c['different_model_pixels'] == 0 for c in cases.values()),
                cases=cases, model_crop=[0, 48, 320, 192],
                calibration_normalization_allowed=normalize_calibration,
                scope='Retained static cube controls and screenshots; timings are '
                'instrumented emulator submissions, not displayed frames or hardware '
                'qualification. This does not certify source/build-flag equivalence '
                'or all runtime settings; establish those from retained commands.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, nargs='+', required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--normalize-calibration', action='store_true',
                        help='allow independently calibrated FRT rates after checking clock policy')
    args = parser.parse_args()
    report = compare(args.baseline, args.candidate, args.normalize_calibration)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    raise SystemExit(0 if report['passed'] else 1)
