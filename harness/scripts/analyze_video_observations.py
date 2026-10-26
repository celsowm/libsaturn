#!/usr/bin/env python3
"""Count exact composited RGB transitions, independently of repeated VDP1 draws."""
import argparse
import csv
import hashlib
import json
import math
import re
from pathlib import Path

FIELDS=('frame','system_cycles','width','height','callback_delta','crop_x','crop_y',
        'crop_width','crop_height','comparable','compared_pixels','changed_pixels','rgb_fingerprint')


def summarize(rows, nominal_hz, after_field=300):
    if not math.isfinite(nominal_hz) or nominal_hz<=0 or after_field<0:
        raise ValueError('require positive nominal refresh and nonnegative warmup')
    if not rows or [r['frame'] for r in rows]!=list(range(len(rows))):
        raise ValueError('video observations must contain contiguous program fields starting at zero')
    selected=[r for r in rows if r['frame']>=after_field]
    if len(selected)<2:raise ValueError('need at least two video observations after warmup')
    geometry=('width','height','crop_x','crop_y','crop_width','crop_height')
    for index,row in enumerate(selected):
        if any(row[k]!=selected[0][k] for k in geometry):
            raise ValueError('video dimensions/crop changed inside the measured interval')
        if row['callback_delta']!=1 or row['comparable'] not in (0,1) or (index and row['comparable']!=1):
            raise ValueError('need one fresh, comparable composited video callback per field')
        x,y,w,h=(row[k] for k in ('crop_x','crop_y','crop_width','crop_height'))
        if (min(x,y)<0 or min(w,h)<=0 or x+w>row['width'] or y+h>row['height'] or
                row['compared_pixels']!=(w*h if row['comparable'] else 0) or
                not 0<=row['changed_pixels']<=row['compared_pixels']):
            raise ValueError('invalid compared crop/pixel counts')
    steps=[b['system_cycles']-a['system_cycles'] for a,b in zip(selected,selected[1:])]
    if min(steps)<=0:raise ValueError('non-advancing video system clock')
    for a,b in zip(selected,selected[1:]):
        if b['changed_pixels']==0 and a['rgb_fingerprint']!=b['rgb_fingerprint']:
            raise ValueError('unchanged RGB pixels have a different fingerprint')
    transitions=selected[1:] # First selected image is the interval's baseline.
    changes=sum(r['changed_pixels']>0 for r in transitions)
    held=longest=0
    for r in transitions:
        held=held+1 if not r['changed_pixels'] else 0
        longest=max(longest,held)
    intervals=len(transitions)
    return dict(first_field=selected[0]['frame'],last_field=selected[-1]['frame'],
        interval_fields=intervals,nominal_refresh_hz=nominal_hz,interval_seconds=intervals/nominal_hz,
        crop={k:selected[0][k] for k in geometry},rgb_change_transitions=changes,
        unchanged_fields=intervals-changes,longest_no_change_fields=longest,
        rgb_changes_per_second=changes*nominal_hz/intervals,
        system_cycle_interval=selected[-1]['system_cycles']-selected[0]['system_cycles'],
        cycles_per_field_range=[min(steps),max(steps)],
        scope='Exact RGB transitions between adjacent composited emulator fields in the selected crop. '
              'Repeated draw lists/swaps are not counted as changes. '
              'Not unique animation poses, CPU submissions or hardware certification.')


def analyze(csv_path, probe_report, after_field=300):
    metadata=json.loads(probe_report.read_text())
    video=metadata.get('video_observations',{})
    if video.get('version')!=1 or video.get('equality')!='exact-rgb-pixels-alpha-excluded':
        raise ValueError('unsupported video observation metadata')
    if metadata['boot'].get('synchronous_video_readback') is not True:
        raise ValueError('video readback must be synchronous')
    rows=[]
    with csv_path.open(newline='') as stream:
        reader=csv.DictReader(stream)
        if tuple(reader.fieldnames or ())!=FIELDS:raise ValueError('invalid video CSV schema')
        for raw in reader:
            if None in raw or any(v is None for v in raw.values()):raise ValueError('truncated video CSV row')
            if not re.fullmatch('[0-9a-f]{1,16}',raw['rgb_fingerprint']):
                raise ValueError('invalid RGB fingerprint')
            row={k:int(raw[k],16 if k=='rgb_fingerprint' else 10) for k in FIELDS}
            if any(v<0 for v in row.values()):raise ValueError('negative video observation field')
            rows.append(row)
    if len(rows)!=metadata['frames_run']:raise ValueError('partial video capture')
    result=summarize(rows,metadata['boot']['nominal_refresh_hz'],after_field)
    result.update(csv_sha256=hashlib.sha256(csv_path.read_bytes()).hexdigest(),
                  probe_report_sha256=hashlib.sha256(probe_report.read_bytes()).hexdigest(),
                  cache_emulation=metadata['boot']['sh2_cache_emulation'],
                  division_timing_model=metadata['division_timing_model'])
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--csv',type=Path,required=True)
    p.add_argument('--probe-report',type=Path,required=True)
    p.add_argument('--after-field',type=int,default=300)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();result=analyze(a.csv,a.probe_report,a.after_field)
    a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))


if __name__=='__main__':main()
