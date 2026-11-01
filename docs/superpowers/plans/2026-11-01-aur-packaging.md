# AUR Packaging Implementation Plan

> Execute inline using superpowers:executing-plans; test publishing behavior
> before implementing it. User delegated routine choices and chose GPL-3.0-only.

**Goal:** Build a valid kinectcam-git package and prepare manual Gitea → AUR sync.
**Architecture:** Arch x86_64 validation job followed by ARM64 SSH publishing.
**Tech Stack:** makepkg, CMake/CTest, Bash, Python unittest, Gitea Actions.
**Spec:** ../specs/2026-11-01-aur-packaging-design.md

## Global Constraints

Only master; AUR_SSH_KEY and AUR_REPO_URL are secrets; GPL-3.0-only;
anonymous HTTPS source; no automatic service/authentication changes or pushes.
No usable .git in workspace; use temporary Git fixtures and retain changes here.

## Review Focus

Empty remote versus failed authentication; unchanged package metadata;
untracked AUR data; moving source refs; secret material must stay outside Git.

### Task 1: License and package

- [x] Add chosen GPL text and install docs/license with CMake.
- [x] Add PKGBUILD with versioned Git source semantics and required dependencies.
- [x] Generate .SRCINFO with makepkg; verify staged files and recipe using namcap.

### Task 2: Tested publisher

Files: scripts/publish-aur.sh, tests/test_publish_aur.py, packaging/aur/known_hosts.
Interface: publish-aur.sh PACKAGE_DIR; AUR_REPO_URL, AUR_SSH_KEY from environment;
test harness may provide a local Git transport through Git URL rewriting.

- [x] Write tests using local bare remotes for first push, preserving history,
      unchanged no-op, missing files and failed clone; observe failure.
- [x] Implement isolated SSH key/known_hosts temp files and separate AUR clone.
- [x] Run publisher tests; ensure no private key appears in the outgoing tree.

### Task 3: CI and verification

- [x] Add manual workflow with Arch x86_64 build validation and ARM64 publication.
- [x] Pin dependency recipe, checkout action and Gitleaks release/checksums.
- [x] Document first source push, public/private keys, secrets, runner labels,
      local package validation and explicit manual publication.
- [x] Build real package locally and in a clean Arch container where available;
      run full CTest suite and independent final source review.
