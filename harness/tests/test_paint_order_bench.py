"""C1 V2 complete painter timing and exact key/order telemetry."""
import json,os,struct,unittest
from pathlib import Path
from harness.scripts import telemetry as tel
SIZE=2432
NAMES=("empty","singleton","byte_small_span","wide_boundary","all_skipped","equal","ascending","robot_1800","dense_random","sparse_random","extreme_unsigned","clustered","byte_sparse","two_live","one_pass","two_pass_runs","four_pass_interleaved","eight_pass","nine_pass_fallback","byte_grouped","mixed_skip_grouped","shift1","shift31","group_equal_sparse")
COUNTS=(0,1,255,256,96,512,1024,1800,2048,2048,2048,2048,255,2048,2048,2048,2048,2048,2048,255,2048,2048,2048,255)
LANES=("serial_master","parallel_master","parallel_slave")
FIELDS=("cases","errors","first_error","faces","live","control_ticks","byte_ticks","v2_ticks")
def decode(words):
    if len(words)!=SIZE//4:raise ValueError("Unexpected painter report size")
    r=dict(zip(("magic","version","done","init_status","slave_ready","parallel_status","frequency","reserved"),words[:8]))
    r["lanes"]={}
    for i,name in enumerate(LANES):
        base=8+i*200
        lane=dict(zip(("on_slave","sr","ccr","done","edges","edge_errors"),words[base:base+6]))
        lane["scenarios"]={}
        for j,sc in enumerate(NAMES):
            row=dict(zip(FIELDS,words[base+8+j*8:base+16+j*8]))
            a=row["control_ticks"]
            row.update(byte_reduction_percent=100*(a-row["byte_ticks"])/a if a else None,
                       v2_reduction_percent=100*(a-row["v2_ticks"])/a if a else None)
            lane["scenarios"][sc]=row
        r["lanes"][name]=lane
    return r
def validate(r):
    assert (r["magic"],r["version"],r["done"])==(0x504f4232,2,1),r
    assert (r["init_status"],r["slave_ready"],r["parallel_status"])==(0,1,0),r
    assert r["frequency"]>0,r
    for i,name in enumerate(LANES):
        lane=r["lanes"][name]
        assert (lane["on_slave"],lane["done"],lane["edge_errors"])==(int(i==2),1,0),lane
        for j,sc in enumerate(NAMES):
            row=lane["scenarios"][sc]
            assert (row["cases"],row["errors"],row["faces"])==(32,0,32*COUNTS[j]),(name,sc,row)
            assert 0<=row["live"]<=row["faces"],row
            if sc in ("empty","all_skipped"):assert row["live"]==0,row
            elif sc=="two_live":assert row["live"]==64,row
            elif sc=="group_equal_sparse":assert row["live"]==32*128,row
class PainterOrder(unittest.TestCase):
    def test_target(self):
        dump=os.environ.get("LIBSATURN_PAINT_ORDER_BENCH_DUMP")
        mapping=os.environ.get("LIBSATURN_PAINT_ORDER_BENCH_MAP")
        if not dump or not mapping:self.skipTest("Run target benchmark")
        words=tel.load_dump(dump,mapping,"_g_paint_order_bench",f">{SIZE//4}I")
        r=decode(words)
        Path(dump).with_name("telemetry.bin").write_bytes(struct.pack(f">{SIZE//4}I",*words))
        Path(dump).with_name("analysis.json").write_text(json.dumps(r,indent=2)+"\n")
        validate(r)
