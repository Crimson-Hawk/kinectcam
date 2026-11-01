#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Sync only the selected master commit. Never embed credentials in a URL/config,
# force a push, delete refs, or publish unrelated private branches/tags.
set -euo pipefail
[[ $# -le 1 ]] || { echo 'Usage: mirror-github.sh [COMMIT]' >&2; exit 1; }
[[ -n ${GH_MIRROR_TOKEN:-} ]] || { echo 'GH_MIRROR_TOKEN is not configured' >&2; exit 1; }
commit=$(git rev-parse --verify --end-of-options "${1:-HEAD}^{commit}")
on_master=false
for branch in refs/heads/master refs/remotes/origin/master; do
    if git show-ref --verify --quiet "$branch" && git merge-base --is-ancestor "$commit" "$branch"; then
        on_master=true
        break
    fi
done
$on_master || { echo 'Refusing to mirror a commit outside master history' >&2; exit 1; }
umask 077
work=$(mktemp -d "${TMPDIR:-/tmp}/kinectcam-mirror.XXXXXXXX")
trap 'rm -rf -- "$work"' EXIT
cat > "$work/askpass" <<'SH'
#!/bin/sh
case "$1" in
    *Username*) printf '%s\n' 'x-access-token' ;;
    *Password*) printf '%s\n' "$GH_MIRROR_TOKEN" ;;
    *) exit 1 ;;
esac
SH
chmod 700 "$work/askpass"
export GIT_ASKPASS="$work/askpass" GIT_TERMINAL_PROMPT=0 LC_ALL=C
git -c credential.helper= -c http.extraHeader= push --no-follow-tags \
    'https://github.com/Crimson-Hawk/kinectcam.git' "$commit:refs/heads/master"
printf 'GitHub master synchronized to %s.\n' "$commit"
