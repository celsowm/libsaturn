#!/usr/bin/env python3
"""Original articulated, textured humanoids for the skeletal example family.

No third-party geometry or motion. Uses the canonical SourceModel and skeletal
compiler. Rings cross elbow/knee joints with two anchors, not rigid block limbs.
"""
from __future__ import annotations
import argparse
import copy
import json
import math
from pathlib import Path
from model_pipeline.model import SourceModel, SourceNode, SourceSkin, AnimationClip, AnimationChannel
from model_pipeline import skeletal_export as sx
from model_pipeline.static_import import ImportResult
from model_pipeline.skeletal_binary import encode, compression_report

SAMPLE_RATE = 20
RING_SEGMENTS = {'high':8,'medium':6,'low':3}
CLIP_NAMES = ('idle','walk','run','jump','punch','kick','guard','hit','knockdown')
CLIP_SECONDS = (1.2,1.0,0.6,0.8,0.5,0.7,1.0,0.4,1.0)
LOOP_CLIPS = {'idle','walk','run','guard'}
TEXTURE_SIDE = 16
FACE_TEXTURE_SIDE = 32
BAKED_LIGHT_LEVELS = (3, 5, 7)
MATERIAL_COUNT = 8
PALETTE_LEVELS = 8
FACE_EYE_ROW = 11
FACE_MOUTH_ROW = 24
JOINTS = [
 ('root',-1,(0,0,0)),('hips',0,(0,1.02,0)),('spine',1,(0,1.22,0)),
 ('chest',2,(0,1.46,0)),('neck',3,(0,1.68,0)),('head',4,(0,1.77,0)),
 ('left_shoulder',3,(-.23,1.52,0)),('left_arm',6,(-.29,1.49,0)),
 ('left_elbow',7,(-.36,1.23,.015)),('left_wrist',8,(-.37,.99,.07)),
 ('left_hand',9,(-.37,.94,.09)),
 ('right_shoulder',3,(.23,1.52,0)),('right_arm',11,(.29,1.49,0)),
 ('right_elbow',12,(.36,1.23,.015)),('right_wrist',13,(.37,.99,.07)),
 ('right_hand',14,(.37,.94,.09)),
 ('left_thigh',1,(-.13,1.00,0)),('left_knee',16,(-.16,.56,.01)),
 ('left_ankle',17,(-.17,.13,0)),('left_foot',18,(-.17,.055,.16)),
 ('right_thigh',1,(.13,1.00,0)),('right_knee',20,(.16,.56,.01)),
 ('right_ankle',21,(.17,.13,0)),('right_foot',22,(.17,.055,.16))]
BY_NAME={name:i for i,(name,_,_) in enumerate(JOINTS)}

def rotation(x=0,y=0,z=0):
    # Host-authored XYZ Euler composition only; target gets Q15 quaternions.
    def mul(a,b):
        ax,ay,az,aw=a; bx,by,bz,bw=b
        return (aw*bx+ax*bw+ay*bz-az*by,aw*by-ax*bz+ay*bw+az*bx,
                aw*bz+ax*by-ay*bx+az*bw,aw*bw-ax*bx-ay*by-az*bz)
    out=(0,0,0,1)
    for axis,degrees in enumerate((x,y,z)):
        q=[0,0,0,math.cos(math.radians(degrees)/2)]
        q[axis]=math.sin(math.radians(degrees)/2); out=mul(out,q)
    return out

def build_source(detail):
    model=SourceModel(); model.skin_index=0
    for name,parent,position in JOINTS:
        offset=position if parent<0 else tuple(a-b for a,b in zip(position,JOINTS[parent][2]))
        model.nodes.append(SourceNode(name,translation=offset))
    for i,(_,parent,_) in enumerate(JOINTS):
        if parent>=0:model.nodes[parent].children.append(i)
    matrices=[]
    for _,_,p in JOINTS:
        m=[1.,0,0,0,0,1.,0,0,0,0,1.,0,-p[0],-p[1],-p[2],1.]; matrices.append(m)
    model.skins=[SourceSkin(list(range(len(JOINTS))),matrices)]
    quads=[]; face_materials=[]
    segments=RING_SEGMENTS[detail]
    def vertex(p,weights):
        js=[j for j,w in weights]; ws=[w for j,w in weights]
        js+= [0]*(4-len(js)); ws += [0.]*(4-len(ws))
        model.vertices.append(tuple(p)); model.joints.append(tuple(js)); model.weights.append(tuple(ws))
        model.uvs.append((0.,0.)); return len(model.vertices)-1
    def face(q,material):
        quads.append(tuple(q)); face_materials.append(material)
        a,b,c,d=q; model.triangles.append((a,d,b)); model.tri_materials.append(material)
        if c!=d:model.triangles.append((b,d,c)); model.tri_materials.append(material)
    def loft(rings,axis,material,cap=True):
        ids=[]
        # cross-section axes: X/Z for vertical torso/legs, Y/Z for arms.
        for center,rx,rz,weights in rings:
            ring=[]
            for k in range(segments):
                angle=2*math.pi*k/segments
                p=list(center)
                p[0 if axis=='y' else 1]+=rx*math.cos(angle)
                p[2]+=rz*math.sin(angle)
                ring.append(vertex(p,weights))
            ids.append(ring)
        for row in range(len(ids)-1):
            for k in range(segments):
                q=(ids[row][k],ids[row][(k+1)%segments],ids[row+1][(k+1)%segments],ids[row+1][k])
                # Verify outward orientation independently from winding.
                a,b,_,d=(model.vertices[i] for i in q)
                u=[d[i]-a[i] for i in range(3)]; v=[b[i]-a[i] for i in range(3)]
                n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                mid=[sum(model.vertices[i][j] for i in q)/4 for j in range(3)]
                center=[(rings[row][0][j]+rings[row+1][0][j])/2 for j in range(3)]
                if sum(n[j]*(mid[j]-center[j]) for j in range(3))<0:q=(q[0],q[3],q[2],q[1])
                mat=material
                if material==1:
                    mat=3 if row>=2 else 1
                if material==0 and axis=='y' and rings[0][0][0]==0 and row==0:
                    mat=2  # A real belt band, rather than stripes on every quad.
                if material==4 and rings[0][0][1]>.9:
                    mat=2 if mid[1]>.24 else 4
                face(q,mat)
        if cap:
            for row,direction in ((0,-1),(len(ids)-1,1)):
                center=vertex(rings[row][0],rings[row][3])
                for k in range(segments):
                    q=(center,ids[row][k],ids[row][(k+1)%segments],ids[row][(k+1)%segments])
                    # Outward cap faces follow the longitudinal direction.
                    a,b,_,d=(model.vertices[i] for i in q)
                    n=(d[0]-a[0])*(b[2]-a[2])-(d[2]-a[2])*(b[0]-a[0])
                    longitudinal=rings[-1][0][1]-rings[0][0][1] if axis=='y' else rings[-1][0][0]-rings[0][0][0]
                    if axis=='y' and n*direction*longitudinal<0:q=(q[0],q[3],q[2],q[1])
                    if axis=='x':
                        nx=(d[1]-a[1])*(b[2]-a[2])-(d[2]-a[2])*(b[1]-a[1])
                        if nx*direction*longitudinal<0:q=(q[0],q[3],q[2],q[1])
                    face(q,material if material!=1 else 3)
    loft([((0,1.02,0),.20,.13,[(1,1)]),((0,1.08,0),.20,.13,[(1,.6),(2,.4)]),
          ((0,1.27,0),.235,.15,[(2,.6),(3,.4)]),((0,1.48,0),.295,.17,[(3,1)]),
          ((0,1.59,0),.225,.13,[(3,1)])],'y',0)
    loft([((0,1.60,0),.075,.065,[(4,1)]),((0,1.76,0),.075,.065,[(4,.5),(5,.5)])],'y',1)
    loft([((0,1.73,.015),.085,.09,[(5,1)]),((0,1.80,.015),.13,.115,[(5,1)]),
          ((0,1.92,0),.135,.115,[(5,1)]),((0,1.98,-.005),.10,.095,[(5,1)])],'y',1)
    # One complete face decal. Repeating a face on each radial panel creates
    # multiple eyes and stretched features as the head turns.
    face(tuple(vertex(p,[(BY_NAME['head'],1)]) for p in
               ((-.092,1.765,.126),(-.092,1.925,.126),
                (.092,1.925,.126),(.092,1.765,.126))),6)
    for side,arm,elbow,wrist,hand,thigh,knee,ankle,foot in (
            (-1,7,8,9,10,16,17,18,19),(1,12,13,14,15,20,21,22,23)):
        p0=JOINTS[arm][2]; p1=JOINTS[elbow][2]; p2=JOINTS[wrist][2]
        def interp(a,b,t):return tuple(a[i]+(b[i]-a[i])*t for i in range(3))
        loft([(p0,.12,.115,[(arm,1)]),(interp(p0,p1,.75),.10,.095,[(arm,.8),(elbow,.2)]),
              (p1,.085,.085,[(arm,.5),(elbow,.5)]),(interp(p1,p2,.3),.10,.095,[(arm,.15),(elbow,.85)]),
              (p2,.055,.06,[(elbow,.5),(wrist,.5)])],'y',0)
        ph=JOINTS[hand][2]
        loft([(p2,.06,.065,[(hand,1)]),((ph[0],ph[1]-.045,ph[2]),.075,.08,[(hand,1)])],'y',5)
        a=JOINTS[thigh][2]; b=JOINTS[knee][2]; c=JOINTS[ankle][2]
        loft([(a,.14,.135,[(thigh,1)]),(interp(a,b,.75),.11,.11,[(thigh,.85),(knee,.15)]),
              (b,.09,.10,[(thigh,.5),(knee,.5)]),(interp(b,c,.3),.105,.105,[(thigh,.15),(knee,.85)]),
              (c,.065,.08,[(knee,.6),(ankle,.4)])],'y',4)
        pf=JOINTS[foot][2]
        loft([((pf[0],.02,.10),.09,.17,[(foot,1)]),((pf[0],.12,.08),.085,.15,[(foot,1)])],'y',4)
    # Original, deterministic motion poses. Channels remain independent per bone.
    for name,seconds in zip(CLIP_NAMES,CLIP_SECONDS):
        frames=max(2,round(seconds*SAMPLE_RATE)); times=[i/SAMPLE_RATE for i in range(frames+1)]
        channels=[]
        rotations=[[] for _ in JOINTS]; translations=[]
        for t in times:
            u=t/seconds; wave=math.sin(2*math.pi*u); root=[0,0,0]
            # Relaxed boxing stance: elbows bend naturally from a downward bind
            # pose. Every attack returns to this stance instead of a T pose.
            angles={'left_arm':(-25,0,-8),'left_elbow':(-75,0,0),
                    'right_arm':(-25,0,8),'right_elbow':(-75,0,0)}
            if name in ('idle','guard'):
                angles.update(chest=(0,0,wave*1.2),head=(0,-wave*2,0))
                if name=='guard':angles.update(left_arm=(-50,0,-8),left_elbow=(-95,0,0),right_arm=(-50,0,8),right_elbow=(-95,0,0))
            elif name in ('walk','run'):
                swing=24 if name=='walk' else 42
                angles={'left_thigh':(wave*swing,0,0),'right_thigh':(-wave*swing,0,0),
                        'left_knee':(max(0,-wave)*38,0,0),'right_knee':(max(0,wave)*38,0,0),
                        'left_arm':(-wave*swing*.65,0,0),'right_arm':(wave*swing*.65,0,0),
                        'chest':(-4 if name=='run' else 0,wave*5,0)}
                root[2]=(1.1 if name=='walk' else 2.5)*t
            elif name=='jump':
                jump=math.sin(math.pi*u); root[1]=.5*jump
                angles={'left_thigh':(-30*jump,0,0),'right_thigh':(-30*jump,0,0),
                        'left_knee':(65*jump,0,0),'right_knee':(65*jump,0,0),
                        'left_arm':(-110*jump,0,0),'right_arm':(-110*jump,0,0)}
            elif name=='punch':
                reach=math.sin(math.pi*u)**2
                angles.update(right_arm=(-25-65*reach,0,8-8*reach),
                              right_elbow=(-75*(1-reach),0,0),chest=(0,-22*reach,0))
            elif name=='kick':
                reach=math.sin(math.pi*u)**2
                angles.update(right_thigh=(-100*reach,0,0),right_knee=(30*reach,0,0),
                              chest=(15*reach,0,0))
            elif name=='hit':
                reach=math.sin(math.pi*u)
                angles.update(chest=(25*reach,0,0),head=(15*reach,0,0))
            elif name=='knockdown':
                root[1]=-.75*min(1,u*2)
                angles={'hips':(min(85,u*170),0,0),'left_knee':(45,0,0),'right_knee':(45,0,0)}
            for j,(joint,_,_) in enumerate(JOINTS):rotations[j].append(rotation(*angles.get(joint,(0,0,0))))
            translations.append(tuple(root))
        for j,values in enumerate(rotations):
            if any(v!=(0,0,0,1) for v in values):channels.append(AnimationChannel(j,'rotation',times,values))
        channels.append(AnimationChannel(0,'translation',times,translations))
        model.clips.append(AnimationClip(name,seconds,channels))
    return model,quads,face_materials

def palette(red=False):
    bases=[(35,112,184) if not red else (180,45,47),(223,167,122),
           (32,39,51),(20,23,31),(58,66,82),(105,124,150),(223,167,122),(88,205,213)]
    colors=[0]
    for color in bases:
        for level in range(PALETTE_LEVELS):
            rgb=[round(c*(.52+level*.075)) for c in color]
            colors.append(0x8000|(min(31,rgb[0]//8))|(min(31,rgb[1]//8)<<5)|(min(31,rgb[2]//8)<<10))
    return colors+[0]*(256-len(colors))

def render_asset(model,exp,quads,materials,red=False):
    textures=[]
    for material in range(MATERIAL_COUNT):
        side=FACE_TEXTURE_SIDE if material==6 else TEXTURE_SIDE
        for light in BAKED_LIGHT_LEVELS:
            pixels=[]
            for y in range(side):
                for x in range(side):
                    code=1+material*PALETTE_LEVELS+light
                    if material==6:
                        if y<4:code=1+3*PALETTE_LEVELS+2
                        if y in (FACE_EYE_ROW-2,FACE_EYE_ROW-1) and (5<=x<=12 or 19<=x<=26):code=1+3*PALETTE_LEVELS+1
                        if y in (FACE_EYE_ROW,FACE_EYE_ROW+1) and (8<=x<=11 or 20<=x<=23):code=1+3*PALETTE_LEVELS
                        if 15<=x<=17 and 15<=y<=20:code=1+PALETTE_LEVELS+max(0,light-2)
                        if y==FACE_MOUTH_ROW and 11<=x<=21:code=1+PALETTE_LEVELS+max(0,light-3)
                    pixels.append(code)
            textures.append(dict(width=side,height=side,pixels=bytes(pixels),
                                 flags=1,pixel_count=side**2))
    selectors=[]
    for q,material in zip(quads,materials):
        a,b,_,d=(model.vertices[i] for i in q)
        u=[d[i]-a[i] for i in range(3)];v=[b[i]-a[i] for i in range(3)]
        n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        length=math.sqrt(sum(c*c for c in n))
        light=sum(c*l for c,l in zip(n,(-.4,.6,.7)))/length if length else 0
        shade=2 if light>.35 else (0 if light<-.25 else 1)
        selectors.append(material*len(BAKED_LIGHT_LEVELS)+shade)
    verts=[tuple(round(c*65536) for c in model.vertices[i]) for i in exp.skin.source_vertex]
    bounds=[[round(extreme(p[a] for p in model.vertices)*65536) for a in range(3)] for extreme in (min,max)]
    return ImportResult(verts,[tuple(exp.skin.source_to_vertex[i] for i in q) for q in quads],
                        selectors,textures,palette(red),1,{'source_bounds_fx':bounds})

def generate(destination):
    destination.mkdir(parents=True,exist_ok=True); disc=destination/'disc'; disc.mkdir(exist_ok=True)
    reports={}; headers=[]; sources=[]; high_exp=high_render=None
    for detail in RING_SEGMENTS:
        model,quads,mats=build_source(detail)
        exp=sx.export_model(model,rate=SAMPLE_RATE,loop_clips=LOOP_CLIPS)
        render=render_asset(model,exp,quads,mats)
        name='fighter_'+detail
        emit_exp=sx.SkeletalExport(exp.skeleton,exp.skin,[],exp.scale)
        header,source=sx.emit_c(emit_exp,name,render)
        (destination/(name+'.h')).write_text(header); (destination/(name+'.c')).write_text(source)
        headers.append(name+'.h'); sources.append(source)
        events={n:[(round(SAMPLE_RATE*d*.45),1)] for n,d in zip(CLIP_NAMES,CLIP_SECONDS) if n in ('punch','kick')}
        roots={n:2|8 for n in ('walk','run')}
        for red in (False,True):
            variant=copy.copy(render); variant.palette_rgb555=palette(red)
            payload=encode(exp,variant,[j[0] for j in JOINTS],events,roots)
            (disc/((detail+'_red' if red else detail+'_blue')+'.ska')).write_bytes(payload)
            if detail=='high' and not red:
                high_exp,high_render=exp,render; archive=payload
        reports[detail]={'vertices':len(exp.skin.source_vertex),'faces':len(quads),
                         'joints':len(JOINTS),'blend_vertices':len(exp.skin.blend),
                         'archive_bytes':len(payload),'runtime':sx.size_report(exp),
                         'verification':sx.verify(model,exp),'compression':compression_report(exp)}
    header=['#pragma once','#include "saturn/character3d.h"']+['#include "'+h+'"' for h in headers]
    header += [f'#define FIGHTER_JOINT_COUNT {len(JOINTS)}u',f'#define FIGHTER_CLIP_COUNT {len(CLIP_NAMES)}u',
               f'#define FIGHTER_TEXTURE_COUNT {len(high_render.textures)}u',f'#define FIGHTER_ARCHIVE_BYTES {len(archive)}u',
               'extern const uint8_t fighter_archive[FIGHTER_ARCHIVE_BYTES];',
               'extern const uint16_t fighter_red_palette[256];']
    header += [f'#define FIGHTER_CLIP_{n.upper()} {i}u' for i,n in enumerate(CLIP_NAMES)]
    source=['#include "fighter_assets.h"',f'const uint8_t fighter_archive[{len(archive)}] = {{']
    source += ['    '+','.join(str(b) for b in archive[i:i+32])+',' for i in range(0,len(archive),32)]
    source += ['};','const uint16_t fighter_red_palette[256] = {'+','.join(map(str,palette(True)))+'};']
    (destination/'fighter_assets.h').write_text('\n'.join(header)+'\n')
    (destination/'fighter_assets.c').write_text('\n'.join(source)+'\n')
    (destination/'report.json').write_text(json.dumps(reports,indent=2))
    print(json.dumps({d:{k:v for k,v in r.items() if k not in ('runtime','compression')} for d,r in reports.items()},indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--output',type=Path,required=True)
    generate(parser.parse_args().output)
