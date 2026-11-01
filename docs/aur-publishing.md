# Packaging and manual AUR publication

Public source: https://github.com/Crimson-Hawk/kinectcam, branch **master**.
Package: **kinectcam-git**, initially x86_64. License: **GPL-3.0-only**.

The AUR stores only the build recipe and metadata. Development and CI stay on
your self-hosted Gitea instance; users download source anonymously from the public
GitHub mirror over HTTPS. Gitea does not need to be reachable by package users.
Keep your existing private Gitea `origin` remote for development pushes.

## Repository settings

Configure these **repository secrets in Gitea**, not GitHub:

| Secret | Value |
| --- | --- |
| AUR_SSH_KEY | Complete, unencrypted dedicated AUR private key |
| AUR_REPO_URL | ssh://aur@aur.archlinux.org/kinectcam-git.git |
| GH_MIRROR_TOKEN | Fine-grained GitHub PAT for Crimson-Hawk/kinectcam with Contents: Read and write |

Add the corresponding SSH public key to your AUR account. Limit the GitHub PAT
to the single destination repository and select an expiration date. No GitHub
Actions or administration permissions are needed for the current tree, which
contains Gitea workflows under `.gitea/`, not GitHub workflows under `.github/`.
If GitHub Actions workflow files are added later, updating them may also require
the token's Workflows write permission. Do not put tokens or private keys in the
checkout. The AUR host key is public and pinned in
`packaging/aur/known_hosts`, verified against https://aur.archlinux.org/.

Enable Gitea Actions and register these runner labels:

- `ubuntu-latest`: x86_64; package validation runs inside `archlinux:base-devel`.
- `ubuntu-latest-arm64`: ARM64; metadata publication runs inside `node:24-bookworm`.

Copying metadata and pushing Git commits is architecture independent. Compiling
and testing the x86_64 package is done natively on x86_64. The runner labels are
labels registered in your Gitea runner; they do not provision hosted runners.

## Source sync

Push development changes to Gitea `master`. **Sync GitHub mirror** runs after
each master push and can also be started manually. It scans source/history for
secrets and then pushes the triggering commit to GitHub `master`. Set
`GH_MIRROR_TOKEN` before the first run; if it was missing when a push occurred,
run the sync workflow manually after adding it.

GitHub must be public so AUR users can clone it without credentials. Prefer an
empty destination when setting up the mirror. Only master and its reachable
history are synchronized; unrelated branches/tags are not published. Commit IDs
are preserved. Existing history includes the previously committed Gitea URLs;
moving the source host does not erase them or rewrite history.

The script uses HTTPS with a temporary credential helper reading the PAT from
the environment. It never embeds the token in a URL or Git configuration. There
is no force push: if GitHub has diverged, the sync fails for review rather than
overwriting commits. Avoid making independent commits on GitHub master.

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
3. Before AUR publication, the same validated commit is synchronized to GitHub
   using `GH_MIRROR_TOKEN`. If sync fails, AUR publication stops so the package
   cannot point at source that was never mirrored.
4. Only the final step receives the two AUR secrets. It validates the destination,
   creates temporary SSH files outside the checkout, and uses strict host-key
   checking. It clones AUR into a separate temporary directory and commits only
   `PKGBUILD` and `.SRCINFO`. Existing AUR history is preserved; identical metadata
   is a no-op. Failed authentication/network operations stop the job.

The AUR workflow remains manual; only the separate source mirror has an automatic
master-push trigger. Running validation locally never publishes. A Git
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
- Rotate `GH_MIRROR_TOKEN` in Gitea when the GitHub PAT expires. Keep the GitHub
  repository public and treat its master branch as a read-only mirror.
- If AUR rotates its host key, verify the new key against Arch's published
  fingerprint before changing `packaging/aur/known_hosts`; do not disable checking.
- A failed concurrent push is left as a failure, without force-pushing. Retry the
  manual workflow after reviewing any intervening AUR changes.

References: [AUR submission guidelines](https://wiki.archlinux.org/title/AUR_submission_guidelines),
[Gitea Actions differences](https://docs.gitea.com/usage/actions/comparison/),
[GitHub personal access tokens](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens).
