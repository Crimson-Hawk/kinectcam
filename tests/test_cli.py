"""Argument mistakes must be rejected before any USB or camera access."""
import subprocess
import sys
import unittest

EXE = sys.argv.pop(1)

class CliTests(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run([EXE, *args], text=True, capture_output=True, timeout=5)

    def test_help(self):
        p = self.run_cli('--help')
        self.assertEqual(p.returncode, 0)
        self.assertIn('--test-pattern', p.stdout)
        self.assertIn('--ir-gain', p.stdout)

    def test_no_outputs(self):
        p = self.run_cli('--no-rgb', '--no-ir', '--no-depth')
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('at least one output', p.stderr)

    def test_invalid_arguments(self):
        for args, message in [
            (['--pipeline', 'nonsense'], 'pipeline'),
            (['--ir-gain', 'nan'], 'ir-gain'),
            (['--ir-gamma', '0'], 'ir-gamma'),
            (['--frames', '-1'], 'frames'),
            (['--frames', '2x'], 'frames'),
            (['--ir-dev'], 'requires a value'),
            (['--ir-dev', '--no-rgb'], 'requires a value'),
            (['--rgb-dev', '/dev/video11'], 'distinct'),
            (['--ir-format', 'rgb'], 'ir-format'),
            (['--depth-format', 'rgb'], 'depth-format'),
        ]:
            with self.subTest(args=args):
                p = self.run_cli(*args)
                self.assertNotEqual(p.returncode, 0)
                self.assertIn(message, p.stderr)
                self.assertNotIn('libusb', p.stderr + p.stdout)

    def test_test_pattern_never_uses_usb(self):
        p = self.run_cli('--test-pattern', '--frames', '1', '--rgb-dev', '/nonexistent/kinect')
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('/nonexistent/kinect', p.stderr)
        self.assertNotIn('libusb', p.stderr + p.stdout)

if __name__ == '__main__':
    unittest.main()
