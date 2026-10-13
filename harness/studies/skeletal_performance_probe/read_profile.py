"""Read study telemetry and compare the timer with hardware VBlank counts."""
import argparse
import json
import re
import statistics as stats
import struct
from pathlib import Path

FIELDS = ('phase ccr wait_ms input_ms sample_ms skeleton_ms skin_ms submit_ms '
          'flush_ms cpu_ms end_ms frame_ms commands late_swaps draw_waits '
          'draw_timeouts vblank_end display_frames_end').split()

def analyze(map_path, dump_path, output, hz=60, probe_log=None):
    text = map_path.read_text()
    data = dump_path.read_bytes()
    def address(symbol):
        return int(re.search(r'0x([0-9a-fA-F]+)\s+_?'+symbol+r'\b',text).group(1),16)
    start = address('g_skeletal_study') - 0x06000000
    magic, initial_ccr, count, capacity = struct.unpack_from('>4I',data,start)
    if magic not in (0x53545532, 0x53545533, 0x53545534) or not 4 < count <= capacity <= 128:
        raise ValueError('invalid study header or insufficient completed frames')
    fields = FIELDS + (['projection_ticks', 'prepare_ticks', 'painter_ticks', 'emit_ticks']
                      if magic >= 0x53545533 else [])
    row_bytes = len(fields) * 4
    rows = [dict(zip(fields,struct.unpack_from('>'+str(len(fields))+'I',data,start+16+i*row_bytes))) for i in range(count)]
    selected = rows[4:]  # discard initial render warm-up
    medians = {key:stats.median(row[key] for row in selected) for key in FIELDS[2:16]}
    vblanks = selected[-1]['vblank_end'] - selected[0]['vblank_end']
    submitted = len(selected)-1
    timer_ms = sum(row['frame_ms'] for row in selected[1:])
    panic = struct.unpack_from('>3I',data,address('g_sat_last_panic')-0x06000000)
    summary = dict(initial_ccr=initial_ccr, samples=count, medians=medians,
                   interval_submissions=submitted, interval_vblanks=vblanks,
                   submitted_fps_from_vblank=hz*submitted/vblanks if vblanks else None,
                   interval_timer_ms=timer_ms, interval_vblank_ms=vblanks*1000/hz,
                   timer_to_vblank_ratio=timer_ms/(vblanks*1000/hz) if vblanks else None,
                   panic=list(panic), rows=rows)
    if magic >= 0x53545533:
        summary['renderer_median_frt_ticks'] = {
            key:stats.median(row[key] for row in selected) for key in fields[18:]}
        parallel_fields = ['init_result', 'slave_available', 'slave_tasks', 'master_tasks', 'errors']
        if magic == 0x53545534:
            parallel_fields.append('verified_poses')
        summary['parallel'] = dict(zip(parallel_fields,
            struct.unpack_from('>'+str(len(parallel_fields))+'I', data,
                              address('g_skeletal_study_parallel')-0x06000000)))
    if probe_log is not None:
        observed = {int(counter):int(frame) for frame,counter in re.findall(
            r'frame (\d+) -> .*count(20|80)\.png',probe_log.read_text())}
        if set(observed) != {20,80}:
            raise ValueError('missing emulator frame markers for submissions 20 and 80')
        delta = observed[80] - observed[20]
        irq_delta = rows[79]['vblank_end'] - rows[19]['vblank_end']
        summary['emulator_frame_at_submission'] = observed
        summary['emulator_fields_between_20_and_80'] = delta
        summary['irq_fields_between_20_and_80'] = irq_delta
        summary['submitted_fps_from_emulator'] = hz * 60 / delta
        summary['timer_ms_between_20_and_80'] = sum(row['frame_ms'] for row in rows[20:80])
        if abs(delta-irq_delta)>2:
            raise ValueError('guest VBlank counter disagrees with the emulator frame markers')
    output.write_text(json.dumps(summary,indent=2))
    print(json.dumps({k:v for k,v in summary.items() if k!='rows'},indent=2))
    if any(panic):
        raise ValueError('guest panicked during the study')
    if summary.get('parallel', {}).get('errors', 0):
        raise ValueError('parallel deformation disagreed with serial reference or failed')
    parallel = summary.get('parallel', {})
    if parallel.get('slave_available') and 'verified_poses' in parallel and parallel['verified_poses'] != 3:
        raise ValueError('parallel warm-up equivalence checks did not complete')
    return summary

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--map',type=Path,required=True)
    parser.add_argument('--dump',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--hz',type=int,default=60)
    parser.add_argument('--probe-log',type=Path)
    args=parser.parse_args()
    analyze(args.map,args.dump,args.output,args.hz,args.probe_log)
