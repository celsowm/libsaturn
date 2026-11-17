"""B3 differential telemetry schema."""
import json,os,struct,unittest
from pathlib import Path
from harness.scripts import telemetry as tel
SIZE=1568
NAMES=("empty","singleton","rigid","single","half","uneven_pair","triple","mixed","q12","q0","partial","extreme")
COUNTS=(0,1,96,96,96,96,96,96,96,96,47,96)
LANES=("serial_master","parallel_master","parallel_slave")
def decode(words):
    if len(words)!=SIZE//4:raise ValueError("Wrong weighted blend telemetry size")
    r=dict(zip(("magic","version","done","init_status","slave_ready","parallel_status","frequency","reserved"),words[:8]))
    r["lanes"]={}
    for i,name in enumerate(LANES):
        base=8+i*128
        lane=dict(zip(("on_slave","sr","ccr","done","edge_checks","edge_errors"),words[base:base+6]))
        lane["scenarios"]={}
        for j,sc in enumerate(NAMES):
            idx=base+8+j*10
            row=dict(zip(("cases","errors","first_error","control_ticks","candidate_ticks","vertices","pair_axes","triple_axes","fallback_axes","selective_ticks"),words[idx:idx+10]))
            c=row["control_ticks"]
            row["reduction_percent"]=100*(c-row["candidate_ticks"])/c if c else None
            if r["version"]==2:
                row["selective_reduction_percent"]=100*(c-row["selective_ticks"])/c if c else None
            lane["scenarios"][sc]=row
        r["lanes"][name]=lane
    return r

def validate(r):
    assert r["magic"]==0x57424231 and r["version"] in (1,2) and r["done"]==1,r
    assert (r["init_status"],r["slave_ready"],r["parallel_status"])==(0,1,0),r
    assert r["frequency"]>0,r
    for i,name in enumerate(LANES):
        l=r["lanes"][name]
        assert (l["on_slave"],l["done"],l["edge_checks"],l["edge_errors"])==(int(i==2),1,4480 if r["version"]==2 else 4032,0),l
        assert not l["sr"]&2,l
        for j,sc in enumerate(NAMES):
            row=l["scenarios"][sc]
            assert (row["cases"],row["errors"],row["vertices"])==(64,0,64*COUNTS[j]),(name,sc,row)
            assert row["fallback_axes"]<=row["pair_axes"]+row["triple_axes"]<=3*row["vertices"],row
            if sc=="extreme":assert row["fallback_axes"]>0,row
            else:assert row["fallback_axes"]==0,row
            if sc in ("empty","rigid","single","half"):assert row["pair_axes"]==row["triple_axes"]==0,row
            if sc=="uneven_pair":assert row["pair_axes"]==64*96*3 and row["triple_axes"]==0,row
            if sc=="triple":assert row["triple_axes"]==64*96*3 and row["pair_axes"]==0,row

class WeightedBlend(unittest.TestCase):
    def test_target(self):
        dump=os.environ.get("LIBSATURN_WEIGHTED_BLEND_BENCH_DUMP")
        mapping=os.environ.get("LIBSATURN_WEIGHTED_BLEND_BENCH_MAP")
        if not dump or not mapping:self.skipTest("Run native benchmark")
        words=tel.load_dump(dump,mapping,"_g_weighted_blend_bench",f">{SIZE//4}I")
        r=decode(words)
        Path(dump).with_name("telemetry.bin").write_bytes(struct.pack(f">{SIZE//4}I",*words))
        Path(dump).with_name("analysis.json").write_text(json.dumps(r,indent=2)+"\n")
        validate(r)
