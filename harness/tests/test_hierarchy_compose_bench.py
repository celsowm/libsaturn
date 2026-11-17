"""Exact hierarchy evaluation against production and independent oracle."""
import json,os,struct,unittest
from pathlib import Path
from harness.scripts import telemetry as tel
SIZE=1088
NAMES=("root","branch_22","chain_66","chain_96","scaled_branch_66","forest_66","reverse_66","scaled_pose_66","unchanged_66","dirty_leaf_66")
COUNTS=(1,22,66,96,66,66,66,66,66,66)
MULS=(0,21,65,95,65,60,65,65,0,1)
LANES=("serial_master","parallel_master","parallel_slave")
def decode(words):
    if len(words)!=SIZE//4:raise ValueError("Wrong hierarchy telemetry size")
    r=dict(zip(("magic","version","done","init_status","slave_ready","parallel_status","frequency","reserved"),words[:8]))
    r["lanes"]={}
    for i,name in enumerate(LANES):
        base=8+i*88
        lane=dict(zip(("on_slave","sr","ccr","done","edge_checks","edge_errors"),words[base:base+6]))
        rows={}
        for j,sc in enumerate(NAMES):
            idx=base+8+j*8
            row=dict(zip(("cases","errors","first_case","control_ticks","cpp_ticks","asm_ticks","nodes","compositions"),words[idx:idx+8]))
            c=row["control_ticks"]
            for alternative in ("cpp","asm"):
                row[alternative+"_reduction_percent"]=100*(c-row[alternative+"_ticks"])/c if c else None
            rows[sc]=row
        lane["scenarios"]=rows;r["lanes"][name]=lane
    return r

def validate(r):
    assert (r["magic"],r["version"],r["done"])==(0x48434231,1,1),r
    assert (r["init_status"],r["slave_ready"],r["parallel_status"])==(0,1,0),r
    assert r["frequency"]>0,r
    for i,name in enumerate(LANES):
        l=r["lanes"][name]
        assert (l["on_slave"],l["done"],l["edge_checks"],l["edge_errors"])==(int(i==2),1,195,0),l
        for j,sc in enumerate(NAMES):
            row=l["scenarios"][sc]
            assert (row["cases"],row["errors"],row["nodes"],row["compositions"])==(64,0,64*COUNTS[j],64*MULS[j]),(name,sc,row)

class Hierarchy(unittest.TestCase):
    def test_target(self):
        dump=os.environ.get("LIBSATURN_HIERARCHY_COMPOSE_BENCH_DUMP")
        mapping=os.environ.get("LIBSATURN_HIERARCHY_COMPOSE_BENCH_MAP")
        if not dump or not mapping:self.skipTest("Run native benchmark")
        words=tel.load_dump(dump,mapping,"_g_hierarchy_compose_bench",f">{SIZE//4}I")
        r=decode(words)
        Path(dump).with_name("telemetry.bin").write_bytes(struct.pack(f">{SIZE//4}I",*words))
        Path(dump).with_name("analysis.json").write_text(json.dumps(r,indent=2)+"\n")
        validate(r)
    def test_schema(self):
        words=[0]*(SIZE//4);words[:8]=[0x48434231,1,1,0,1,0,210540,0]
        for i in range(3):
            base=8+i*88;words[base:base+6]=[int(i==2),0,1,1,195,0]
            for j,n in enumerate(COUNTS):
                at=base+8+j*8;words[at:at+8]=[64,0,0,100,90,80,64*n,64*MULS[j]]
        r=decode(words);validate(r)
        self.assertEqual(r["lanes"]["serial_master"]["scenarios"]["chain_96"]["asm_reduction_percent"],20)
