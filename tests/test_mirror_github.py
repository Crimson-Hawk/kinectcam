"""Exercise the public mirror boundary using real local Git repositories."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1]/'scripts/mirror-github.sh'
URL = 'https://github.com/Crimson-Hawk/kinectcam.git'
REAL_GIT = shutil.which('git')

def git(*args, **kwargs):
    return subprocess.run([REAL_GIT,*args],capture_output=True,text=True,check=True,**kwargs).stdout.strip()

class MirrorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.source, self.remote = self.root/'source', self.root/'remote.git'
        config = self.root/'gitconfig'
        config.write_text('[user]\n name = Test\n email = test@example.invalid\n[commit]\n gpgsign = false\n[url "'+self.remote.as_uri()+'"]\n insteadOf = '+URL+'\n')
        self.env = dict(os.environ,GIT_CONFIG_GLOBAL=str(config),GIT_CONFIG_NOSYSTEM='1',
                        GH_MIRROR_TOKEN='throwaway-test-credential')
        git('init','--quiet','--initial-branch=master',str(self.source),env=self.env)
        git('init','--quiet','--bare','--initial-branch=master',str(self.remote),env=self.env)
        self.commit('initial')

    def commit(self, text):
        (self.source/'README').write_text(text)
        git('add','README',cwd=self.source,env=self.env)
        git('commit','--quiet','-m',text,cwd=self.source,env=self.env)
        return git('rev-parse','HEAD',cwd=self.source,env=self.env)

    def mirror(self, ref='HEAD'):
        return subprocess.run(['bash',str(SCRIPT),ref],cwd=self.source,env=self.env,
                              text=True,capture_output=True,timeout=15)

    def remote_git(self, *args):
        return git('--git-dir='+str(self.remote),*args,env=self.env)

    def test_initial_sync_and_update_preserve_commit_ids(self):
        first = git('rev-parse','HEAD',cwd=self.source,env=self.env)
        p = self.mirror()
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(self.remote_git('rev-parse','master'),first)
        second = self.commit('update')
        p = self.mirror()
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(self.remote_git('rev-parse','master'),second)
        self.assertEqual(self.remote_git('rev-parse','master^'),first)

    def test_same_revision_is_a_noop(self):
        self.assertEqual(self.mirror().returncode,0)
        p = self.mirror()
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(self.remote_git('rev-list','--count','master'),'1')

    def test_private_branches_and_tags_are_not_published(self):
        git('branch','private-notes',cwd=self.source,env=self.env)
        git('tag','-a','private-tag','-m','private note',cwd=self.source,env=self.env)
        git('config','push.followTags','true',cwd=self.source,env=self.env)
        p = self.mirror()
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(self.remote_git('for-each-ref','--format=%(refname)'), 'refs/heads/master')

    def test_divergent_remote_is_not_overwritten(self):
        self.assertEqual(self.mirror().returncode,0)
        parent = git('rev-parse','HEAD',cwd=self.source,env=self.env)
        self.commit('github-side change')
        self.assertEqual(self.mirror().returncode,0)
        remote_head = self.remote_git('rev-parse','master')
        git('reset','--hard',parent,cwd=self.source,env=self.env)
        self.commit('gitea-side change')
        p = self.mirror()
        self.assertNotEqual(p.returncode,0)
        self.assertEqual(self.remote_git('rev-parse','master'),remote_head)

    def test_missing_token_does_not_publish(self):
        self.env.pop('GH_MIRROR_TOKEN')
        p = self.mirror()
        self.assertNotEqual(p.returncode,0)
        self.assertIn('GH_MIRROR_TOKEN',p.stderr)
        self.assertEqual(self.remote_git('for-each-ref','--format=%(refname)'), '')

    def test_private_feature_commit_is_rejected(self):
        git('checkout','--quiet','-b','private-notes',cwd=self.source,env=self.env)
        self.commit('private branch content')
        p = self.mirror()
        self.assertNotEqual(p.returncode,0)
        self.assertIn('master',p.stderr)
        self.assertEqual(self.remote_git('for-each-ref','--format=%(refname)'), '')

    def test_credentials_are_supplied_by_temporary_askpass_only(self):
        wrappers = self.root/'bin'
        wrappers.mkdir()
        record = self.root/'credential-record.json'
        wrapper = wrappers/'git'
        wrapper.write_text('#!'+sys.executable+'\n'+'''import json, os, subprocess, sys
if 'push' in sys.argv:
    helper = os.environ['GIT_ASKPASS']
    user = subprocess.check_output([helper,"Username for GitHub:"],text=True).strip()
    password = subprocess.check_output([helper,"Password for GitHub:"],text=True).strip()
    assert user == 'x-access-token'
    assert password == os.environ['GH_MIRROR_TOKEN']
    assert all(password not in arg for arg in sys.argv)
    with open(os.environ['MIRROR_TEST_RECORD'],'w') as f:
        json.dump({'helper':helper,'remote':sys.argv[-2]},f)
os.execv(os.environ['MIRROR_TEST_GIT'],[os.environ['MIRROR_TEST_GIT'],*sys.argv[1:]])
''')
        wrapper.chmod(0o755)
        self.env.update(PATH=str(wrappers)+':'+os.environ['PATH'],MIRROR_TEST_RECORD=str(record),MIRROR_TEST_GIT=REAL_GIT)
        p = self.mirror()
        self.assertEqual(p.returncode,0,p.stderr)
        data = json.loads(record.read_text())
        self.assertEqual(data['remote'],URL)
        self.assertFalse(Path(data['helper']).exists(),'temporary credentials must be removed')
        self.assertNotIn(self.env['GH_MIRROR_TOKEN'],p.stdout+p.stderr)
        self.assertNotIn(self.env['GH_MIRROR_TOKEN'],(self.source/'.git/config').read_text())

if __name__ == '__main__':
    unittest.main()
