"""Check real application routing and converted payloads against a fake kernel."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import time

EXE = sys.argv.pop(1)

class StreamTests(unittest.TestCase):
    def wait_until(self, predicate):
        end = time.monotonic()+5
        while time.monotonic()<end:
            if predicate(): return
            time.sleep(.02)
        self.fail('timed out waiting for synthetic demand transition')

    def test_on_demand_idle_wake_stop_and_wake_again(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            clients = root/'clients'
            clients.write_text('0')
            env = dict(os.environ,KINECTCAM_TEST_DIR=directory)
            p = subprocess.Popen([EXE,'--test-pattern','--on-demand','--idle-timeout-ms','100',
                                  '--no-rgb','--no-depth','--ir-format','yuyv','--ir-dev','/fake/ir'],
                                 stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,text=True,env=env)
            try:
                image = root/'ir'
                self.wait_until(lambda: image.exists() and image.stat().st_size>0)
                initial = image.stat().st_size
                time.sleep(.15)
                self.assertEqual(image.stat().st_size,initial,'idle must not generate frames')
                self.assertEqual(image.read_bytes()[:4],bytes([16,128,16,128]))
                clients.write_text('1')
                self.wait_until(lambda: image.stat().st_size>initial+2*434176)
                clients.write_text('0')
                time.sleep(.35)
                stopped = image.stat().st_size
                time.sleep(.2)
                self.assertEqual(image.stat().st_size,stopped,'sensor output must stop after grace')
                tail = image.read_bytes()[-434176:]
                self.assertEqual(tail,bytes([16,128])*(512*424),'idle must clear old images')
                clients.write_text('1')
                self.wait_until(lambda: image.stat().st_size>stopped+2*434176)
                self.assertIsNone(p.poll(),'daemon must remain alive across idle periods')
            finally:
                p.terminate()
                _, stderr = p.communicate(timeout=5)
            self.assertEqual(p.returncode,0,stderr)

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
