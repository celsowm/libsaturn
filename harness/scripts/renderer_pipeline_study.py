#!/usr/bin/env python3
"""Report CPU costs, actual CCR values and sampled VDP1 draw spans together."""
import argparse
import csv
import json
import statistics
from pathlib import Path

from analyze_cpu_profile import analyze


def frame_metrics(rows):
    if len(rows) < 2:
        raise ValueError('need at least two frame snapshots')
    rows = [{key: int(value) for key, value in row.items()} for row in rows]
    fields = [row['frame'] for row in rows]
    if fields != list(range(fields[0], fields[0]+len(fields))):
        raise ValueError('frame snapshots are not contiguous')
    steps = [b['system_cycles']-a['system_cycles'] for a, b in zip(rows, rows[1:])]
    if min(steps) <= 0:
        raise ValueError('non-advancing system clock')
    field_cycles = statistics.median(steps)
    start, end = rows[0]['system_cycles']-field_cycles, rows[-1]['system_cycles']
    # A frame snapshot may repeat the last completed list or show the latest
    # begin with the PREVIOUS list's end. Neither is an additional draw sample.
    pairs = sorted({(row['last_begin_cycle'], row['last_end_cycle']) for row in rows
                    if start <= row['last_begin_cycle'] < row['last_end_cycle'] <= end})
    spans = [finish-begin for begin, finish in pairs]
    counters = {}
    for key in ('draw_begins', 'draw_ends', 'fb_swaps', 'visible_completed_draws'):
        if any(b[key] < a[key] for a, b in zip(rows, rows[1:])):
            raise ValueError('non-monotonic VDP1 counter')
        counters[key] = rows[-1][key]-rows[0][key]
    return dict(frame_snapshots=len(rows), counter_interval_fields=fields[-1]-fields[0],
                system_cycles_per_field=field_cycles,
                observed_ccr={cpu: sorted({row[cpu+'_ccr'] for row in rows})
                              for cpu in ('master', 'slave')},
                counter_deltas=counters, completed_draw_samples=len(spans),
                completed_draw_cycle_spans=spans,
                median_draw_fields=statistics.median(spans)/field_cycles if spans else None,
                maximum_draw_fields=max(spans)/field_cycles if spans else None,
                scope='Sampled latest completed-list spans, not a complete draw log. '
                      'Counter deltas span frame endpoints; CPU totals span the full profile. '
                      'Swap count is not distinct presented-list count.')


def study(prefix, elf, nm):
    report = analyze(prefix, elf, nm)
    meta = report['profile']
    if meta['dropped_frames'] or any(meta[cpu][key] for cpu in ('master', 'slave')
            for key in ('dropped_pc_observations', 'dropped_events', 'dropped_snapshots')):
        raise ValueError('incomplete CPU capture')
    with Path(str(prefix)+'.frames.csv').open() as stream:
        frames = list(csv.DictReader(stream))
    if len(frames) != meta['frames_stored']:
        raise ValueError('frame snapshot count differs from profile')
    report['frame_observations'] = frame_metrics(frames)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', required=True, type=Path)
    parser.add_argument('--elf', required=True, type=Path)
    parser.add_argument('--nm', default=str(Path.home()/'dev/saturn-tools/bin/sh2eb-elf-nm'))
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    result = study(args.prefix, args.elf, args.nm)
    args.out.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result['frame_observations'], indent=2))
