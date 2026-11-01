# Packaging and manual AUR publication

Source: https://git.crimsonnet.org/CrimsonHawk/kinectcam, branch **master**.
Package: **kinectcam-git**, initially x86_64. License: **GPL-3.0-only**.

The AUR stores only the build recipe and metadata. The source stays in public
Gitea and is downloaded anonymously over HTTPS when users build the package.
Your SSH development remote remains:

```text
ssh://gitea@git.crimsonnet.org:58852/CrimsonHawk/kinectcam.git
```

## Repository settings

The existing two **repository secrets** are sufficient:

| Secret | Value |
| --- | --- |
| AUR_SSH_KEY | Complete, unencrypted dedicated AUR private key |
| AUR_REPO_URL | ssh://aur@aur.archlinux.org/kinectcam-git.git |

Add the corresponding public key to your AUR account. The workflow never needs
your Gitea SSH key or an additional personal access token. Do not put private
keys in the checkout. The AUR host key is public and pinned in
`packaging/aur/known_hosts`, verified against https://aur.archlinux.org/.

Enable Gitea Actions and register these runner labels:

- `ubuntu-latest`: x86_64; package validation runs inside `archlinux:base-devel`.
- `ubuntu-latest-arm64`: ARM64; metadata publication runs inside `node:24-bookworm`.

Copying metadata and pushing Git commits is architecture independent. Compiling
and testing the x86_64 package is done natively on x86_64. The runner labels are
labels registered in your Gitea runner; they do not provision hosted runners.

## First source push

The Gitea repository was public and empty when checked during preparation.
Push this source tree to `master` before running its workflow. From a normal
checkout, inspect `git status`, commit the source/package/workflow files and push
to your supplied SSH remote. The Codex workspace's empty read-only `.git` is
not a usable repository, so no Git commits or remote pushes were made here.

The files to publish with the source include `LICENSE`, `packaging/aur/`,
`scripts/`, `.gitea/workflows/push-aur.yml` and the existing application/tests.
Do not commit `build/` or package build artifacts. If this directory has no Git
history outside the Codex sandbox either, initialize a new repository on master
and add the remote before your first commit/push.

## Local package build

On Arch x86_64, install `libfreenect2` from the AUR and the declared build/runtime
dependencies first. AUR helpers can resolve the AUR dependency; `makepkg -s`
alone only installs dependencies available through pacman repositories.

```sh
cd packaging/aur
makepkg --syncdeps --cleanbuild --check
makepkg --printsrcinfo > .SRCINFO
namcap PKGBUILD kinectcam-git-*.pkg.tar.zst
```

The build runs the application test suite and installs into a package staging
directory. It does not load a kernel module or capture a camera. Installation
includes the executable, `/usr/lib/kinectcam/setup-loopback.sh`, udev rules,
systemd unit, example config, README and license. Service activation and Howdy
configuration remain user actions.

After installing the package:

```sh
sudo systemctl enable --now kinectcam
```

If replacing the earlier manual `/usr` installation, pacman may report existing
unowned files. Review the conflicting file list, stop the old service, and move
aside only those previously installed kinectcam files before retrying. Avoid a
blanket `--overwrite '*'`.

## Manual workflow

Run **Actions → Push to AUR → Run workflow** on **master**.

1. An Arch x86_64 container installs build dependencies, builds the pinned
   `libfreenect2` AUR recipe, and builds/tests `kinectcam-git`. Source download is
   locally redirected to the triggering Git checkout so a moving master branch
   cannot change what is validated. The old dependency recipe receives a CI-only
   CMake 4 compatibility option; the outgoing kinectcam recipe is unchanged.
2. The ARM64 job checks out the same triggering commit. A pinned, checksum-checked
   Gitleaks binary scans source/history. `makepkg` validates committed `.SRCINFO`,
   then generates outgoing metadata with the actual Git-derived package version.
3. Only the final step receives the two AUR secrets. It validates the destination,
   creates temporary SSH files outside the checkout, and uses strict host-key
   checking. It clones AUR into a separate temporary directory and commits only
   `PKGBUILD` and `.SRCINFO`. Existing AUR history is preserved; identical metadata
   is a no-op. Failed authentication/network operations stop the job.

There is no push/tag trigger. Running validation locally never publishes. A Git
package does not require an AUR commit for every upstream source commit; users'
builds fetch master and compute `pkgver()` themselves. Run this workflow when
you deliberately want to publish/update its packaging metadata.

The source `PKGBUILD` starts at bootstrap version `0.2.0.r0`. Actual builds and
outgoing publication use `0.2.0.r<commit-count>.g<short-hash>`.

## Maintenance

- Regenerate `.SRCINFO` whenever committed recipe metadata changes.
- Review and update the pinned libfreenect2 recipe revision in
  `scripts/ci-arch-build.sh` when its AUR packaging changes.
- Review updates to the pinned checkout action and Gitleaks version/checksums.
- If AUR rotates its host key, verify the new key against Arch's published
  fingerprint before changing `packaging/aur/known_hosts`; do not disable checking.
- A failed concurrent push is left as a failure, without force-pushing. Retry the
  manual workflow after reviewing any intervening AUR changes.

References: [AUR submission guidelines](https://wiki.archlinux.org/title/AUR_submission_guidelines),
[Gitea Actions differences](https://docs.gitea.com/usage/actions/comparison/).
