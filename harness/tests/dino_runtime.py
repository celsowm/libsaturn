"""Validate the dino runtime and parallel pipeline from completed-frame diagnostics."""
import argparse
import json
import struct
from pathlib import Path
from character_plan import symbol, HIGH_BASE

DIAGNOSTIC_MAGIC=0x44494E31
FIELDS=('magic frames pose_submitted pose_completed pose_fallbacks geometry_submitted '
        'geometry_merged draw_overflow music_error slave_tasks master_tasks failed_tasks').split()

def analyze(prefix):
    path=lambda suffix:Path(str(prefix)+suffix)
    mapping=path('.map').read_text();high=path('_high.bin').read_bytes()
    panic=struct.unpack_from('>3I',high,symbol(mapping,'g_sat_last_panic')-HIGH_BASE)
    assert not any(panic),f'guest panic: {panic}'
    metrics=dict(zip(FIELDS,struct.unpack_from('>'+str(len(FIELDS))+'I',high,
                 symbol(mapping,'g_dino_diagnostics')-HIGH_BASE)))
    assert metrics['magic']==DIAGNOSTIC_MAGIC and metrics['frames']>50,metrics
    assert metrics['pose_completed']>10 and metrics['geometry_merged']>10,metrics
    assert 0<=metrics['pose_submitted']-metrics['pose_completed']<=1,metrics
    assert 0<=metrics['geometry_submitted']-metrics['geometry_merged']<=1,metrics
    assert metrics['slave_tasks']>0 and metrics['failed_tasks']==0,metrics
    assert metrics['draw_overflow']==0 and metrics['music_error']==0,metrics
    probe=json.loads(path('.json').read_text())
    assert probe['ram_cartridge']['type']=='none'
    result={'metrics':metrics,'panic':list(panic)}
    path('_analysis.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('prefix',type=Path)
    analyze(parser.parse_args().prefix)
