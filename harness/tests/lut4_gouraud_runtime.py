"""Check actual VDP1 LUT4/Gouraud commands and the shared immutable texture."""
import argparse
import json
import struct
from pathlib import Path
from PIL import Image, ImageChops
from character_plan import symbol, HIGH_BASE

def analyze(prefix):
    path=lambda suffix:Path(str(prefix)+suffix)
    mapping=path('.map').read_text()
    high=path('_high.bin').read_bytes()
    def words(name,count):
        return struct.unpack_from('>'+str(count)+'I',high,symbol(mapping,name)-HIGH_BASE)
    assert not any(words('g_sat_last_panic',3))
    frames,draws,errors,paused=words('g_lut4_metrics',4)
    assert frames>200 and frames<=draws<=frames+1 and errors==0 and paused==0
    data=path('_vdp1.bin').read_bytes()
    sprites=[]
    for offset in range(0,len(data),32):
        command=struct.unpack_from('>16H',data,offset)
        if command[0]&0x8000:break
        if command[0]&15==2:sprites.append(command)
    assert len(sprites)==2
    plain,shaded=sprites
    assert plain[2]&63==8 and plain[14]==0
    assert shaded[2]&63==12 and shaded[14]==65536//8
    assert plain[3:6]==shaded[3:6], 'CLUT, texture or dimensions differ'
    table=struct.unpack('>4H',path('_table.bin').read_bytes())
    assert len(set(table))>1, table
    a=Image.open(path('_paused_a.png')).convert('RGB')
    b=Image.open(path('_paused_b.png')).convert('RGB')
    assert ImageChops.difference(a,b).getbbox() is None
    final=Image.open(path('_final.png')).convert('RGB')
    original=final.crop((28,60,148,180))
    lit=final.crop((172,60,292,180))
    assert ImageChops.difference(original,lit).getbbox() is not None
    result={'frames':frames,'errors':errors,'shared_srca':plain[4],
            'shared_clut':plain[3],'gouraud_address_div8':shaded[14],
            'gouraud_corners':list(table),'pause_images_equal':True}
    path('_analysis.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix',type=Path)
    analyze(parser.parse_args().prefix)
