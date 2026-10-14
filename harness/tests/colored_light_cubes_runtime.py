"""Validate the small scene's actual shading, additive mode and pause behavior."""
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
    frames,paused,lights,pool,phase,errors=words('g_light_cubes',6)
    assert frames>300 and paused==0 and lights==pool==1 and errors==0
    regs=path('_vdp2.bin').read_bytes()
    assert struct.unpack_from('>H',regs,0xec)[0]&0x140==0x140
    data=path('_vdp1.bin').read_bytes();plain=[];shaded=[];add=[]
    for offset in range(0,len(data),32):
        command=struct.unpack_from('>16H',data,offset)
        if command[0]&0x8000:break
        if command[0]&15!=2:continue
        assert not command[2]&0x100, 'mesh coverage found'
        mode=command[2]&63
        if mode==8:plain.append(command)
        elif mode==12:shaded.append(command)
        elif mode==32:add.append(command)
    assert len(plain)==len(shaded)==3 and len(add)==1, (len(plain),len(shaded),len(add))
    assert plain[0][3:6]==shaded[0][3:6], 'cube textures are not shared'
    assert add[0][3]==0x4500 and add[0][14]==0
    assert min(command[14] for command in shaded)>=65536//8
    a=Image.open(path('_paused_a.png')).convert('RGB')
    b=Image.open(path('_paused_b.png')).convert('RGB')
    assert ImageChops.difference(a,b).getbbox() is None, 'paused scene changes'
    assert len(a.crop((0,160,80,192)).getcolors())==2, 'VDP2 checker is missing'
    result={'frames':frames,'errors':errors,'phase':phase,'plain_faces':len(plain),
            'gouraud_faces':len(shaded),'additive_pools':len(add),'pause_images_equal':True}
    path('_analysis.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prefix',type=Path)
    analyze(parser.parse_args().prefix)
