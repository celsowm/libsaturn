"""Target static projection differential and production-control timing."""
import json
import os
from pathlib import Path
import unittest
from harness.scripts import telemetry as tel

SIZE=944
NAMES=("empty","singleton","rotated_32","rotated_256","sparse_64",
       "behind","clamp_x","clamp_y","large_w","mixed_640")
COUNTS=(0,1,32,256,64,33,33,33,33,127)
LANES=("serial_master","parallel_master","parallel_slave")


def decode(words):
    if len(words)!=SIZE//4:
        raise ValueError("Wrong static bench telemetry size")
    r=dict(zip(("magic","version","done","init_status","slave_ready",
                "parallel_status","frequency","reserved"),words[:8]))
    r["lanes"]={}
    for i,name in enumerate(LANES):
        base=8+i*76
        lane=dict(zip(("on_slave","sr","ccr","done"),words[base:base+4]))
        rows={}
        for j,scenario in enumerate(NAMES):
            idx=base+4+j*7
            row=dict(zip(("cases","errors","first_case","asm_ticks","control_ticks",
                          "vertices","fast_vertices"),words[idx:idx+7]))
            a,b=row["asm_ticks"],row["control_ticks"]
            row["reduction_percent"]=100*(b-a)/b if b else None
            rows[scenario]=row
        lane["scenarios"]=rows
        r["lanes"][name]=lane
    return r


def validate(r):
    assert (r["magic"],r["version"],r["done"])==(0x53504231,1,1),r
    assert (r["init_status"],r["slave_ready"],r["parallel_status"])==(0,1,0),r
    assert r["frequency"]>0,r
    for i,name in enumerate(LANES):
        l=r["lanes"][name]
        assert (l["on_slave"],l["done"])==(int(i==2),1),(name,l)
        assert not(l["sr"]&2),(name,l)
        for j,s in enumerate(NAMES):
            row=l["scenarios"][s]
            assert (row["cases"],row["errors"],row["vertices"])==(128,0,128*COUNTS[j]),(name,s,row)
            assert 0<=row["fast_vertices"]<=row["vertices"],(name,s,row)
            if s in ("rotated_32","rotated_256"):
                assert row["fast_vertices"]==row["vertices"],(name,s,row)
            if s in ("behind","clamp_x","clamp_y","mixed_640"):
                assert 0<row["fast_vertices"]<row["vertices"],(name,s,row)
            if s=="large_w":
                assert row["fast_vertices"]==0,(name,s,row)


class StaticProjection(unittest.TestCase):
    def test_target(self):
        dump=os.environ.get("LIBSATURN_STATIC_PROJECT_BENCH_DUMP")
        map_path=os.environ.get("LIBSATURN_STATIC_PROJECT_BENCH_MAP")
        if not dump or not map_path:
            self.skipTest("Run the target benchmark")
        r=decode(tel.load_dump(dump,map_path,"_g_static_project_bench",">236I"))
        r["scope"]="Ymir diagnostic elapsed FRT sums; production batch versus candidate"
        Path(dump).with_name("analysis.json").write_text(json.dumps(r,indent=2)+"\n")
        print(json.dumps(r,indent=2))
        validate(r)
