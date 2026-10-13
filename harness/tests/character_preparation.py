"""Compare two-fighter preparation with identical input and display settings."""
import argparse
import json
import statistics
import struct
from pathlib import Path
from character_plan import (analyze, symbol, SAMPLE_FIELDS, SAMPLE_MAGIC,
                            LOW_BASE, WORD, NOMINAL_NTSC_HZ)

TRANSITION_SAMPLES=4
ACTORS=2

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
                       ('cpu_ms','preparation_ms','skin_ms','frame_ms','commands')},
            'frame_p95_ms':statistics.quantiles([s['frame_ms'] for s in samples],n=100,method='inclusive')[94]})
    assert {w['policy'] for w in workloads}=={0,1,2}
    result['workloads']=workloads
    path('_analysis.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(workloads,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('prefix',type=Path)
    report(parser.parse_args().prefix)
