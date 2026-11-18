"""Painter stage timing and differential report."""
import json,os,struct,unittest
from pathlib import Path
from harness.scripts import telemetry as tel
SIZE=2432
NAMES=("empty","singleton","byte_small_span","wide_boundary","all_skipped","equal","ascending","robot_1800","dense_random","sparse_random","extreme_unsigned","clustered","pass_bits","byte_sparse","two_live","bucket_edges")
COUNTS=(0,1,255,256,96,512,1024,1800,2048,2048,2048,2048,1024,255,2048,2048)
LANES=("serial_master","parallel_master","parallel_slave")
FIELDS=("cases","errors","first_error","faces","live","range_ticks","spread_ticks","prefix_cpp_ticks","prefix_asm_ticks","place_cpp_ticks","place_asm_ticks","words")
def reduction(a,b):
    return 100*(a-b)/a if a else None
def decode(words):
    if len(words)!=SIZE//4:raise ValueError("Unexpected painter report size")
    r=dict(zip(("magic","version","done","init_status","slave_ready","parallel_status","frequency","reserved"),words[:8]))
    r["lanes"]={}
    for i,name in enumerate(LANES):
        base=8+i*200
        lane=dict(zip(("on_slave","sr","ccr","done","edges","edge_errors"),words[base:base+6]))
        lane["scenarios"]={}
        for j,sc in enumerate(NAMES):
            row=dict(zip(FIELDS,words[base+8+j*12:base+20+j*12]))
            a,b=row["prefix_cpp_ticks"],row["prefix_asm_ticks"]
            c,d=row["place_cpp_ticks"],row["place_asm_ticks"]
            row.update(prefix_reduction_percent=reduction(a,b),placement_reduction_percent=reduction(c,d),
                       final_stages_reduction_percent=reduction(a+c,b+d),
                       staged_pipeline_reduction_percent=reduction(a+c+row["range_ticks"]+row["spread_ticks"],b+d+row["range_ticks"]+row["spread_ticks"]))
            lane["scenarios"][sc]=row
        r["lanes"][name]=lane
    return r
def validate(r):
    assert (r["magic"],r["version"],r["done"])==(0x50444231,1,1),r
    assert (r["init_status"],r["slave_ready"],r["parallel_status"])==(0,1,0),r
    assert r["frequency"]>0,r
    for i,name in enumerate(LANES):
        lane=r["lanes"][name]
        assert (lane["on_slave"],lane["done"],lane["edges"],lane["edge_errors"])==(int(i==2),1,5,0),lane
        for j,sc in enumerate(NAMES):
            row=lane["scenarios"][sc]
            assert (row["cases"],row["errors"],row["faces"])==(32,0,32*COUNTS[j]),(name,sc,row)
            assert 0<=row["live"]<=row["faces"] and 0<=row["words"]<=32*512,row
            if sc in ("empty","all_skipped"):assert row["live"]==row["words"]==0,row
            if sc=="two_live":assert row["live"]==64,row
            if sc=="equal":assert row["live"]==32*512 and row["words"]==32,row

class PainterDistribution(unittest.TestCase):
    def test_target(self):
        dump=os.environ.get("LIBSATURN_PAINT_DISTRIBUTION_BENCH_DUMP")
        mapping=os.environ.get("LIBSATURN_PAINT_DISTRIBUTION_BENCH_MAP")
        if not dump or not mapping:self.skipTest("Run target benchmark")
        words=tel.load_dump(dump,mapping,"_g_paint_distribution_bench",f">{SIZE//4}I")
        r=decode(words)
        Path(dump).with_name("telemetry.bin").write_bytes(struct.pack(f">{SIZE//4}I",*words))
        Path(dump).with_name("analysis.json").write_text(json.dumps(r,indent=2)+"\n")
        validate(r)
