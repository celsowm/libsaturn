"""Analyze bounded extended-FRT timings; submitted loops are not image FPS."""
import argparse
import json
import math
import statistics
import struct
from pathlib import Path
from character_plan import symbol, LOW_BASE, HIGH_BASE

STAGES = ('wait input_simulation evaluate environment lights normal_accumulation '
          'geometry flush hud end pose_dispatch skin_join bounds').split()
FIELDS = ('frame fields simulation ui lights_enabled commands late_swaps elapsed').split() + STAGES + ['painter', 'emission']
MAGIC = 0x434C5031
NTSC_FIELDS_PER_SECOND = 60


def compare_profiles(after, before):
    report = {}
    for workload in ('idle', 'walk', 'combat'):
        old = Path(str(before)+'_'+workload)
        new = Path(str(after)+'_'+workload)
        assert Path(str(old)+'.pad').read_bytes() == Path(str(new)+'.pad').read_bytes()
        baseline = json.loads(Path(str(old)+'_profile.json').read_text())
        result = json.loads(Path(str(new)+'_profile.json').read_text())
        # Calibration is an integer FRT count per field multiplied by 60.
        # A one-count startup phase difference is one quantization step;
        # each report already converts its ticks using its own calibration.
        previous_frequency, current_frequency = baseline['frequency_hz'], result['frequency_hz']
        assert previous_frequency % NTSC_FIELDS_PER_SECOND == 0
        assert current_frequency % NTSC_FIELDS_PER_SECOND == 0
        assert abs(previous_frequency-current_frequency) <= NTSC_FIELDS_PER_SECOND
        timings = {}
        for stage in ('elapsed', 'lights', 'normal_accumulation', 'geometry', 'flush'):
            previous = baseline['timings_ms'][stage]['median']
            current = result['timings_ms'][stage]['median']
            timings[stage] = {'before_ms': previous, 'after_ms': current,
                              'reduction_percent': round(100*(previous-current)/previous, 2)}
        previous = baseline['submissions_per_second']
        current = result['submissions_per_second']
        report[workload] = {'before_submissions_per_second': previous,
                            'after_submissions_per_second': current,
                            'throughput_gain_percent': round(100*(current-previous)/previous, 2),
                            'frequency_hz_before_after': [previous_frequency, current_frequency],
                            'timings': timings}
    report['limitations'] = 'Matched NTSC field/input workloads, with calibration differences limited to one FRT count per field. Each timing uses its own measured frequency. Not identical sampled poses or unique displayed-image FPS.'
    Path(str(after)+'_comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


def compare_no_record(prefix):
    mapping = Path(str(prefix)+'.map').read_text()
    high = Path(str(prefix)+'_high.bin').read_bytes()
    low = Path(str(prefix)+'_low.bin').read_bytes()
    assert not any(struct.unpack_from('>3I', high, symbol(mapping, 'g_sat_last_panic')-HIGH_BASE))
    assert struct.unpack_from('>I', low, symbol(mapping, 'g_clown_profile')-LOW_BASE+16)[0] == 0
    # Stable existing metrics layout: errors/body overlap/timeouts must remain zero.
    metrics = struct.unpack_from('>32I', high, symbol(mapping, 'g_clown_metrics')-HIGH_BASE)
    assert metrics[5] == metrics[9] == metrics[21] == 0, metrics
    baseline = Path(str(prefix).replace('_no_record', '_idle'))
    original = Path(str(baseline)+'_high.bin').read_bytes()
    original_map = Path(str(baseline)+'.map').read_text()
    recorded = struct.unpack_from('>I', original, symbol(original_map, 'g_clown_metrics')-HIGH_BASE)[0]
    report = {'instrumented_total_loops': recorded, 'no_record_total_loops': metrics[0],
              'program_fields': 1000,
              'limitations': 'Whole-program loop counts at identical field duration/input, including menu and warm-up. Both retain library profiling. Discrete scheduling and pose differences preclude treating this as exact per-stage instrumentation overhead.'}
    Path(str(prefix)+'_comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


def lighting_metrics(mapping, high):
    data = dict(zip(('enabled', 'verify', 'updates', 'verified', 'errors'),
                    struct.unpack_from('>5I', high, symbol(mapping, 'g_clown_lighting_jobs')-HIGH_BASE)))
    assert data['enabled'] in (0, 1) and data['verify'] in (0, 1), data
    assert data['errors'] == 0, data
    assert data['verified'] == (data['updates'] if data['verify'] else 0), data
    return data


def geometry_metrics(mapping, high):
    geometry = dict(zip(('enabled', 'verify', 'pairs', 'verified', 'errors', 'serial'),
                        struct.unpack_from('>6I', high, symbol(mapping, 'g_clown_geometry')-HIGH_BASE)))
    assert geometry['errors'] == 0, geometry
    if geometry['enabled']:
        assert geometry['pairs'] > 20, geometry
        if geometry['verify']:
            assert geometry['verified'] == geometry['pairs'], geometry
    else:
        assert geometry['pairs'] == geometry['verified'] == 0, geometry
        assert geometry['serial'] > 40, geometry
    return geometry


def analyze(prefix, ambient=False):
    path = lambda suffix: Path(str(prefix) + suffix)
    mapping = path('.map').read_text()
    low = path('_low.bin').read_bytes()
    high = path('_high.bin').read_bytes()
    panic = struct.unpack_from('>3I', high, symbol(mapping, 'g_sat_last_panic') - HIGH_BASE)
    assert not any(panic), panic
    metrics = struct.unpack_from('>32I', high, symbol(mapping, 'g_clown_metrics')-HIGH_BASE)
    assert metrics[5] == metrics[9] == metrics[21] == 0, metrics
    geometry = geometry_metrics(mapping, high)
    lighting = lighting_metrics(mapping, high)
    if not ambient:
        assert lighting['updates'] > 20, lighting
    offset = symbol(mapping, 'g_clown_profile') - LOW_BASE
    magic, capacity, stride, frequency, count, overwritten = struct.unpack_from('>6I', low, offset)
    assert magic == MAGIC and capacity == 192 and stride == len(FIELDS) * 4
    assert frequency > 0 and count > 20 and overwritten == max(0, count - capacity)
    rows = [dict(zip(FIELDS, struct.unpack_from('>' + str(len(FIELDS)) + 'I', low,
            offset + 24 + (i % capacity) * stride))) for i in range(max(0, count-capacity), count)]
    assert all(b['frame'] == a['frame']+1 for a, b in zip(rows, rows[1:]))
    assert all(row['elapsed'] > 0 and row['normal_accumulation'] <= row['lights']
               and row['pose_dispatch']+row['skin_join']+row['bounds'] <= row['evaluate']
               for row in rows)
    outer = [stage for stage in STAGES if stage not in
             ('normal_accumulation', 'pose_dispatch', 'skin_join', 'bounds')]
    assert all(sum(row[stage] for stage in outer) <= row['elapsed'] for row in rows)
    fight = [row for row in rows if row['ui'] == 1]
    assert fight
    ambient_faces = 0
    if ambient:
        assert all(row['lights_enabled'] == row['normal_accumulation'] == 0 for row in fight)
        commands = path('_vdp1.bin').read_bytes()
        expected = {(16 << 10) | (14 << 5) | 14, (14 << 10) | (14 << 5) | 16}
        seen = set()
        for offset in range(0, len(commands), 32):
            command = struct.unpack_from('>16H', commands, offset)
            if command[0] & 0x8000:
                break
            if command[0] & 15 == 2 and command[2] & 63 == 12:
                corners = struct.unpack_from('>4H', commands, command[14]*8)
                assert len(set(corners)) == 1 and corners[0] in expected, corners
                seen.add(corners[0])
                ambient_faces += 1
            assert not (command[2] & 0x38 == 0x20 and command[3] & 0x7800 == 0x4000), 'light sprite in ambient mode'
        assert ambient_faces > 100 and seen == expected
    start = fight[0]['fields'] + 120  # two seconds of warm-up at NTSC 60 fields/s
    steady = [row for row in fight if start <= row['fields'] < start + 600]
    assert len(steady) >= 20 and steady[-1]['fields'] - steady[0]['fields'] >= 540, len(steady)
    def summary(values):
        values = sorted(values)
        return {'median': round(statistics.median(values), 3),
                'p95': round(values[math.ceil(len(values)*.95)-1], 3),
                'max': round(max(values), 3)}
    timings = {key: summary([row[key]*1000/frequency for row in steady])
               for key in ['elapsed'] + STAGES + ['painter', 'emission']}
    span = (steady[-1]['fields']-steady[0]['fields'])/60
    report = {'samples': len(steady), 'frequency_hz': frequency,
              'ambient_gouraud_faces': ambient_faces, 'geometry_execution': geometry, 'lighting_execution': lighting,
              'interval_seconds': round(span, 3),
              'submissions_per_second': round((len(steady)-1)/span, 3),
              'simulation_ticks_per_second': round((steady[-1]['simulation']-steady[0]['simulation'])/span, 3),
              'missed_33ms_loop_budgets': sum(row['elapsed']*1000/frequency > 1000/30 for row in steady),
              'late_swap_delta': steady[-1]['late_swaps']-steady[0]['late_swaps'],
              'commands': summary([row['commands'] for row in steady]), 'timings_ms': timings,
              'rows': steady,
              'limitations': 'Instrumented CPU loop timings; not unique displayed-image FPS. Evaluate contains pose_dispatch, skin_join and bounds; lights contains normal_accumulation; flush contains painter and emission. Pose_dispatch includes synchronous Master skin work. Scene painter/emission use the existing 16-bit timer API.'}
    path('_profile.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({key: value for key, value in report.items() if key != 'rows'}, indent=2))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('prefix', type=Path)
    parser.add_argument('--no-record', action='store_true')
    parser.add_argument('--ambient', action='store_true')
    parser.add_argument('--compare-before', type=Path)
    args = parser.parse_args()
    if args.compare_before:
        compare_profiles(args.prefix, args.compare_before)
    elif args.no_record:
        compare_no_record(args.prefix)
    else:
        analyze(args.prefix, ambient=args.ambient)
