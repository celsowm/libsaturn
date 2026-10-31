"""Analyze the read-only PCSX-Redux Dino observer's JSONL output."""
import argparse
import json
from pathlib import Path
from statistics import mean


def analyze(path, field_hz):
    rows = [json.loads(line) for line in path.read_text().splitlines() if line]
    assert len(rows) >= 240, 'Capture at least 240 warmed fields'
    wraps = []
    for previous, current in zip(rows, rows[1:]):
        assert current['field'] == previous['field'] + 1, 'Nonconsecutive fields'
        assert 0 <= current['guest_frame'] - previous['guest_frame'] <= 1
        assert current['phase'] == current['guest_frame'] % 32
        assert current['speed'] == 32, 'Original playback speed was changed'
        assert len(current['weights']) == 9
        if current['phase'] < previous['phase']:
            wraps.append(current['field'])
    assert any(row['weights'][:4] != rows[0]['weights'][:4] for row in rows[1:]), \
        'VDF weights did not animate'
    periods = [b - a for a, b in zip(wraps, wraps[1:])]
    assert len(periods) >= 3, 'Capture at least four cycle boundaries'
    seconds = mean(periods) / field_hz
    result = {
        'source': 'PCSX-Redux emulation; not a physical hardware measurement',
        'field_hz_assumed': field_hz,
        'captured_fields': len(rows),
        'stride_guest_frames': 32,
        'period_fields': periods,
        'mean_period_fields': mean(periods),
        'period_seconds': seconds,
        'reconstructed_cycle_seconds': 40 / 30,
        'suggested_walk_speed_percent': 100 * (40 / 30) / seconds,
    }
    path.with_suffix('.analysis.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--field-hz', type=float, default=59.94)
    args = parser.parse_args()
    assert args.field_hz > 0
    print(json.dumps(analyze(args.capture, args.field_hz), indent=2))
