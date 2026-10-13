"""Read named character diagnostics and the bounded stress benchmark log."""
import argparse
import json
import re
import statistics
import struct
from pathlib import Path

FIELDS=('magic frames simulation_ticks steps_last loop_rate_x10 simulation_rate_x10 '
        'requested accepted downgraded rejected policy quality clip animation_frame '
        'joints vertices visible_faces commands command_capacity skeleton_ms skin_ms '
        'geometry_ms sort_us emit_us frame_ms max_frame_ms persistent_bytes texture_bytes '
        'slave_tasks master_tasks wait_ticks queue_ticks player_health opponent_health '
        'hits blocked knockdowns round_over events root_travel skin_equivalence '
        'source_reloads errors combat_hits_total combat_blocks_total').split()
SAMPLE_FIELDS=('iteration vblank simulation_ticks requested accepted lod policy frame_ms '
               'cpu_ms skin_ms preparation_ms sort_us emission_us commands slave_tasks '
               'master_tasks wait_ticks queue_ticks complexity').split()
GEOMETRY_MAGIC=0x43475031
GEOMETRY_FIELDS=('iteration preparation_us dispatch_us master_us wait_us merge_us '
                 'verification_us pairs verified errors').split()
GEOMETRY_METRICS=('magic pairs verified errors preparation_us dispatch_us master_us '
                  'wait_us merge_us verification_us').split()
MAGIC=0x43484152
SAMPLE_MAGIC=0x43424E31
WORD=struct.calcsize('>I')
HIGH_BASE=0x06000000
LOW_BASE=0x00200000
NOMINAL_NTSC_HZ=60

def symbol(map_text,name):
    hit=re.search(r'0x([0-9a-fA-F]+)\s+_?'+name+r'\b',map_text)
    if not hit:raise ValueError('missing map symbol: '+name)
    return int(hit[1],16)

def analyze(example,map_path,high_path,probe_path,output,low_path=None,body_collision=False):
    mapping=map_path.read_text();high=high_path.read_bytes()
    start=symbol(mapping,'g_character_demo')-HIGH_BASE
    metrics=dict(zip(FIELDS,struct.unpack_from('>'+str(len(FIELDS))+'I',high,start)))
    panic=struct.unpack_from('>3I',high,symbol(mapping,'g_sat_last_panic')-HIGH_BASE)
    probe=json.loads(probe_path.read_text())
    assert not any(panic),f'guest panic: {panic}'
    assert metrics['magic']==MAGIC and metrics['frames']>5,metrics
    assert metrics['errors']==0,metrics
    assert metrics['commands']<metrics['command_capacity'],metrics
    assert metrics['joints']==24,metrics
    assert metrics['accepted']==metrics['requested'] and not metrics['rejected'],metrics
    assert metrics['skin_equivalence']>0,metrics
    assert probe['ram_cartridge']['type']=='none'
    report={'example':example,'metrics':metrics,'panic':list(panic)}
    if 'g_character_geometry' in mapping:
        geometry=dict(zip(GEOMETRY_METRICS,struct.unpack_from('>'+str(len(GEOMETRY_METRICS))+'I',
                      high,symbol(mapping,'g_character_geometry')-HIGH_BASE)))
        assert geometry['magic']==GEOMETRY_MAGIC and geometry['errors']==0
        assert geometry['pairs']>0 and geometry['verified']>0
        report['geometry']=geometry
    if example=='skeletal_animation_3d':
        assert metrics['clip']==8, 'Viewer did not reach all nine clips'
    if example=='fighter_arena_3d':
        if not body_collision:
            assert metrics['combat_hits_total']>0 and metrics['combat_blocks_total']>0,metrics
            assert metrics['source_reloads']>=4,metrics
        else:
            bodies=struct.unpack_from('>4i2I',high,symbol(mapping,'g_character_bodies')-HIGH_BASE)
            px,pz,qx,qz,ticks,overlaps=bodies
            diameter=(65536*3//10)*2  # Matches COMBAT_BODY_DIAMETER_Q16.
            assert ticks>300 and overlaps==0,bodies
            assert px<qx and (qx-px)**2+(qz-pz)**2>=diameter**2,bodies
            report['body_collision']={'positions_q16':bodies[:4],'checked_ticks':ticks,'overlaps':overlaps}
    if low_path:
        low=low_path.read_bytes();offset=symbol(mapping,'g_character_benchmark')-LOW_BASE
        magic,count,capacity,row_bytes=struct.unpack_from('>4I',low,offset)
        assert magic==SAMPLE_MAGIC and 0<count<=capacity and row_bytes==WORD*len(SAMPLE_FIELDS)
        rows=[dict(zip(SAMPLE_FIELDS,struct.unpack_from('>'+str(len(SAMPLE_FIELDS))+'I',low,
                    offset+WORD*4+i*row_bytes))) for i in range(count)]
        groups={};segments=[];last_key=None
        for row in rows:
            key=(row['requested'],row['policy'],row['lod'],row.get('complexity',0))
            if key!=last_key:segments.append((key,[]));last_key=key
            segments[-1][1].append(row)
        for key,segment in segments:
            if len(segment)>5:groups.setdefault(key,[]).append(segment[3:])
        results=[]
        for (actors,policy,lod,complexity),segments in sorted(groups.items()):
            samples=[sample for segment in segments for sample in segment]
            # Never count other workloads across gaps between matching modes.
            fields=sum(segment[-1]['vblank']-segment[0]['vblank'] for segment in segments)
            submissions=sum(segment[-1]['iteration']-segment[0]['iteration'] for segment in segments)
            sim_steps=sum(segment[-1]['simulation_ticks']-segment[0]['simulation_ticks'] for segment in segments)
            result={'actors':actors,'policy':policy,'lod':lod,'complexity':complexity,'samples':len(samples),
                    'submitted_lists_per_second':NOMINAL_NTSC_HZ*submissions/fields,
                    'simulation_steps_per_second':NOMINAL_NTSC_HZ*sim_steps/fields,
                    'medians':{key:statistics.median(s[key] for s in samples) for key in
                               ('cpu_ms','skin_ms','preparation_ms','sort_us','emission_us','commands')},
                    'max_frame_ms':max(s['frame_ms'] for s in samples)}
            assert all(s['accepted']==actors for s in samples)
            results.append(result)
        seen={(r['actors'],r['policy']) for r in results}
        assert seen=={(actors,policy) for actors in (1,2,4,8) for policy in (0,1,2)},seen
        report['workloads']=results;report['sample_count']=count
    output.write_text(json.dumps(report,indent=2))
    print(json.dumps({k:v for k,v in report.items() if k!='rows'},indent=2))
    return report

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--example',required=True)
    for flag in ('map','high','probe','output'):parser.add_argument('--'+flag,type=Path,required=True)
    parser.add_argument('--low',type=Path)
    parser.add_argument('--body-collision',action='store_true')
    args=parser.parse_args();analyze(args.example,args.map,args.high,args.probe,args.output,args.low,args.body_collision)
