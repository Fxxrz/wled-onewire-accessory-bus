# Public Release Checklist

Use this gate for every public beta. A previous green binary is not a substitute for rerunning the gate from the final tagged source.

## Source And History

- [ ] Use a new version and annotated/signed tag; never move an existing public tag.
- [ ] Record the exact public WLED base SHA and public usermod/patch SHA.
- [ ] Publish all modified source needed to rebuild the distributed firmware, not only a private integration commit.
- [ ] Version the exact PlatformIO environment, defines, dependency versions, and packaging script.
- [ ] Confirm the public default branch and release tag contain the same intended implementation.
- [ ] Review root files, hidden files, Git history, annotated tags, author/committer email, image metadata, generated archives, and release text for private data.
- [ ] Require privacy-preserving Noreply metadata before the first public push. If a privacy incident reaches public refs, stop publication, back up privately, obtain explicit approval, reset every reachable ref, and repeat release verification.

## Code Gates

- [ ] `git show --check --format= HEAD` passes on the exact release commit.
- [ ] Native C++ CRC/parser tests execute successfully both normally and under ASan/UBSan.
- [ ] Negative packaging tests reject non-ESP binaries and wrong metadata, SBOM, and WLED revisions.
- [ ] Node source-contract tests execute successfully.
- [ ] Integration build links both `onewire_accessory_bus` and `audioreactive`.
- [ ] Compile against the pinned release base and a current WLED upstream snapshot in clean worktrees.
- [ ] Confirm the diff against WLED contains only `usermods/onewire_accessory_bus`, unless a separately approved core change is documented.
- [ ] Record RAM and flash use and compare it with the previous release.
- [ ] CI starts from a clean checkout and fails on test, compile, packaging, notice, secret-scan, or hash errors.
- [ ] Complete 1/3/4/8-ROM worker scans are verified in executable tests or HIL; do not infer capacity from a short scan budget.
- [ ] A new apply request arriving during apply/rollback is retained and executed after verification.

## Hardware Gate

- [ ] Back up the target WLED configuration and verify recovery access.
- [ ] Run the smoke test with an expected CRC-valid ROM, or explicitly set `OWAB_ALLOW_EMPTY=1` for an empty-bus test.
- [ ] Test repeated boot with accessory attached and absent.
- [ ] Test hotplug/contact bounce, clean empty, bus-low, bad CRC, and recovery.
- [ ] Test one and multiple ROMs, including ambiguous matching profiles.
- [ ] Test same-length ledmap changes and `ledmap=-1` preservation.
- [ ] Test apply failure/rollback under constrained heap where practical.
- [ ] Exercise AudioReactive, HTTP, MQTT, and LED effects during repeated scans.
- [ ] Test a faulty first smart slave plus another healthy slave before claiming smart multi-device support.

## API And Integration Gate

- [ ] Validate Settings-PIN locked/unlocked behavior and manual-scan rate limiting.
- [ ] Test profile create/update/delete, duplicate rejection, capacity overflow, fallback, and runtime pin reinit.
- [ ] Test maximum MQTT state without ArduinoJson overflow.
- [ ] Verify Home Assistant discovery retry, availability, entity classes, unique IDs, and hot-unplug expiry.
- [ ] Test the `/onewire` manager on phone and desktop with long names/ROMs and failure responses.

## Firmware Assets

- [ ] Name PlatformIO `firmware.bin` clearly as an OTA/app image.
- [ ] Publish a factory/merged image only when generated from the same final build with documented board, chip, flash size, partition layout, offsets, and recovery.
- [ ] Never market an app-only image as universal ready-to-flash firmware.
- [ ] Build in an empty staging directory and reject duplicate ZIP paths.
- [ ] Include exactly one provenance manifest in each archive.
- [ ] Include license, third-party notices, SBOM, public source pointers, and modification date.
- [ ] Hash every binary, archive, manifest, notice, and SBOM after final packaging.
- [ ] Validate the app image header, checksum, validation hash, and ESP32 chip ID with the pinned esptool.
- [ ] Bind firmware, ELF, map, PlatformIO metadata, SPDX SBOM, WLED SHA, Usermod SHA, and verified source remotes in provenance.
- [ ] Sign the tag and checksum/attestation with a separately protected key or CI identity when available.
- [ ] Download public assets again and verify size, hash, archive contents, and embedded manifests.

## Repository Governance

- [ ] Protect `main` and release tags against force-push/deletion.
- [ ] Require CI status checks and review for public release changes.
- [ ] Add `SECURITY.md`, support/compatibility guidance, changelog, and bug-report template.
- [ ] Verify Forgejo and GitHub mirror SHAs and release assets after publication.

## Images And Claims

- [ ] Label AI-generated UI images explicitly as illustrative mockups.
- [ ] Do not use a mockup as evidence of a live hardware/UI test.
- [ ] Prefer a real, manually anonymized screenshot for release QA.
- [ ] Remove private IPs, SSIDs, real ROMs, QR codes, labels, serials, domains, paths, and location clues.
- [ ] Mark smart/battery hardware as conceptual until schematic, BOM, protection, and measurements are published.

## Suggested Commands

```sh
node usermods/onewire_accessory_bus/onewire_accessory_bus.test.js
pio run -e esp32dev_onewire_audio
node usermods/onewire_accessory_bus/smoke-test.js http://wled.local 09000000000000CC
git show --check --format= HEAD
python3 tools/package_release.test.py
```
