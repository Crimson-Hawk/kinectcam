#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Run as root INSIDE an Arch base-devel container. Builds as an unprivileged
# user and never loads kernel modules or touches the host's camera service.
set -euo pipefail
[[ $# == 1 ]] || { echo 'Usage: ci-arch-build.sh SOURCE_GIT_CHECKOUT' >&2; exit 1; }
[[ $(id -u) == 0 && $(uname -m) == x86_64 ]] || {
    echo 'Run this script as root in an x86_64 Arch container' >&2; exit 1;
}
source_dir=$(cd "$1" && pwd)
work=$(mktemp -d /tmp/kinectcam-build.XXXXXXXX)
trap 'rm -rf -- "$work"' EXIT
useradd --create-home --shell /bin/bash builder
pacman -Syu --noconfirm --needed cmake git python openssh namcap \
    libusb glfw libjpeg-turbo ocl-icd opencl-headers \
    v4l2loopback-dkms v4l2loopback-utils

# Pin the AUR dependency recipe. Its old upstream CMakeLists needs the explicit
# CMake 4 compatibility floor; this adjustment is confined to the CI build.
git clone --quiet https://aur.archlinux.org/libfreenect2.git "$work/libfreenect2"
git -C "$work/libfreenect2" checkout --quiet bfd1356ad6af9463072c86916e4c386605a0a3fd
python - "$work/libfreenect2/PKGBUILD" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
text = p.read_text()
needle = '-DCMAKE_INSTALL_PREFIX=/usr \\\n'
assert text.count(needle) == 1
p.write_text(text.replace(needle, needle+'\t\t-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \\\n'))
PY
git -c safe.directory="$source_dir" -c safe.directory="$source_dir/.git" \
    clone --quiet --no-hardlinks "$source_dir" "$work/source"
mkdir "$work/package"
cp "$source_dir/packaging/aur/PKGBUILD" "$source_dir/packaging/aur/.SRCINFO" "$work/package/"
chown -R builder:builder "$work"
runuser -u builder -- bash -c 'cd "$1"; makepkg --noconfirm --cleanbuild' bash "$work/libfreenect2"
pacman -U --noconfirm "$work"/libfreenect2/libfreenect2-*.pkg.tar.zst

# Build the exact triggering checkout, even if public master advances mid-job.
runuser -u builder -- git config --file "$work/source-gitconfig" \
    "url.file://$work/source.insteadOf" 'https://github.com/Crimson-Hawk/kinectcam.git'
# makepkg 7.1 deliberately ignores user-global Git configuration. Supply its
# dedicated configuration file so URL rewriting survives source sanitization.
runuser -u builder -- env MAKEPKG_GIT_CONFIG="$work/source-gitconfig" \
    GIT_CONFIG_GLOBAL="$work/source-gitconfig" bash -c '
    set -euo pipefail
    cd "$1"
    makepkg --printsrcinfo > generated.SRCINFO
    diff -u .SRCINFO generated.SRCINFO
    makepkg --noconfirm --cleanbuild --check
    report=$(namcap PKGBUILD kinectcam-git-*.pkg.tar.zst)
    printf "%s\n" "$report"
    if [[ $report == *" E: "* ]]; then
        echo "namcap reported package errors" >&2
        exit 1
    fi
' bash "$work/package"
echo 'Arch x86_64 package build and checks passed.'
