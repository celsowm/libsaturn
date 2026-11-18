"""C2 screen clip differential telemetry."""
import json,os,struct,unittest
from pathlib import Path
from harness.scripts import telemetry as tel
SIZE=2048
NAMES=("inside","outside","one_inside","two_inside","three_inside","alternating","on_plane","duplicate_corner","axis_x","axis_negative_z","oblique_xz","oblique_xyz","wide_coordinate_delta","wide_depth_delta","wide_eye_delta","invalid_args","axis_negative_y","axis_negative_x","nonunit_forward","one_lsb_crossing")
LANES=("serial_master","parallel_master","parallel_slave")
FIELDS=("cases","errors","first_error","quads","pieces","control_ticks","depth_ticks","intersection_ticks")
def decode(words):
    if len(words)!=SIZE//4:raise ValueError("Unexpected clipping report size")
    r=dict(zip(("magic","version","done","init_status","slave_ready","parallel_status","frequency","reserved"),words[:8]))
    r["lanes"]={}
    for i,name in enumerate(LANES):
        base=8+i*168
        lane=dict(zip(("on_slave","sr","ccr","done"),words[base:base+4]))
        lane["scenarios"]={}
        for j,sc in enumerate(NAMES):
            row=dict(zip(FIELDS,words[base+8+j*8:base+16+j*8]))
            a=row["control_ticks"]
            row.update(depth_reduction_percent=100*(a-row["depth_ticks"])/a if a else None,
                       intersection_reduction_percent=100*(a-row["intersection_ticks"])/a if a else None)
            lane["scenarios"][sc]=row
        r["lanes"][name]=lane
    return r
def validate(r):
    assert (r["magic"],r["version"],r["done"])==(0x4e434231,1,1),r
    assert (r["init_status"],r["slave_ready"],r["parallel_status"])==(0,1,0),r
    assert r["frequency"]>0,r
    for i,name in enumerate(LANES):
        lane=r["lanes"][name]
        assert (lane["on_slave"],lane["done"])==(int(i==2),1),lane
        for sc,row in lane["scenarios"].items():
            assert (row["cases"],row["errors"],row["quads"])==(64,0,2048),(name,sc,row)
            assert 0<=row["pieces"]<=4*row["quads"],row
            if sc=="inside":assert row["pieces"]==row["quads"],row
            if sc in ("outside","invalid_args"):assert row["pieces"]==0,row
            if sc=="alternating":assert row["pieces"]==4*row["quads"],row
            if sc=="one_inside":assert row["pieces"]==row["quads"],row
            if sc in ("two_inside","one_lsb_crossing","wide_coordinate_delta","wide_depth_delta"):assert row["pieces"]==2*row["quads"],row
class ScreenClip(unittest.TestCase):
    def test_target(self):
        dump=os.environ.get("LIBSATURN_NEAR_CLIP_BENCH_DUMP")
        mapping=os.environ.get("LIBSATURN_NEAR_CLIP_BENCH_MAP")
        if not dump or not mapping:self.skipTest("Run target benchmark")
        words=tel.load_dump(dump,mapping,"_g_near_clip_bench",f">{SIZE//4}I")
        r=decode(words)
        Path(dump).with_name("telemetry.bin").write_bytes(struct.pack(f">{SIZE//4}I",*words))
        Path(dump).with_name("analysis.json").write_text(json.dumps(r,indent=2)+"\n")
        validate(r)
