#!/usr/bin/env python3
"""Compile separate cage geometry, RGB555 crowd cells and indexed RBG0 mat cells."""
import argparse
import json
from pathlib import Path
from PIL import Image
from model_pipeline.gltf import parse_glb, extract_image_bytes
from model_pipeline.animated_import import import_animated_model
from model_pipeline.static_import import emit_c_h
from satlz import compress, decompress
from saturn_asset_common import format_byte_array, format_word_array
import io

CELL_SIZE=8
CROWD_SIZE=(256,256)
MAT_SIZE=256
MAT_BANK=4
MAT_CHAR_BASE=0x40000


def cells(data,width,height,stride):
    output=bytearray()
    for y in range(0,height,CELL_SIZE):
        for x in range(0,width,CELL_SIZE):
            for row in range(CELL_SIZE):
                start=((y+row)*width+x)*stride
                output.extend(data[start:start+CELL_SIZE*stride])
    return bytes(output)


def compile_environment(stage,crowd,prefix):
    result=import_animated_model(stage,animation='rest',animation_fps='1',
        merge_rigid_meshes=True,max_triangles=200,quality='aggressive',texel_extent=16,
        weld_vertices=True)
    asset=result.static
    # Remove all floor/base faces. RBG0 owns this plane; VDP1 owns only the cage.
    floor=1.1098922491073608
    asset.indices_abcd=[face for face in asset.indices_abcd
        if max(asset.vertices_fx[index][2]/65536 for index in face)>floor+0.05]
    asset.face_texture_indices=[65535]*len(asset.indices_abcd)
    asset.textures=[];asset.palette_rgb555=[]
    emit_c_h(asset,prefix.parent/'octagon','octagon')
    glb=parse_glb(stage)
    payload,_=extract_image_bytes(glb,0)
    mat=Image.open(io.BytesIO(payload)).convert('RGB').resize((MAT_SIZE,MAT_SIZE),Image.Resampling.LANCZOS)
    quantized=mat.quantize(colors=254,method=Image.Quantize.MEDIANCUT)
    pixels=bytearray(value+1 for value in quantized.tobytes())
    for y in range(MAT_SIZE):
        for x in range(MAT_SIZE):
            if abs(x-MAT_SIZE/2)+abs(y-MAT_SIZE/2)>MAT_SIZE*0.7:
                pixels[y*MAT_SIZE+x]=255
    palette=[0]
    colors=quantized.getpalette()
    for i in range(254):
        r,g,b=colors[i*3:i*3+3]
        palette.append(0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10))
    palette.append(0x8000|2|(3<<5)|(5<<10))
    mat_cells=bytes([255])*(CELL_SIZE*CELL_SIZE)+cells(pixels,MAT_SIZE,MAT_SIZE,1)
    crowd_image=Image.open(crowd).convert('RGB').resize(CROWD_SIZE,Image.Resampling.LANCZOS)
    rgb=crowd_image.tobytes();rgb_words=bytearray()
    for offset in range(0,len(rgb),3):
        r,g,b=rgb[offset:offset+3]
        rgb_words.extend((0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10)).to_bytes(2,'big'))
    crowd_cells=cells(rgb_words,*CROWD_SIZE,2)
    packed_crowd=compress(crowd_cells);packed_mat=compress(mat_cells)
    assert decompress(packed_mat)==mat_cells and decompress(packed_crowd)==crowd_cells
    prefix.parent.mkdir(parents=True,exist_ok=True)
    prefix.with_suffix('.h').write_text(
        '#pragma once\n#include <stdint.h>\n'
        f'#define CROWD_BYTES {len(crowd_cells)}u\n#define CROWD_PACKED_BYTES {len(packed_crowd)}u\n'
        f'#define MAT_BYTES {len(mat_cells)}u\n#define MAT_PACKED_BYTES {len(packed_mat)}u\n'
        f'extern const uint8_t crowd_packed[{len(packed_crowd)}];\n'
        f'extern const uint8_t mat_packed[{len(packed_mat)}];\nextern const uint16_t mat_palette[256];\n')
    prefix.with_suffix('.c').write_text(f'#include "{prefix.name}.h"\n'
        f'const uint8_t crowd_packed[{len(packed_crowd)}]={{\n'+format_byte_array(list(packed_crowd))+'\n};\n'
        f'const uint8_t mat_packed[{len(packed_mat)}]={{\n'+format_byte_array(list(packed_mat))+'\n};\n'
        'const uint16_t mat_palette[256]={\n'+format_word_array(palette)+'\n};\n')
    report={'cage_faces':len(asset.indices_abcd),'crowd_bytes':len(crowd_cells),
            'crowd_packed_bytes':len(packed_crowd),'mat_bytes':len(mat_cells),'mat_packed_bytes':len(packed_mat)}
    prefix.with_suffix('.report.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--stage',type=Path,required=True);p.add_argument('--crowd',type=Path,required=True)
    p.add_argument('--out-prefix',type=Path,required=True)
    args=p.parse_args();compile_environment(args.stage,args.crowd,args.out_prefix)
