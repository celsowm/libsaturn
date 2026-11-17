#!/usr/bin/env python3
"""Inventory actual exported skin records, without guessing from GLB weights."""
import argparse,collections,hashlib,json,re,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
ASSET=re.compile(r"const\s+sat_skin_asset_t\s+(\w+)\s*=\s*\{([^{}]+)\};")
ARRAY=re.compile(r"static\s+const\s+sat_skin_blend_t\s+(\w+)\[(\d+)\]\s*=\s*\{(.*?)\n\s*\};",re.S)
ROW=re.compile(r"\{\s*\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}\s*,\s*(\d+)\s*,\s*\{\s*(\d+)\s*,\s*(\d+)\s*\}\s*,")
def inspect(path,origin):
    raw=path.read_bytes();source=raw.decode("utf-8")
    arrays={a:(int(n),body) for a,n,body in ARRAY.findall(source)}
    reports=[]
    for symbol,fields in ASSET.findall(source):
        parts=[p.strip() for p in fields.split(",")]
        if len(parts)!=8:raise ValueError("Unknown asset initializer: "+symbol)
        rigid,blend,joints,shift,highest=map(int,parts[3:])
        rows=[]
        if blend:
            length,body=arrays[parts[1]]
            rows=[tuple(map(int,x)) for x in ROW.findall(body)]
            if len(rows)!=blend or length!=blend:raise ValueError("Incomplete blend records: "+symbol)
        counts=collections.Counter(rigid=rigid,single=0,half=0,uneven_pair=0,triple=0)
        runs=collections.Counter();last=None;longest=0;run=0;endpoints=0;previous=None;same_joint=0
        for a,b,c,n,w,v in rows:
            if n not in (1,2,3) or w>32768 or (n==3 and w+v>32768):
                raise ValueError("Invalid weights: "+symbol)
            kind="single" if n==1 else "triple" if n==3 else "half" if w==16384 else "uneven_pair"
            counts[kind]+=1
            if kind=="uneven_pair" and w in (0,32768):endpoints+=1
            if kind!=last:
                runs[kind]+=1;run=0
            run+=1;last=kind
            if kind=="uneven_pair":longest=max(longest,run)
            key=(a,b,c)[:n]
            if previous==key:same_joint+=1
            previous=key
        total=rigid+blend
        reports.append(dict(asset=symbol,source=origin,sha256=hashlib.sha256(raw).hexdigest(),
            vertices=total,joints=joints,pos_shift=shift,max_joint=highest,counts=dict(counts),
            unequal_pair_percent=100*counts["uneven_pair"]/total if total else 0,
            unequal_pair_endpoints=endpoints,blend_runs=dict(runs),longest_uneven_pair_run=longest,
            adjacent_same_joint_tuple=same_joint))
    return reports

def collect(root=ROOT):
    root=Path(root);report={"assets":[],"errors":[]}
    candidates=[(root/"examples/skeletal_animation_3d/gen/robot_police_skeletal.c","tracked robot snapshot"),
                (root/"build/generated/skeletal_animation_3d/robot_police_skeletal.c","existing local generated robot (may be stale)")]
    for p,label in candidates:
        if p.is_file():
            try:report["assets"].extend(inspect(p,label))
            except Exception as e:report["errors"].append(f"{label}: {e}")
    # Always generate the three procedural fighter LODs in a fresh directory.
    # Do not modify the working tree or reuse unknown cached output.
    with tempfile.TemporaryDirectory(prefix="skin-profile-") as temp:
        dest=Path(temp)
        try:
            r=subprocess.run([sys.executable,str(root/"tools/generate_character_examples.py"),"--output",str(dest)],
                             cwd=root,capture_output=True,text=True,timeout=180)
            if r.returncode:raise RuntimeError(r.stderr[-4000:])
            for detail in ("high","medium","low"):
                report["assets"].extend(inspect(dest/f"fighter_{detail}.c","fresh generated fighter "+detail))
        except Exception as e:report["errors"].append("fighter generator: "+str(e))
    report["scope"]="Exported runtime C records only. Dino baked pose playback is not runtime skinning. Local generated snapshot may differ from tracked robot."
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("--out",required=True,type=Path);a=p.parse_args()
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(json.dumps(collect(),indent=2)+"\n")
    print("Weight inventory:",a.out,flush=True)
if __name__=="__main__":main()
