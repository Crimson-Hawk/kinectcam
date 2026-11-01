"""Exercise setup idempotency with fake privileged tools, never kernel devices."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = sys.argv.pop(1)
FAKE = r'''
import json, os, sys
from pathlib import Path
root = Path(os.environ['KINECTCAM_SETUP_TEST'])
name = Path(sys.argv[0]).name
args = sys.argv[1:]
if name == 'id':
    print(0)
    sys.exit(0)
with (root/'calls').open('a') as f:
    f.write(json.dumps([name, *args])+'\n')
if name == 'v4l2loopback-ctl':
    state = json.loads((root/'state').read_text())
    dev = args[-1]
    if args[0] == 'query':
        if dev not in state:
            sys.exit(1)
        print('card_label : '+state[dev])
        print('announce_all_caps: 0')
    elif args[0] == 'add':
        state[dev] = args[args.index('-n')+1]
        (root/'state').write_text(json.dumps(state))
    else:
        sys.exit(80)
'''

class SetupTests(unittest.TestCase):
    def setUp(self):
        # Real loopbacks may already exist on a developer's machine. Use free
        # IDs so collision checks never depend on the live Kinect installation.
        available = [i for i in range(200,256)
                     if not Path('/dev/video'+str(i)).exists()
                     and not Path('/sys/class/video4linux/video'+str(i)).exists()]
        self.assertGreaterEqual(len(available),3,'need three unused video IDs for setup fixtures')
        self.numbers = ','.join(map(str,available[:3]))
        self.devices = ['/dev/video'+str(i) for i in available[:3]]
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root/'state').write_text('{}')
        (self.root/'calls').touch()
        for tool in ('modprobe', 'v4l2loopback-ctl', 'udevadm', 'id'):
            f = self.root/tool
            f.write_text('#!'+sys.executable+'\n'+FAKE)
            f.chmod(0o755)
        self.env = dict(os.environ, PATH=str(self.root)+':'+os.environ['PATH'],
                        KINECTCAM_SETUP_TEST=str(self.root))

    def run_setup(self, *args):
        return subprocess.run(['bash',SCRIPT,'--video-nr',self.numbers,*args], capture_output=True, text=True,
                              env=self.env, timeout=10)

    def calls(self):
        return [json.loads(s) for s in (self.root/'calls').read_text().splitlines()]

    def test_repeated_setup_preserves_existing_devices(self):
        for _ in range(2):
            p = self.run_setup()
            self.assertEqual(p.returncode,0,p.stderr)
        adds = [c for c in self.calls() if c[:2] == ['v4l2loopback-ctl','add']]
        self.assertEqual(len(adds),3)
        self.assertEqual({c[-1] for c in adds},set(self.devices))
        for c in self.calls():
            self.assertNotIn('-r',c)
            self.assertNotIn('delete',c)

    def test_conflict_creates_no_devices(self):
        (self.root/'state').write_text(json.dumps({self.devices[1]:'OBS Camera'}))
        p = self.run_setup()
        self.assertNotEqual(p.returncode,0)
        self.assertIn('incompatible',p.stderr)
        self.assertFalse(any(c[:2] == ['v4l2loopback-ctl','add'] for c in self.calls()))

    def test_dry_run_never_invokes_privileged_tools(self):
        p = self.run_setup('--dry-run')
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(self.calls(),[])

    def test_invalid_device_numbers_never_invoke_tools(self):
        for ids in ('10,10,12','256,11,12','10,11','01,11,12'):
            p = self.run_setup('--video-nr',ids)
            self.assertNotEqual(p.returncode,0)
        self.assertEqual(self.calls(),[])

if __name__ == '__main__':
    unittest.main()
