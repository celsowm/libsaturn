#!/usr/bin/env python3
"""Classify exact edge-register checkpoints without instrumenting guest code.

The checkpoint and six registers MUST be reviewed in the matching disassembly.
This tool cannot infer register liveness from a function name or a PC histogram.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path

OLD_EDGE_BITS = 15
NEW_EDGE_BITS = 16
CPU_NAMES = ('master', 'slave')


def classify(rows, pc, registers, opcode):
    if len(registers) != 6 or len(set(registers)) != 6 or any(r not in range(16) for r in registers):
        raise ValueError('six distinct live edge registers are required')
    result = {cpu: dict(checkpoints=0, old_range=0, newly_eligible=0, wide_range=0)
              for cpu in CPU_NAMES}
    for row in rows:
        if int(row['pc']) != pc:
            continue
        if row['cpu'] not in result or int(row['opcode0']) != opcode:
            raise ValueError('checkpoint CPU/opcode does not match reviewed disassembly')
        values = [int(row[f'r{r}']) for r in registers]
        if any(v < 0 or v >= 1 << 32 for v in values):
            raise ValueError('invalid unsigned SH-2 register value')
        edges = [v if v < 1 << 31 else v - (1 << 32) for v in values]
        inside = lambda bits: all(-(1 << (bits-1)) <= v < 1 << (bits-1) for v in edges)
        category = 'old_range' if inside(OLD_EDGE_BITS) else (
            'newly_eligible' if inside(NEW_EDGE_BITS) else 'wide_range')
        result[row['cpu']]['checkpoints'] += 1
        result[row['cpu']][category] += 1
    if not any(cpu['checkpoints'] for cpu in result.values()):
        raise ValueError('no matching edge checkpoints')
    return result


def analyze(prefix, elf, pc, registers, opcode):
    metadata = json.loads(Path(str(prefix)+'.json').read_text())
    if metadata['truncated'] or metadata['dropped_frames'] or not metadata['frames_stored']:
        raise ValueError('incomplete debug capture')
    for cpu in CPU_NAMES:
        if any(metadata[cpu][k] for k in ('dropped_pc_observations','dropped_events','dropped_snapshots')):
            raise ValueError('incomplete CPU capture')
    with Path(str(prefix)+'.snapshots.csv').open() as source:
        counts = classify(csv.DictReader(source), pc, registers, opcode)
    return dict(elf_sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
        checkpoint_pc=pc, opcode=opcode, edge_registers=registers,
        trace_from=metadata['trace_from'], frames_stored=metadata['frames_stored'], cpus=counts,
        scope='Exact observed edge-box eligibility, not timing or completed fast-path calls. '
              'Zero normals still fall back. Matching ELF/register liveness requires disassembly review.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--pc', type=lambda s:int(s,0), required=True)
    parser.add_argument('--registers', type=int, nargs=6, required=True)
    parser.add_argument('--opcode', type=lambda s:int(s,0), required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    report = analyze(args.prefix, args.elf, args.pc, args.registers, args.opcode)
    args.out.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
