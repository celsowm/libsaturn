"""GPL-3.0: distinguish image changes from VDP1 redraw/swap bookkeeping."""
import copy
import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from analyze_video_observations import FIELDS, analyze, summarize


class VideoObservations(unittest.TestCase):
    def rows(self):
        rows=[]
        for i,(changed,fingerprint) in enumerate(((0,1),(0,1),(1,2),(0,2),(1,1),(0,1))):
            rows.append(dict(frame=i,system_cycles=i*100,width=4,height=3,callback_delta=1,
                crop_x=1,crop_y=1,crop_width=2,crop_height=1,comparable=int(i>0),
                compared_pixels=2 if i else 0,changed_pixels=changed,rgb_fingerprint=fingerprint))
        return rows

    def test_return_to_an_old_image_counts_but_redraw_does_not(self):
        result=summarize(self.rows(),60,1)
        self.assertEqual(result['interval_fields'],4)
        self.assertEqual(result['rgb_change_transitions'],2)
        self.assertEqual(result['rgb_changes_per_second'],30)
        self.assertEqual(result['unchanged_fields'],2)
        self.assertEqual(result['longest_no_change_fields'],1)
        self.assertEqual(summarize(self.rows(),50,1)['rgb_changes_per_second'],25)
        self.assertEqual(summarize(self.rows(),60,0)['rgb_changes_per_second'],24)

    def test_truncation_freshness_geometry_and_corrupt_counts_are_rejected(self):
        for key,value in (('frame',100),('callback_delta',0),('callback_delta',2),
                ('comparable',0),('width',5),('crop_width',5),('compared_pixels',1),
                ('changed_pixels',3),('rgb_fingerprint',7),('system_cycles',0)):
            rows=self.rows();rows[3][key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):summarize(rows,60,1)
        with self.assertRaises(ValueError):summarize(self.rows(),0,1)
        with self.assertRaises(ValueError):summarize(self.rows(),60,5)

    def test_raw_csv_and_metadata_are_bound_and_must_be_complete(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);csv_file=root/'video.csv';meta=root/'probe.json'
            metadata=dict(video_observations=dict(version=1,equality='exact-rgb-pixels-alpha-excluded'),
                frames_run=6,boot=dict(synchronous_video_readback=True,nominal_refresh_hz=60,
                sh2_cache_emulation=True),division_timing_model='immediate-result-no-busy-wait')
            def write(rows):
                with csv_file.open('w',newline='') as stream:
                    writer=csv.DictWriter(stream,fieldnames=FIELDS);writer.writeheader()
                    for row in copy.deepcopy(rows):
                        row['rgb_fingerprint']=format(row['rgb_fingerprint'],'x');writer.writerow(row)
            meta.write_text(json.dumps(metadata));write(self.rows())
            result=analyze(csv_file,meta,1)
            self.assertEqual(len(result['csv_sha256']),64)
            self.assertEqual(result['rgb_change_transitions'],2)
            write(self.rows()[:-1])
            with self.assertRaisesRegex(ValueError,'partial'):analyze(csv_file,meta,1)
            write(self.rows());metadata['boot']['synchronous_video_readback']=False
            meta.write_text(json.dumps(metadata))
            with self.assertRaisesRegex(ValueError,'synchronous'):analyze(csv_file,meta,1)


if __name__=='__main__':unittest.main()
