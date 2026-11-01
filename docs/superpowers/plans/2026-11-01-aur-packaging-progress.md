# Execution ledger — plan: 2026-11-01-aur-packaging.md

Completed inline in the supplied workspace; .git is empty/read-only, so no real
Git commit or push was possible/performed. Temporary Git snapshots were used for
validation only. User chose GPL-3.0-only and confirmed both runner architectures.

Task 1 complete: GPL text and explicit README grant added. PKGBUILD and generated
.SRCINFO declare runtime/build/test dependencies, master Git source and staged
installation. Package installs executable, helper, service, rules, example config,
README, guides and license. It does not start services or modify authentication.

Task 2 complete: six publisher tests first failed without the implementation;
all now pass against real local bare Git repositories with throwaway keys.
Verified first push, history preservation, unchanged no-op, missing metadata,
failed clone and incorrect destination. Only two metadata files enter AUR Git.
The public SSH host key was checked against the HTTPS AUR homepage fingerprint.

Task 3 complete: manual workflow validates on ubuntu-latest/x86_64 in Arch, then
prepares/publishes from ubuntu-latest-arm64. Both configured values are secrets,
available only in the final publication step. Checkout/Gitleaks/dependency recipe
are pinned; Gitleaks archives also have pinned checksums. Outgoing metadata uses
the actual triggering Git revision's version.

Build findings fixed and verified:
- The pinned old libfreenect2 recipe needs CMake's explicit 3.5 compatibility
  floor with CMake 4. CI patches only its temporary dependency recipe.
- makepkg 7.1 discards ordinary global Git configuration. Clean build initially
  fetched the empty public repository; supplying MAKEPKG_GIT_CONFIG verified the
  intended local exact-commit source redirect and the package then built.
- Disabled unnecessary debug splitting and made CI reject namcap E diagnostics.
  Final package has no namcap errors. Warnings about shell-tool runtime needs
  and gcc-libs' implicit libgcc/libstdc++ satisfaction are reviewed and expected.
- Debian makepkg requires the pacman executable even for --printsrcinfo, but the
  makepkg package does not depend on it. The container preflight initially failed;
  adding pacman-package-manager made all metadata preparation/scanning pass.

Final independent read-only review found no Critical/Important findings.
Final clean Arch x86_64 build created the package and passed CTest 6/6. Local
CTest also passed 6/6. Publisher preparation ran successfully in node:24-bookworm
(on local x86_64; ARM64 runner execution remains a live CI check). Pinned Gitleaks
8.30.1 scanned history, checkout and outgoing metadata without findings. Generated
.SRCINFO matches the recipe. No real AUR authentication/publication was attempted.

Reviewer set aside tags-only AUR history because this destination is restricted
to AUR's master publication model; no additional branch behavior is required.
Live Gitea execution, AUR key permissions and remote acceptance require the user
to push the source to master and run the manual workflow.
