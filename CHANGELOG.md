# Changelog

All notable public changes are recorded here. Published tags are immutable; fixes use a new version.

## Unreleased

- No changes yet.

## 0.1.0-beta.2

- Republished from one privacy-reviewed root commit before community announcement; withdrawn predecessor refs and assets are not retained.
- Added CI enforcement that rejects non-Noreply commit and annotated-tag email metadata.
- Moved complete ESP32 ROM searches and bounded smart transfers to a dedicated OneWire worker so eight-device scans and maximum smart frames do not block WLED's main loop.
- Preserved newer LED apply requests while an earlier apply/rollback transaction is being verified.
- Closed the button/profile race and made post-setup state-lock failures explicit and observable.
- Added wrap-safe per-field telemetry freshness; stale values are no longer republished as fresh MQTT data.
- Added three-observation smart descriptor confirmation and a separate default-off, once-per-boot existing-profile update policy.
- Hardened JSON/config typing, command results, MQTT discovery transitions, runtime reinitialization diagnostics, and config import counters.
- Added native parser length/CRC/partial-frame coverage under ASan/UBSan and executable negative release-packaging tests.
- Bound release packaging to a pinned clean WLED tree, exact public Usermod source, ESP32 image validation, PlatformIO metadata, linker evidence, and a nonempty SPDX SBOM.
- Pinned CI runner/tool/action inputs and documented the 2026-07-17 re-audit disposition.
