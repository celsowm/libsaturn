"""Compare two-fighter preparation with identical input and display settings."""
import argparse
import json
import statistics
import struct
from pathlib import Path
from character_plan import (analyze, symbol, SAMPLE_FIELDS, SAMPLE_MAGIC,
                            LOW_BASE, WORD, NOMINAL_NTSC_HZ, GEOMETRY_MAGIC, GEOMETRY_FIELDS)

TRANSITION_SAMPLES=4
ACTORS=2
HANDOFF_MAGIC=0x43484E31
HANDOFF_FIELDS=('iteration wait_us end_us late_swaps draw_waits draw_timeouts '
                'display_frames vblank_start').split()

def report(prefix):
    path=lambda suffix:Path(str(prefix)+suffix)
    result=analyze('skinning_stress_3d',path('.map'),path('_high.bin'),
                   path('.json'),path('_analysis.json'))
    low=path('_low.bin').read_bytes()
    offset=symbol(path('.map').read_text(),'g_character_benchmark')-LOW_BASE
    magic,count,capacity,stride=struct.unpack_from('>4I',low,offset)
    assert magic==SAMPLE_MAGIC and 0<count<=capacity
    assert stride==WORD*len(SAMPLE_FIELDS)
    rows=[dict(zip(SAMPLE_FIELDS,struct.unpack_from('>'+str(len(SAMPLE_FIELDS))+'I',
          low,offset+WORD*4+i*stride))) for i in range(count)]
    handoffs={}
    if 'g_character_handoff' in path('.map').read_text():
        offset=symbol(path('.map').read_text(),'g_character_handoff')-LOW_BASE
        magic,handoff_count,handoff_capacity,handoff_stride=struct.unpack_from('>4I',low,offset)
        assert magic==HANDOFF_MAGIC and handoff_count==count and handoff_capacity==capacity
        assert handoff_stride==WORD*len(HANDOFF_FIELDS)
        for i in range(handoff_count):
            row=dict(zip(HANDOFF_FIELDS,struct.unpack_from('>'+str(len(HANDOFF_FIELDS))+'I',
                     low,offset+WORD*4+i*handoff_stride)))
            assert row['iteration']==rows[i]['iteration']
            assert row['draw_timeouts']==0
            handoffs[row['iteration']]=row
    geometry={}
    mapping=path('.map').read_text()
    if 'geometry' in result:
        assert result['geometry']['verified']>=2
    if 'g_character_geometry_samples' in mapping:
        offset=symbol(mapping,'g_character_geometry_samples')-LOW_BASE
        magic,geometry_count,geometry_capacity,geometry_stride=struct.unpack_from('>4I',low,offset)
        assert magic==GEOMETRY_MAGIC and geometry_count==count and geometry_capacity==capacity
        assert geometry_stride==WORD*len(GEOMETRY_FIELDS)
        for i in range(geometry_count):
            row=dict(zip(GEOMETRY_FIELDS,struct.unpack_from('>'+str(len(GEOMETRY_FIELDS))+'I',
                     low,offset+WORD*4+i*geometry_stride)))
            assert row['iteration']==rows[i]['iteration'] and row['errors']==0
            geometry[row['iteration']]=row
    segments=[]
    for row in rows:
        key=(row['requested'],row['policy'],row['lod'],row['complexity'])
        if not segments or segments[-1][0]!=key:segments.append((key,[]))
        segments[-1][1].append(row)
    workloads=[]
    for (actors,policy,lod,complexity),segment in segments:
        if actors!=ACTORS:continue
        samples=segment[TRANSITION_SAMPLES:]
        assert len(samples)>=20 and lod==0 and complexity==0
        assert all(s['accepted']==actors for s in samples)
        fields=samples[-1]['vblank']-samples[0]['vblank']
        simulation=NOMINAL_NTSC_HZ*(samples[-1]['simulation_ticks']-samples[0]['simulation_ticks'])/fields
        assert abs(simulation-NOMINAL_NTSC_HZ)<2
        workloads.append({'policy':policy,'samples':len(samples),
            'submissions_per_second':NOMINAL_NTSC_HZ*(samples[-1]['iteration']-samples[0]['iteration'])/fields,
            'simulation_per_second':simulation,
            'medians':{field:statistics.median(s[field] for s in samples) for field in
                       ('cpu_ms','preparation_ms','skin_ms','sort_us','emission_us','frame_ms','commands')},
            'frame_p95_ms':statistics.quantiles([s['frame_ms'] for s in samples],n=100,method='inclusive')[94]})
        if handoffs:
            timing=[handoffs[s['iteration']] for s in samples]
            first,last=timing[0],timing[-1]
            counters={field:last[field]-first[field] for field in
                      ('late_swaps','draw_waits','draw_timeouts','display_frames')}
            assert counters['draw_timeouts']==0
            workloads[-1]['handoff']={
                'medians_us':{field:statistics.median(s[field] for s in timing)
                              for field in ('wait_us','end_us')},
                'p95_us':{field:statistics.quantiles([s[field] for s in timing],n=100,
                             method='inclusive')[94] for field in ('wait_us','end_us')},
                'counter_deltas':counters}
        if geometry:
            timing=[geometry[s['iteration']] for s in samples]
            assert all(s['verification_us']==0 for s in timing)
            workloads[-1]['geometry']={
                'medians_us':{field:statistics.median(s[field] for s in timing) for field in
                    ('preparation_us','dispatch_us','master_us','wait_us','merge_us')},
                'pairs':timing[-1]['pairs']-timing[0]['pairs'],
                'verified_total':timing[-1]['verified']}
    assert {w['policy'] for w in workloads}=={0,1,2}
    result['workloads']=workloads
    path('_analysis.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(workloads,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('prefix',type=Path)
    report(parser.parse_args().prefix)
