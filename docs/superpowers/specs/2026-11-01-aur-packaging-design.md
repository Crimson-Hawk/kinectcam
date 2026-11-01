# AUR packaging and Gitea publication

User supplied public repository https://git.crimsonnet.org/CrimsonHawk/kinectcam.git,
branch master, and configured AUR_SSH_KEY and AUR_REPO_URL as repository secrets.
User chose GPL-3.0-only. Available runner labels: ubuntu-latest (x86_64) and
ubuntu-latest-arm64. Prior instruction delegates remaining implementation choices.

Create a kinectcam-git recipe under packaging/aur, with generated .SRCINFO,
pkgver derived from Git commits, explicit dependencies and staged CMake install.
Install docs and license, but do not enable services or change authentication.

Use a manual-only Gitea workflow. Validate the recipe and source on an x86_64
runner in an Arch base-devel container, including package build and existing
CTest suite. The installed libfreenect2 dependency is an AUR package, so CI
builds its pinned recipe first. Use a local source mirror for validation so CI
tests the triggering source commit, rather than a moving master revision.

Publish from ARM64 after validation. Verify .SRCINFO using makepkg, scan source
and outgoing metadata with a pinned/checksummed Gitleaks release, and inject the
AUR secrets only in the final publication step. Commit only PKGBUILD/.SRCINFO
to a separate AUR clone. Strict SSH host checking uses a committed public host
key verified against the HTTPS AUR homepage. Skip unchanged metadata; preserve
history and fail on fetch/authentication errors. An empty authenticated AUR clone
is the only first-publication case. Reject manual runs outside master.

Alternative: tagged stable package; deferred until releases are established.
Alternative: metadata-only ARM64 validation; unnecessary because the user also
has an x86_64 runner. Publishing metadata itself is architecture independent.

No remote push, credential retrieval, service change or automatic publication
is part of local preparation. The repository is currently public and empty.
