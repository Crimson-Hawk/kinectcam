"""Exercise real Git publication against isolated local bare repositories."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT/'scripts/publish-aur.sh'
URL = 'ssh://aur@aur.archlinux.org/kinectcam-git.git'

def run(*args, **kwargs):
    return subprocess.run(args, text=True, capture_output=True, check=True, **kwargs)

class PublishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.keys = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.keys.cleanup)
        key = Path(cls.keys.name)/'key'
        run('ssh-keygen','-q','-t','ed25519','-N','','-f',str(key))
        cls.private_key = key.read_text()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.remote = self.root/'remote.git'
        run('git','init','--bare','--initial-branch=master',str(self.remote))
        self.package = self.root/'package'
        self.package.mkdir()
        (self.package/'PKGBUILD').write_text('pkgname=kinectcam-git\npkgver=0.2.0.r1.g1234567\npkgrel=1\n')
        (self.package/'.SRCINFO').write_text('pkgbase = kinectcam-git\n\tpkgver = 0.2.0.r1.g1234567\n\tpkgrel = 1\npkgname = kinectcam-git\n')
        config = self.root/'gitconfig'
        config.write_text('[url "'+self.remote.as_uri()+'"]\n\tinsteadOf = '+URL+'\n')
        self.env = dict(os.environ, GIT_CONFIG_GLOBAL=str(config), GIT_CONFIG_NOSYSTEM='1',
                        AUR_REPO_URL=URL, AUR_SSH_KEY=self.private_key)

    def publish(self):
        return subprocess.run(['bash',str(SCRIPT),str(self.package)], env=self.env,
                              capture_output=True,text=True,timeout=15)

    def git(self, *args):
        return run('git','--git-dir='+str(self.remote),*args).stdout.strip()

    def test_first_publication_contains_only_metadata(self):
        (self.package/'private.txt').write_text('must never be published')
        p = self.publish()
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(self.git('ls-tree','--name-only','master').splitlines(),['.SRCINFO','PKGBUILD'])
        self.assertNotIn(self.private_key,p.stdout+p.stderr)

    def test_unchanged_publication_creates_no_commit(self):
        self.assertEqual(self.publish().returncode,0)
        head = self.git('rev-parse','master')
        p = self.publish()
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertIn('unchanged',p.stdout)
        self.assertEqual(self.git('rev-parse','master'),head)

    def test_update_preserves_remote_history(self):
        self.assertEqual(self.publish().returncode,0)
        before = self.git('rev-parse','master')
        (self.package/'PKGBUILD').write_text('pkgname=kinectcam-git\npkgver=0.2.0.r2.gabcdef0\npkgrel=1\n')
        (self.package/'.SRCINFO').write_text('pkgbase = kinectcam-git\n\tpkgver = 0.2.0.r2.gabcdef0\n\tpkgrel = 1\npkgname = kinectcam-git\n')
        p = self.publish()
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(self.git('rev-parse','master^'),before)
        self.assertEqual(self.git('log','-1','--format=%s'),'Update kinectcam-git to 0.2.0.r2.gabcdef0-1')

    def test_missing_metadata_does_not_create_remote_branch(self):
        (self.package/'.SRCINFO').unlink()
        p = self.publish()
        self.assertNotEqual(p.returncode,0)
        self.assertIn('.SRCINFO',p.stderr)
        self.assertEqual(self.git('for-each-ref','--format=%(refname)'), '')

    def test_failed_clone_is_not_treated_as_empty_repo(self):
        self.remote.rename(self.root/'inaccessible.git')
        p = self.publish()
        self.assertNotEqual(p.returncode,0)
        self.assertNotIn('BEGIN OPENSSH PRIVATE KEY',p.stdout+p.stderr)

    def test_wrong_package_remote_is_rejected(self):
        self.env['AUR_REPO_URL'] = 'ssh://aur@aur.archlinux.org/ssd-flash-id-git.git'
        p = self.publish()
        self.assertNotEqual(p.returncode,0)
        self.assertIn('kinectcam-git',p.stderr)

if __name__ == '__main__':
    unittest.main()
