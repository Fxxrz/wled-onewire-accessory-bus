# Public Export Manifest

This repository is the privacy-reviewed standalone export of the OneWire Accessory Bus Usermod. It intentionally does not contain the private WLED development history or private remotes.

## Source Relationship

- Original integration base: WLED `ff97ad5fe07d8182605cc561cadbe5013dbd5595`.
- Current-upstream compatibility build: WLED `e39118333d3d5a9c93286243b3aa227e5ee4a713`.
- Custom WLED core changes: none.
- Complete modification surface: `usermods/onewire_accessory_bus`.
- The public repository starts at one privacy-reviewed root commit and retains no predecessor release refs.
- The audited and re-audited remediation targets `onewire-accessory-bus-v0.1.0-beta.2`.

The public source needed for a candidate firmware is the pinned unmodified WLED tree plus this repository's Usermod folder and build definition. A final release manifest must replace branch-relative references with the full public commit/tag SHA used for the artifacts.

## Included

- EUPL license and direct third-party notices.
- Public README, security/contribution/change documentation, and exact release-build guidance.
- Release-ready hardware cutouts and clearly labeled illustrative UI mockups.
- Usermod implementation, native protocol tests, source contracts, smoke test, and documentation.
- Generic ESP32 AudioReactive + OneWire PlatformIO environment.
- GitHub/Forgejo CI definitions and release/SBOM policy.

## Excluded

- Private repository history and remotes.
- Local PlatformIO override files and build caches.
- Internal iterative review logs other than the sanitized final audit disposition.
- Live device dumps and private IPs, domains, paths, MAC addresses, credentials, and real OneWire ROM IDs.
- Original HEIC/JPEG sources and intermediate image-editing files.
- Candidate firmware binaries in Git history; beta 2 binaries are release assets only after the release gate completes.

## Privacy And Integrity Gate

Review the complete tree, hidden files, Git history, tags, author metadata, image metadata, generated archives, and release assets. A worktree-only text scan is not sufficient. Maintainer commits and annotated tags use GitHub Noreply email metadata, enforced in CI. Any privacy incident that requires rewriting published refs must be explicitly approved, backed up privately, disclosed, and followed by a complete public reachability audit.
