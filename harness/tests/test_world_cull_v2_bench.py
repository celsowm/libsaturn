"""C2 world cull V2 differential telemetry."""
import json,os,struct,unittest
from pathlib import Path
from harness.scripts import telemetry as tel
SIZE=3152
NAMES=("small_axis_z","small_axis_x","small_axis_y","small_oblique","small_reversed","collapsed_b","collapsed_d","collinear","signed16_boundary","just_wide","medium_axis","medium_split_zero","medium_split_nonzero","tangent","wide_center","negative_center","scaled_normal","exact_limit","below_limit","nonplanar","mixed","extreme_view","negative_split_zero","odd_center","edge_box_extreme","edge_box_tangent","edge_box_near_zero","near_parallel_extreme")
LANES=("serial_master","parallel_master","parallel_slave")
FIELDS=("cases","errors","first_error","faces","visible","control_ticks","wide_ticks","dmuls_ticks","mac_ticks")
def decode(words):
    if len(words)!=SIZE//4:raise ValueError("Unexpected clipping report size")
    r=dict(zip(("magic","version","done","init_status","slave_ready","parallel_status","frequency","reserved"),words[:8]))
    r["lanes"]={}
    for i,name in enumerate(LANES):
        base=8+i*260
        lane=dict(zip(("on_slave","sr","ccr","done"),words[base:base+4]))
        lane["scenarios"]={}
        for j,sc in enumerate(NAMES):
            row=dict(zip(FIELDS,words[base+8+j*9:base+17+j*9]))
            a=row["control_ticks"]
            row.update({f"{mode}_reduction_percent":100*(a-row[f"{mode}_ticks"])/a if a else None
                        for mode in ("wide","dmuls","mac")})
            lane["scenarios"][sc]=row
        r["lanes"][name]=lane
    return r
def validate(r):
    assert (r["magic"],r["version"],r["done"])==(0x57434232,2,1),r
    assert (r["init_status"],r["slave_ready"],r["parallel_status"])==(0,1,0),r
    assert r["frequency"]>0,r
    for i,name in enumerate(LANES):
        lane=r["lanes"][name]
        assert (lane["on_slave"],lane["done"])==(int(i==2),1),lane
        for sc,row in lane["scenarios"].items():
            assert (row["cases"],row["errors"],row["faces"])==(64,0,2048),(name,sc,row)
            assert 0<=row["visible"]<=row["faces"],row
            if sc=="collinear":assert row["visible"]==0,row
class WorldCullV2(unittest.TestCase):
    def test_target(self):
        dump=os.environ.get("LIBSATURN_WORLD_CULL_V2_BENCH_DUMP")
        mapping=os.environ.get("LIBSATURN_WORLD_CULL_V2_BENCH_MAP")
        if not dump or not mapping:self.skipTest("Run target benchmark")
        words=tel.load_dump(dump,mapping,"_g_world_cull_v2_bench",f">{SIZE//4}I")
        r=decode(words)
        Path(dump).with_name("telemetry.bin").write_bytes(struct.pack(f">{SIZE//4}I",*words))
        Path(dump).with_name("analysis.json").write_text(json.dumps(r,indent=2)+"\n")
        validate(r)
