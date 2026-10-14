"""SKA1: relocatable, big-endian, random-access character archives.

Wire layouts match include/saturn/character_asset_format.h. Pose records use
the native fixed-width translation/quaternion fields, never pointer layouts.
"""
from __future__ import annotations
import struct
import time

MAGIC = 0x534B4131
VERSION = 1
HEADER_WORDS = 24
HEADER_BYTES = HEADER_WORDS * struct.calcsize('>I')
JOINT = struct.Struct('>HH3i4h3i')
RIGID = struct.Struct('>3hBB')
BLEND = struct.Struct('>4B2H9h')
POSE = struct.Struct('>3i4h')
TEXTURE = struct.Struct('>IHHHHI')
CLIP = struct.Struct('>III6H')

def encode(exp, render, joint_names=None, events=None, root_flags=None):
    if render.luts_rgb555:
        raise ValueError("SKA1 does not store RGB CLUTs; use generated C for LUT4 skeletal assets")
    blob = bytearray(HEADER_BYTES)
    def add(payload, alignment=4):
        blob.extend(bytes((-len(blob)) % alignment))
        offset = len(blob)
        blob.extend(payload)
        return offset
    skel, skin = exp.skeleton, exp.skin
    joint_names = joint_names or [f'joint_{j}' for j in range(len(skel.parent))]
    offsets = []
    offsets.append(add(b''.join(JOINT.pack(p,0,*t,*q,*s) for p,t,q,s in zip(
        skel.parent,skel.translation,skel.rotation,skel.scale))))
    offsets.append(add(b''.join(RIGID.pack(*p,j,0) for p,j in skin.rigid)))
    offsets.append(add(b''.join(BLEND.pack(*js,n,*ws,*[v for p in ps for v in p])
                               for js,n,ws,ps in skin.blend)))
    offsets.append(add(b''.join(struct.pack('>3i',*p) for p in render.vertices_fx)))
    offsets.append(add(b''.join(struct.pack('>4H',*q) for q in render.indices_abcd)))
    offsets.append(add(struct.pack('>'+str(len(render.face_texture_indices))+'H',
                                    *render.face_texture_indices)))
    texture_meta = add(bytes(TEXTURE.size*len(render.textures)))
    offsets.append(texture_meta)
    offsets.append(add(struct.pack('>256H',*render.palette_rgb555)))
    clip_meta = add(bytes(CLIP.size*len(exp.clips)))
    offsets.append(clip_meta)
    name_meta = add(bytes(struct.calcsize('>I')*len(skel.parent)))
    offsets.append(name_meta)
    bounds = render.stats['source_bounds_fx']
    offsets.append(add(struct.pack('>6i',*bounds[0],*bounds[1])))
    for i,name in enumerate(joint_names):
        off=add(name.encode('utf8')+b'\0',1)
        struct.pack_into('>I',blob,name_meta+i*struct.calcsize('>I'),off)
    for i,tex in enumerate(render.textures):
        off=add(bytes(tex['pixels']),8)
        TEXTURE.pack_into(blob,texture_meta+i*TEXTURE.size,off,tex['width'],tex['height'],
                          tex.get('flags',1),render.palette_base,tex['pixel_count'])
    for i,clip in enumerate(exp.clips):
        off=add(b''.join(POSE.pack(*t,*q) for row in clip.frames for t,q in row))
        ev=(events or {}).get(clip.name,[])
        ev_offset=add(b''.join(struct.pack('>HH',*e) for e in ev)) if ev else 0
        name_offset=add(clip.name.encode('utf8')+b'\0',1)
        flags=(1 if clip.loop else 0)|(root_flags or {}).get(clip.name,0)
        CLIP.pack_into(blob,clip_meta+i*CLIP.size,off,ev_offset,name_offset,
                       clip.frame_count,len(skel.parent),len(ev),clip.rate,1,flags)
    values=[MAGIC,VERSION,len(blob),len(skel.parent),len(skin.rigid),len(skin.blend),
            len(skin.source_vertex),len(render.indices_abcd),len(render.textures),1,
            len(exp.clips),skin.pos_shift,skin.max_joint,*offsets]
    struct.pack_into('>'+str(HEADER_WORDS)+'I',blob,0,*values)
    return bytes(blob)

def compression_report(exp):
    """Measure independent-frame alternatives without requiring sequential decode."""
    raw=constant=quantized=0
    raw_us=compact_us=0.0
    benchmark_samples=64
    error=0.0
    for clip in exp.clips:
        joints=len(clip.frames[0])
        raw+=clip.frame_count*joints*POSE.size
        compact_data=bytearray();directory=[]
        raw_data=b''.join(POSE.pack(*t,*q) for row in clip.frames for t,q in row)
        for j in range(joints):
            channel=[row[j] for row in clip.frames]
            fixed=all(p==channel[0] for p in channel)
            directory.append((len(compact_data),fixed))
            compact_data.extend(b''.join(POSE.pack(*t,*q) for t,q in (channel[:1] if fixed else channel)))
            constant+=POSE.size if fixed else len(channel)*POSE.size
            translations=[t for t,_ in channel]
            maximum=max(abs(v) for t in translations for v in t)
            shift=0
            while maximum>>shift>32767: shift+=1
            quantized+=len(channel)*(3*struct.calcsize('>h')+4*struct.calcsize('>h'))
            for t in translations:
                error=max(error,max(abs(v-round(v/(1<<shift))*(1<<shift))/65536 for v in t))
        constant+=len(directory)*struct.calcsize('>I')  # indexed channel directory
        sample_frames=[(i*17)%clip.frame_count for i in range(benchmark_samples)]
        started=time.perf_counter()
        decoded=[tuple(POSE.unpack_from(raw_data,(f*joints+j)*POSE.size) for j in range(joints)) for f in sample_frames]
        raw_us+=(time.perf_counter()-started)*1e6
        started=time.perf_counter()
        compact=[tuple(POSE.unpack_from(compact_data,off+(0 if fixed else f)*POSE.size)
                 for off,fixed in directory) for f in sample_frames]
        compact_us+=(time.perf_counter()-started)*1e6
        assert decoded==compact
    return dict(native_pose_bytes=raw,constant_channel_bytes=constant,
                translation_q16_bytes=quantized,max_translation_error=error,
                constant_channel_integer_pose_error=0,
                host_native_decode_us_per_pose=raw_us/(benchmark_samples*len(exp.clips)),
                host_constant_decode_us_per_pose=compact_us/(benchmark_samples*len(exp.clips)),
                selected='native random-access poses',
                reason='No per-frame decompression; each resident clip supports arbitrary time sampling.')
