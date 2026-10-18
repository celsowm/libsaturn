#!/usr/bin/env python3
"""Decode bounded Dino phase telemetry; never compare timestamps across SH-2s."""
import statistics
import struct
import json
from pathlib import Path

ROWS = 64
FIELDS = ('serial field collect publish master_geometry join merge flush painter emit '
          'present work master_ccr master_tcr worker_valid join_response worker_source worker_geometry '
          'worker_output worker_total worker_ccr worker_tcr').split()
MASTER_PHASES = 'collect publish master_geometry join merge flush present work'.split()
WORKER_PHASES = 'worker_source worker_geometry worker_output worker_total'.split()


def decode(raw):
    if len(raw) != 16+4*ROWS*len(FIELDS):
        raise ValueError('invalid Dino phase telemetry size')
    magic, version, frequency, count = struct.unpack_from('>4I', raw)
    if magic != 0x44505231 or version != 1 or not frequency or count < 3:
        raise ValueError('invalid Dino phase telemetry header')
    samples = []
    for serial in range(max(0, count-ROWS), count):
        # Guest may already be overwriting the next slot at the dump boundary.
        if count >= ROWS and serial % ROWS == count % ROWS:
            continue
        sample = dict(zip(FIELDS, struct.unpack_from('>'+str(len(FIELDS))+'I',
                            raw, 16+4*len(FIELDS)*(serial % ROWS))))
        if sample['serial'] != serial or not sample['worker_valid']:
            raise ValueError('incomplete Dino phase sample or failed worker')
        if (sample['master_tcr'] & 3) != 2 or (sample['worker_tcr'] & 3) != 2:
            raise ValueError('report requires both local FRT prescalers /128')
        if any(sample[key] > sample['work'] for key in MASTER_PHASES):
            raise ValueError('Master phase exceeds complete work interval')
        if sum(sample[key] for key in MASTER_PHASES[:-1]) > sample['work']:
            raise ValueError('non-overlapping Master phases exceed work interval')
        if sample['join_response'] > sample['join']:
            raise ValueError('response wait exceeds complete join interval')
        if sample['work'] >= 65536:
            raise ValueError('raw local subphases need work shorter than one FRT wrap')
        if sample['painter']+sample['emit'] > sample['flush']:
            raise ValueError('flush subphases exceed flush interval')
        if sum(sample[key] for key in WORKER_PHASES[:-1]) != sample['worker_total']:
            raise ValueError('wrapped or inconsistent worker FRT interval')
        # Worker starts after publication and is complete before merge. Bound
        # each raw local interval against an extended Master containing span;
        # no counter origins are compared. This also rejects possible FRT wrap.
        bound = sample['publish']+sample['master_geometry']+sample['join']
        if bound >= 65536 or sample['worker_total'] > bound:
            raise ValueError('worker interval cannot be bounded below one FRT wrap')
        samples.append(sample)
    if any(b['field'] <= a['field'] for a, b in zip(samples, samples[1:])):
        raise ValueError('non-advancing Dino phase field')
    costs = {}
    for name in MASTER_PHASES+['painter', 'emit', 'join_response']+WORKER_PHASES:
        values = sorted(s[name]*1e6/frequency for s in samples)
        costs[name] = dict(median=statistics.median(values), maximum=values[-1],
                          p95=values[(95*len(values)+99)//100-1])
    return dict(samples=len(samples), first_field=samples[0]['field'],
                last_field=samples[-1]['field'], frequency=frequency, phase_us=costs,
                observed_ccr={cpu: sorted({s[cpu+'_ccr'] for s in samples})
                              for cpu in ('master', 'worker')},
                scope='Instrumented local CPU elapsed intervals including stalls/interrupts. '
                      'Worker work overlaps Master work; do not add them. Both FRTs use /128; '
                      'conversion uses Master calibration, never cross-CPU timestamps. '
                      'Publish includes dispatch; join includes completion consumption; join_response '
                      'is only executor response waits, excluding service/consumption. '
                      'VDP1 transfer is inside present; no distinct display FPS claim.')


def compare_runs(before, after):
    """Compare retained runs only with matching scenario/emulator/clock policy."""
    paths=[Path(before), Path(after)]
    runs=[json.loads((path/'analysis.json').read_text()) for path in paths]
    for key in ('scenario', 'bios_sha256', 'probe_sha256'):
        if runs[0][key] != runs[1][key]:
            raise ValueError('incompatible Dino runs: '+key)
    checkpoints=[[point['fields'] for point in run['checkpoints']] for run in runs]
    if not checkpoints[0] or checkpoints[0] != checkpoints[1]:
        raise ValueError('incompatible Dino checkpoint windows')
    field=checkpoints[0][-1]
    phases=[decode((path/f'field-{field}_g_dino_pipeline.bin').read_bytes()) for path in paths]
    for key in ('frequency', 'observed_ccr'):
        if phases[0][key] != phases[1][key]:
            raise ValueError('incompatible Dino timer/cache policy: '+key)
    costs={}
    for name, old in phases[0]['phase_us'].items():
        new=phases[1]['phase_us'][name]
        costs[name]=dict(before_median_ms=old['median']/1000,
                         after_median_ms=new['median']/1000,
                         median_change_percent=(new['median']/old['median']-1)*100
                             if old['median'] else None)
    return dict(before=str(paths[0]), after=str(paths[1]), scenario=runs[0]['scenario'],
                checkpoint=field, samples=[phase['samples'] for phase in phases],
                submissions_per_second=[run['nominal_submissions_per_second'] for run in runs],
                phase_costs=costs, scope=phases[1]['scope'])


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compare', type=Path, nargs=2, required=True,
                        metavar=('BEFORE', 'AFTER'), help='compare retained instrumented run directories')
    args=parser.parse_args()
    print(json.dumps(compare_runs(*args.compare), indent=2))
