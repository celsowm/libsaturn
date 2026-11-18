"""C2 screen clip differential telemetry."""
import json,os,struct,unittest
from pathlib import Path
from harness.scripts import telemetry as tel
SIZE=1664
NAMES=("inside","left","right","top","bottom","corner","enclosing","outside","diamond","triangle","degenerate","int16_extreme","arbitrary","max_viewport","odd_viewport","tiny_viewport")
LANES=("serial_master","parallel_master","parallel_slave")
FIELDS=("cases","errors","first_error","quads","pieces","control_ticks","planes_ticks","bounded_ticks")
def decode(words):
    if len(words)!=SIZE//4:raise ValueError("Unexpected clipping report size")
    r=dict(zip(("magic","version","done","init_status","slave_ready","parallel_status","frequency","reserved"),words[:8]))
    r["lanes"]={}
    for i,name in enumerate(LANES):
        base=8+i*136
        lane=dict(zip(("on_slave","sr","ccr","done"),words[base:base+4]))
        lane["scenarios"]={}
        for j,sc in enumerate(NAMES):
            row=dict(zip(FIELDS,words[base+8+j*8:base+16+j*8]))
            a=row["control_ticks"]
            row.update(planes_reduction_percent=100*(a-row["planes_ticks"])/a if a else None,
                       bounded_reduction_percent=100*(a-row["bounded_ticks"])/a if a else None)
            lane["scenarios"][sc]=row
        r["lanes"][name]=lane
    return r
def validate(r):
    assert (r["magic"],r["version"],r["done"])==(0x53434231,1,1),r
    assert (r["init_status"],r["slave_ready"],r["parallel_status"])==(0,1,0),r
    assert r["frequency"]>0,r
    for i,name in enumerate(LANES):
        lane=r["lanes"][name]
        assert (lane["on_slave"],lane["done"])==(int(i==2),1),lane
        for sc,row in lane["scenarios"].items():
            assert (row["cases"],row["errors"],row["quads"])==(64,0,2048),(name,sc,row)
            assert 0<=row["pieces"]<=6*row["quads"],row
            if sc=="inside":assert row["pieces"]==row["quads"],row
            if sc=="outside":assert row["pieces"]==0,row
class ScreenClip(unittest.TestCase):
    def test_target(self):
        dump=os.environ.get("LIBSATURN_SCREEN_CLIP_BENCH_DUMP")
        mapping=os.environ.get("LIBSATURN_SCREEN_CLIP_BENCH_MAP")
        if not dump or not mapping:self.skipTest("Run target benchmark")
        words=tel.load_dump(dump,mapping,"_g_screen_clip_bench",f">{SIZE//4}I")
        r=decode(words)
        Path(dump).with_name("telemetry.bin").write_bytes(struct.pack(f">{SIZE//4}I",*words))
        Path(dump).with_name("analysis.json").write_text(json.dumps(r,indent=2)+"\n")
        validate(r)
