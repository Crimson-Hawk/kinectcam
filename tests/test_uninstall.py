"""Exercise the real CMake uninstall target entirely inside temporary prefixes."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(sys.argv.pop(1)).resolve()
CMAKE = sys.argv.pop(1)


class UninstallTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='kinectcam uninstall ')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        source = self.root / 'source'
        source.mkdir()
        (source / 'installed.txt').write_text('installed asset')
        (source / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.16)\n'
            'project(uninstall_fixture NONE)\n'
            f'include("{SOURCE / "cmake/Uninstall.cmake"}" OPTIONAL)\n'
            'install(FILES installed.txt DESTINATION share/kinectcam)\n')
        self.build = self.root / 'build'
        self.prefix = self.root / 'prefix with spaces'
        self.env = dict(os.environ)
        self.env.pop('DESTDIR', None)
        self.run_ok(CMAKE, '-S', source, '-B', self.build,
                    f'-DCMAKE_INSTALL_PREFIX={self.prefix}')

    def run_command(self, *args):
        return subprocess.run(list(map(str, args)), env=self.env,
                              capture_output=True, text=True, timeout=20)

    def run_ok(self, *args):
        result = self.run_command(*args)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def uninstall(self):
        return self.run_command(CMAKE, '--build', self.build, '--target', 'uninstall')

    def test_destdir_isolates_removal_and_preserves_user_files(self):
        # Losing DESTDIR must not delete the real prefix instead of the stage.
        live_file = self.prefix / 'share/kinectcam/installed.txt'
        live_file.parent.mkdir(parents=True)
        live_file.write_text('outside the staging root')
        stage = self.root / 'stage with spaces'
        self.env['DESTDIR'] = str(stage)
        self.run_ok(CMAKE, '--install', self.build)
        staged_prefix = stage / self.prefix.relative_to('/')
        installed = staged_prefix / 'share/kinectcam/installed.txt'
        self.assertEqual(installed.read_text(), 'installed asset')
        user_file = installed.parent / 'user.conf'
        user_file.write_text('keep user settings')
        dangling = installed.parent / 'dangling-link'
        dangling.symlink_to('nonexistent')
        with (self.build / 'install_manifest.txt').open('a') as manifest:
            manifest.write('\n' + str(self.prefix / 'share/kinectcam/dangling-link') + '\n')
        for _ in range(2):
            result = self.uninstall()
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertFalse(installed.exists())
            self.assertFalse(dangling.is_symlink())
            self.assertEqual(live_file.read_text(), 'outside the staging root')
            self.assertEqual(user_file.read_text(), 'keep user settings')

    def test_install_prefix_override_uses_the_manifest(self):
        prefix = self.root / 'overridden prefix'
        self.run_ok(CMAKE, '--install', self.build, '--prefix', prefix)
        installed = prefix / 'share/kinectcam/installed.txt'
        self.assertTrue(installed.is_file())
        result = self.uninstall()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(installed.exists())

    def test_missing_manifest_explains_that_install_is_required(self):
        result = self.uninstall()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('install_manifest.txt', result.stdout + result.stderr)

    def test_directory_entry_cannot_remove_user_contents(self):
        directory = self.root / 'user directory'
        directory.mkdir()
        user_file = directory / 'keep.txt'
        user_file.write_text('keep')
        (self.build / 'install_manifest.txt').write_text(str(directory) + '\n')
        result = self.uninstall()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('directory', result.stdout + result.stderr)
        self.assertEqual(user_file.read_text(), 'keep')


if __name__ == '__main__':
    unittest.main()
