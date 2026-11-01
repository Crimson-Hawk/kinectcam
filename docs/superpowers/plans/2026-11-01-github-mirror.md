# GitHub mirror implementation plan

User wants Gitea to remain the development/CI host, with public source mirrored
to https://github.com/Crimson-Hawk/kinectcam.git, master, and used by AUR.
Use the Gitea repository secret GH_MIRROR_TOKEN, scoped to that GitHub repository
with Contents read/write. Existing AUR secrets remain unchanged.

Preserve history and push only master without force or ref deletion. This keeps
old source URLs in historical commits; it does not require Gitea to be publicly
reachable. No GitHub privacy/settings changes or history rewriting are requested.

Execute inline under the prior delegation of routine choices. Test mirror
behavior against local bare Git repositories before implementation. Finish with
the full suite and an independent read-only review. The previous user instruction
to commit conventionally and push to Gitea remains the integration preference.

- [x] Add real-Git tests for first sync, updates, unchanged sync, divergence
      refusal, master-only refs and credential handling; observe failure.
- [x] Implement scripts/mirror-github.sh using temporary GIT_ASKPASS and HTTPS,
      without putting the PAT in the remote URL or Git configuration.
- [x] Add push/manual master sync workflow and scan before publishing source.
      Manual AUR flow must sync successfully before publishing AUR metadata.
- [x] Switch PKGBUILD, .SRCINFO and CI source redirect to GitHub; update public
      setup docs and secret instructions. Keep private origin local.
- [ ] Run tests, metadata validation, source scans and independent review, then
      conventional commit/push to origin/master. Live GitHub authentication and
      mirroring depend on the user configuring GH_MIRROR_TOKEN in Gitea.

Verification: full CTest 7/7 passes; mirror tests 7/7 use real local Git and no
live PAT. Clean Arch package build and Debian/Node publishing preparation pass.
Source scan using the pinned/checksummed shared installer reports no leaks.
Independent review found one Important issue: inherited push.followTags could
publish annotated private tags. Strengthened test failed, --no-follow-tags fixed
it, and the full suite passed. Historical self-hosted URLs remain in Git history
as disclosed; no history rewrite or repository-privacy changes were performed.
