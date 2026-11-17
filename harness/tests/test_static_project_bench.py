"""Versioned static projection differential and matched production-control timing."""
import json
import os
from pathlib import Path
import unittest
from harness.scripts import telemetry as tel

SIZE=2144
V1_NAMES=("empty","singleton","rotated_32","rotated_256","sparse_64",
          "behind","clamp_x","clamp_y","large_w","mixed_640")
NAMES=V1_NAMES+("rotated_8","rotated_16","rotated_31","rotated_64",
                "reject_first","reject_middle","reject_last","alternating","late_y")
COUNTS=(0,1,32,256,64,33,33,33,33,127,8,16,31,64,128,128,128,128,128)
# Expected number of records actually processed by each native prefix per batch.
# V1 repeats after rejection; V2 tries at most once and bypasses small/sparse.
V1_FAST=(0,1,32,256,64,26,26,26,0,108,8,16,31,64,127,127,127,64,127)
V2_FAST=(0,0,32,256,0,0,0,0,0,0,0,0,0,64,0,64,127,1,96)
LANES=("serial_master","parallel_master","parallel_slave")


def decode(words):
    if len(words)<8:
        raise ValueError("Short telemetry header")
    version=words[1]
    if version==1 and len(words)==236:
        names,width,stride=V1_NAMES,7,76
    elif version==2 and len(words)==SIZE//4:
        names,width,stride=NAMES,9,176
    else:
        raise ValueError("Wrong static bench telemetry version or size")
    r=dict(zip(("magic","version","done","init_status","slave_ready",
                "parallel_status","frequency","reserved"),words[:8]))
    r["lanes"]={}
    for i,name in enumerate(LANES):
        base=8+i*stride
        lane=dict(zip(("on_slave","sr","ccr","done"),words[base:base+4]))
        rows={}
        fields=("cases","errors","first_case","asm_ticks","control_ticks",
                "vertices","fast_vertices","guarded_ticks","guarded_fast_vertices")[:width]
        for j,scenario in enumerate(names):
            idx=base+4+j*width
            row=dict(zip(fields,words[idx:idx+width]))
            a,b=row["asm_ticks"],row["control_ticks"]
            row["reduction_percent"]=100*(b-a)/b if b else None
            if version==2:
                g=row["guarded_ticks"]
                row["guarded_reduction_percent"]=100*(b-g)/b if b else None
                row["guarded_vs_v1_percent"]=100*(a-g)/a if a else None
            rows[scenario]=row
        lane["scenarios"]=rows
        r["lanes"][name]=lane
    return r


def validate(r):
    assert r["magic"]==0x53504231 and r["version"] in (1,2) and r["done"]==1,r
    assert (r["init_status"],r["slave_ready"],r["parallel_status"])==(0,1,0),r
    assert r["frequency"]>0,r
    names=NAMES if r["version"]==2 else V1_NAMES
    for i,name in enumerate(LANES):
        l=r["lanes"][name]
        assert (l["on_slave"],l["done"])==(int(i==2),1),(name,l)
        assert not(l["sr"]&2),(name,l)
        for j,s in enumerate(names):
            row=l["scenarios"][s]
            assert (row["cases"],row["errors"],row["vertices"])==(128,0,128*COUNTS[j]),(name,s,row)
            assert row["fast_vertices"]==128*V1_FAST[j],(name,s,row)
            if r["version"]==2:
                assert row["guarded_fast_vertices"]==128*V2_FAST[j],(name,s,row)


class StaticProjection(unittest.TestCase):
    def test_schema(self):
        words=[0]*(SIZE//4)
        words[:8]=[0x53504231,2,1,0,1,0,210540,0]
        for i in range(3):
            base=8+i*176
            words[base:base+4]=[int(i==2),0,1,1]
            for j,n in enumerate(COUNTS):
                idx=base+4+9*j
                words[idx:idx+9]=[128,0,0,100,200,128*n,128*V1_FAST[j],150,128*V2_FAST[j]]
        r=decode(words)
        validate(r)
        self.assertEqual(r["lanes"]["parallel_slave"]["scenarios"]["late_y"]["guarded_reduction_percent"],25)
        r["lanes"]["parallel_slave"]["scenarios"]["late_y"]["guarded_fast_vertices"]+=1
        with self.assertRaises(AssertionError):
            validate(r)

    def test_target(self):
        dump=os.environ.get("LIBSATURN_STATIC_PROJECT_BENCH_DUMP")
        map_path=os.environ.get("LIBSATURN_STATIC_PROJECT_BENCH_MAP")
        if not dump or not map_path:
            self.skipTest("Run the target benchmark")
        r=decode(tel.load_dump(dump,map_path,"_g_static_project_bench",f">{SIZE//4}I"))
        r["scope"]="Ymir diagnostic elapsed FRT sums; production batch versus V1 and V2"
        Path(dump).with_name("analysis.json").write_text(json.dumps(r,indent=2)+"\n")
        print(json.dumps(r,indent=2))
        validate(r)
