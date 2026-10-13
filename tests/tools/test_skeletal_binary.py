"""Independent wire-layout and original-character export regression checks."""
import sys
import struct
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from model_pipeline import skeletal_export as sx
from model_pipeline.skeletal_binary import encode,compression_report,HEADER_BYTES,JOINT,POSE
from generate_character_examples import build_source,render_asset,SAMPLE_RATE,LOOP_CLIPS

class BinaryTests(unittest.TestCase):
    def test_wire_layout_random_access_and_names(self):
        model,quads,materials=build_source('low')
        exp=sx.export_model(model,rate=SAMPLE_RATE,loop_clips=LOOP_CLIPS)
        render=render_asset(model,exp,quads,materials)
        payload=encode(exp,render,[n.name for n in model.nodes])
        header=struct.unpack_from('>24I',payload)
        self.assertEqual(header[:3],(0x534B4131,1,len(payload)))
        self.assertEqual(header[3],len(exp.skeleton.parent));self.assertEqual(header[10],len(exp.clips))
        for i,clip in enumerate(exp.clips):
            meta=struct.unpack_from('>III6H',payload,header[21]+i*struct.calcsize('>III6H'))
            self.assertEqual(meta[3],clip.frame_count)
            pose=struct.unpack_from('>3i4h',payload,meta[0]+(clip.frame_count-1)*meta[4]*POSE.size)
            self.assertEqual(pose,(*clip.frames[-1][0][0],*clip.frames[-1][0][1]))
        self.assertEqual(JOINT.size,struct.calcsize('>HH3i4h3i'))
        self.assertEqual(POSE.size,sx.POSE_BYTES)
        self.assertEqual(HEADER_BYTES,struct.calcsize('>24I'))
        report=compression_report(exp)
        self.assertLess(report['constant_channel_bytes'],report['native_pose_bytes'])
        self.assertLess(report['max_translation_error'],.0001)
        # Native quads must have matching header/table counts, not triangle count.
        header_c,source_c=sx.emit_c(exp,'fixture',render)
        self.assertIn(f'FIXTURE_FACE_COUNT {len(quads)}',header_c)
        self.assertIn(f'fixture_indices[{len(quads)*4}]',source_c)

if __name__=='__main__':unittest.main()
