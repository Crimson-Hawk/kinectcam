#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Publish an already-validated metadata directory. Never reads repository secrets
# from disk; the manual workflow supplies them only to this final step.
set -euo pipefail

fail() { printf 'publish-aur: %s\n' "$*" >&2; exit 1; }
[[ $# == 1 ]] || fail 'Usage: publish-aur.sh PACKAGE_DIR'
package_dir=$(cd "$1" && pwd)
for file in PKGBUILD .SRCINFO; do
    [[ -f $package_dir/$file ]] || fail "Missing $file"
done
[[ ${AUR_REPO_URL:-} == ssh://aur@aur.archlinux.org/kinectcam-git.git ]] ||
    fail 'AUR_REPO_URL must target ssh://aur@aur.archlinux.org/kinectcam-git.git'
[[ -n ${AUR_SSH_KEY:-} ]] || fail 'AUR_SSH_KEY is not configured'
pkgbase=$(awk '$1 == "pkgbase" && $2 == "=" {print $3; exit}' "$package_dir/.SRCINFO")
pkgver=$(awk '$1 == "pkgver" && $2 == "=" {print $3; exit}' "$package_dir/.SRCINFO")
pkgrel=$(awk '$1 == "pkgrel" && $2 == "=" {print $3; exit}' "$package_dir/.SRCINFO")
[[ $pkgbase == kinectcam-git && -n $pkgver && -n $pkgrel ]] || fail 'Invalid kinectcam-git .SRCINFO metadata'

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
umask 077
work=$(mktemp -d "${TMPDIR:-/tmp}/kinectcam-aur.XXXXXXXX")
trap 'rm -rf -- "$work"' EXIT
printf '%s\n' "$AUR_SSH_KEY" | tr -d '\r' > "$work/key"
unset AUR_SSH_KEY
ssh-keygen -y -P '' -f "$work/key" >/dev/null 2>&1 || fail 'AUR_SSH_KEY must be a valid unencrypted SSH private key'
cp "$script_dir/../packaging/aur/known_hosts" "$work/known_hosts"
printf -v GIT_SSH_COMMAND '%q ' ssh -F /dev/null -i "$work/key" \
    -o BatchMode=yes -o IdentitiesOnly=yes -o IdentityAgent=none \
    -o StrictHostKeyChecking=yes -o HostKeyAlgorithms=ssh-ed25519 \
    -o "UserKnownHostsFile=$work/known_hosts" -o GlobalKnownHostsFile=/dev/null
export GIT_SSH_COMMAND GIT_TERMINAL_PROMPT=0

# A failed clone is fatal. Only a successful clone with no remote refs means
# an authenticated, empty AUR repository ready for its first commit.
git clone --quiet "$AUR_REPO_URL" "$work/aur"
cd "$work/aur"
if git show-ref --verify --quiet refs/remotes/origin/master; then
    git checkout --quiet -B master origin/master
elif [[ -z $(git for-each-ref --format='%(refname)' refs/remotes/origin/) ]]; then
    git checkout --quiet --orphan master
else
    fail 'Remote has history but no master branch; refusing to replace it'
fi
git config user.name 'CrimsonHawk AUR CI'
git config user.email 'ci@noreply.crimsonnet.org'
git rm --quiet -r --ignore-unmatch -- .
cp "$package_dir/PKGBUILD" "$package_dir/.SRCINFO" .
git add -- PKGBUILD .SRCINFO
if git diff --cached --quiet; then
    printf 'AUR metadata unchanged; nothing to publish.\n'
    exit 0
fi
git commit --quiet -m "Update kinectcam-git to $pkgver-$pkgrel"
git push --quiet origin HEAD:refs/heads/master
printf 'Published kinectcam-git %s-%s to AUR.\n' "$pkgver" "$pkgrel"
