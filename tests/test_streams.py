"""Check real application routing and converted payloads against a fake kernel."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

EXE = sys.argv.pop(1)

class StreamTests(unittest.TestCase):
    def capture(self, *args):
        with tempfile.TemporaryDirectory() as directory:
            env = dict(os.environ, KINECTCAM_TEST_DIR=directory)
            p = subprocess.run([EXE, '--test-pattern', '--frames', '2',
                                '--rgb-dev', '/fake/rgb', '--ir-dev', '/fake/ir',
                                '--depth-dev', '/fake/depth', *args],
                               capture_output=True, text=True, env=env, timeout=10)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertIn('SYNTHETIC TEST PATTERN', p.stderr)
            return {f.name: f.read_bytes() for f in Path(directory).iterdir()}

    def test_all_three_outputs(self):
        files = self.capture('--ir-gain', '1')
        self.assertEqual(set(files), {'rgb', 'ir', 'depth'})
        self.assertEqual(len(files['rgb']), 1920*1080*2*2)
        self.assertEqual(len(files['ir']), 512*424*2)
        self.assertEqual(len(files['depth']), 512*424*2*2)
        self.assertEqual(files['rgb'][:4], bytes([16,128,16,128]))
        self.assertEqual(files['ir'][:4], bytes([0,0,1,1]))
        self.assertEqual(files['depth'][:8], bytes([244,1,252,1,4,2,12,2]))

    def test_ir_only_compatibility_format(self):
        files = self.capture('--no-rgb', '--no-depth', '--ir-format', 'yuyv', '--ir-gain', '1')
        self.assertEqual(set(files), {'ir'})
        self.assertEqual(len(files['ir']), 512*424*2*2)
        self.assertEqual(files['ir'][:8], bytes([16,128,16,128,17,128,17,128]))

if __name__ == '__main__':
    unittest.main()
