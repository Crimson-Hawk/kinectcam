#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
[[ $# -le 1 ]] || { echo 'Usage: install-gitleaks.sh [DESTINATION_DIR]' >&2; exit 1; }
destination=${1:-/usr/local/bin}
case "$(uname -m)" in
    aarch64|arm64)
        asset=gitleaks_8.30.1_linux_arm64.tar.gz
        checksum=e4a487ee7ccd7d3a7f7ec08657610aa3606637dab924210b3aee62570fb4b080 ;;
    x86_64)
        asset=gitleaks_8.30.1_linux_x64.tar.gz
        checksum=551f6fc83ea457d62a0d98237cbad105af8d557003051f41f3e7ca7b3f2470eb ;;
    *) echo 'Unsupported Gitleaks architecture' >&2; exit 1 ;;
esac
work=$(mktemp -d "${TMPDIR:-/tmp}/kinectcam-gitleaks.XXXXXXXX")
trap 'rm -rf -- "$work"' EXIT
curl --fail --location --retry 3 \
    "https://github.com/gitleaks/gitleaks/releases/download/v8.30.1/$asset" -o "$work/gitleaks.tar.gz"
printf '%s  %s\n' "$checksum" "$work/gitleaks.tar.gz" | sha256sum --check --status
tar -xzf "$work/gitleaks.tar.gz" -C "$work" gitleaks
install -Dm755 "$work/gitleaks" "$destination/gitleaks"
