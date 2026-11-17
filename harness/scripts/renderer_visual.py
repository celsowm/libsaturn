"""Lossless visual evidence for static renderer A/B, using FFmpeg."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zlib

FRAMES=(900,1200)
MAGIC=bytes.fromhex("89504e470d0a1a0a")


def require_ffmpeg():
    if not shutil.which("ffmpeg"):
        raise RuntimeError("ffmpeg required (macOS: brew install ffmpeg)")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_rgb(path):
    header=path.read_bytes()[:33]
    if len(header)!=33 or header[:8]!=MAGIC or header[12:16]!=b"IHDR":
        raise ValueError("Not a PNG: "+str(path))
    w,h=struct.unpack_from(">II",header,16)
    if not 0<w<=2048 or not 0<h<=2048:
        raise ValueError("Unexpected screenshot dimensions")
    raw=subprocess.check_output(["ffmpeg","-v","error","-i",str(path),
                                 "-frames:v","1","-f","rawvideo","-pix_fmt","rgb24",
                                 "-threads","1","pipe:1"])
    if len(raw)!=w*h*3:raise ValueError("Incomplete decoded RGB image")
    return w,h,raw


def write_rgb(path,w,h,raw):
    if len(raw)!=w*h*3:raise ValueError("Invalid RGB length")
    def chunk(kind,data):
        return struct.pack(">I",len(data))+kind+data+struct.pack(">I",zlib.crc32(kind+data))
    scan=b"".join(b"\0"+raw[y*w*3:(y+1)*w*3] for y in range(h))
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(MAGIC+chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+
                     chunk(b"IDAT",zlib.compress(scan))+chunk(b"IEND",b""))


def crop(raw,w,h,box):
    x0,y0,x1,y1=box
    if not(0<=x0<x1<=w and 0<=y0<y1<=h):
        raise ValueError("Crop outside raster")
    return b"".join(raw[(y*w+x0)*3:(y*w+x1)*3] for y in range(y0,y1))


def diff(a,b,w,h):
    if len(a)!=len(b) or len(a)!=w*h*3:raise ValueError("RGB layout mismatch")
    mask=bytearray(len(a))
    count=0; xmin=w;ymin=h;xmax=-1;ymax=-1;maximum=0
    for p in range(w*h):
        i=p*3
        if a[i:i+3]==b[i:i+3]:continue
        delta=max(abs(a[i+j]-b[i+j]) for j in range(3))
        count+=1;maximum=max(maximum,delta)
        x,y=p%w,p//w
        xmin=min(xmin,x);xmax=max(xmax,x);ymin=min(ymin,y);ymax=max(ymax,y)
        mask[i:i+3]=bytes.fromhex("ff00ff")
    return dict(different_pixels=count,total_pixels=w*h,max_channel_delta=maximum,
                bounds=[xmin,ymin,xmax+1,ymax+1] if count else None),bytes(mask)


def model_box(w,h,emulator=None):
    # Capture requests scanlines 0..223 and no overscan/aspect correction.
    # Do not guess offsets or resize unexpected rasters.
    # Mednafen 1.32.1 CSCD capture of the Saturn 320x224 mode can
    # retain a 341-dot horizontal raster, doubled to 682x448.
    # The observed full-width model band is exactly rows 96..383.
    # Keep padding too: no horizontal crop, stretching or resampling.
    if emulator=="mednafen" and (w,h)==(682,448):
        return (0,96,w,384)
    if w%320 or h%224:return None
    sx,sy=w//320,h//224
    if sx not in (1,2,3,4) or sy not in (1,2,3,4):return None
    return (0,48*sy,w,192*sy)


def compare_pair(control,candidate,out,root,emulator=None):
    out.mkdir(parents=True,exist_ok=True)
    before,after=load_rgb(control),load_rgb(candidate)
    shutil.copyfile(control,out/"control-screen.png")
    shutil.copyfile(candidate,out/"candidate-screen.png")
    result={"control_sha256":sha(control),"candidate_sha256":sha(candidate),
            "control_dimensions":list(before[:2]),"candidate_dimensions":list(after[:2]),
            "directory":str(out.relative_to(root))}
    if before[:2]!=after[:2]:
        result.update(status="REVIEW_RASTER_MISMATCH",passed=False)
        return result
    w,h,a=before;b=after[2]
    result["screen"],mask=diff(a,b,w,h)
    write_rgb(out/"diff-screen.png",w,h,mask)
    box=model_box(w,h,emulator)
    if box is None:
        result.update(status="REVIEW_UNKNOWN_MODEL_RASTER",passed=False)
        return result
    x,y,x1,y1=box;cw,ch=x1-x,y1-y
    a,b=crop(a,w,h,box),crop(b,w,h,box)
    result["model_crop"]=list(box)
    result["model"],mask=diff(a,b,cw,ch)
    write_rgb(out/"control-crop.png",cw,ch,a)
    write_rgb(out/"candidate-crop.png",cw,ch,b)
    write_rgb(out/"diff-crop.png",cw,ch,mask)
    # A pair of blank or uniform captures cannot qualify.
    nonuniform=all(any(rgb[i:i+3]!=rgb[:3] for i in range(3,len(rgb),3)) for rgb in (a,b))
    result["nonuniform_model"]=nonuniform
    result["passed"]=nonuniform and result["model"]["different_pixels"]==0
    result["status"]="PASS" if result["passed"] else "REVIEW_MODEL"
    return result


def extract_frames(movie,out,frames=FRAMES):
    """Decode fixed zero-based recorded frame indices without resampling."""
    out.mkdir(parents=True,exist_ok=True)
    paths={}
    for frame in frames:
        target=out/f"frame-{frame}.png"
        target.unlink(missing_ok=True)
        subprocess.run(["ffmpeg","-v","error","-y","-i",str(movie),"-map","0:v:0",
                        "-vf",f"select=eq(n\\,{frame})","-frames:v","1",
                        "-c:v","png","-pix_fmt","rgb24","-threads","1",str(target)],
                       check=True,timeout=120)
        if not target.is_file():raise ValueError(f"Recording ended before frame {frame}")
        load_rgb(target)
        paths[str(frame)]={"file":target.name,"sha256":sha(target)}
    return paths


def build_manifest(run,cases,capture_errors=None):
    capture_errors=capture_errors or {}
    def pair(control,candidate,out,emulator=None):
        try:
            return compare_pair(control,candidate,out,run,emulator)
        except Exception as exc:
            return dict(passed=False,status="REVIEW_CAPTURE_ERROR",error=str(exc))
    manifest={"version":2,"scope":"Static scenes; compare within each emulator. Full screen includes the changing frame-counter HUD. Magenta marks RGB differences; black means identical.",
              "case_results":{},"passed":False}
    for name in cases:
        dest=run/"visual"/name
        a,b=(run/v/name for v in ("control","candidate"))
        entry={"ymir":pair(a/"screen.png",b/"screen.png",dest/"ymir"),"mednafen":{},
               "capture_errors":capture_errors.get(name,{})}
        commands=[json.loads((p/"analysis.json").read_text())["execution"]["probe"] for p in (a,b)]
        fields=[int(cmd[cmd.index("--screenshot")+1].split(":",1)[0]) for cmd in commands]
        entry["ymir"]["capture_fields"]=fields
        if fields[0]!=fields[1]:raise ValueError("Ymir screenshot fields differ")
        for frame in FRAMES:
            key=f"frame-{frame}"
            entry["mednafen"][key]=pair(a/"mednafen-visual"/(key+".png"),
                       b/"mednafen-visual"/(key+".png"),dest/"mednafen"/key,"mednafen")
            entry["mednafen"][key]["recorded_frame_index"]=frame
        entry["mednafen_stability"]={}
        for v,p in (("control",a),("candidate",b)):
            entry["mednafen_stability"][v]=pair(
                p/"mednafen-visual"/f"frame-{FRAMES[0]}.png",
                p/"mednafen-visual"/f"frame-{FRAMES[1]}.png",
                dest/"mednafen"/("stability-"+v),"mednafen")
        entry["passed"]=not entry["capture_errors"] and all(r["passed"] for r in [entry["ymir"],*entry["mednafen"].values(),
                                                  *entry["mednafen_stability"].values()])
        manifest["case_results"][name]=entry
        (run/"visual-validation.json").write_text(json.dumps(manifest,indent=2)+"\n")
    manifest["passed"]=all(v["passed"] for v in manifest["case_results"].values())
    (run/"visual-validation.json").write_text(json.dumps(manifest,indent=2)+"\n")
    return manifest
