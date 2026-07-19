# OneWire Accessory Bus v0.1.0 Beta 2

Beta 2 is the first retained prerelease after a privacy-reviewed public-history reset. Withdrawn predecessor refs and assets are intentionally not retained. The supplied firmware remains an OTA/app image and must not be treated as a universal bare-device image.

## Highlights

- Complete bounded OneWire scans and smart transfers run on a dedicated ESP32 worker instead of blocking WLED's main loop.
- Eight ROM result slots remain available without the former 50-ms whole-scan contradiction.
- New profile targets survive an in-progress LED apply or rollback transaction.
- Button handling and asynchronous callbacks share one guarded state; lock/queue/maximum transaction diagnostics are public.
- Smart descriptors require three matching semantic observations. New enrollment and existing-profile updates are separate default-off options; an existing profile can update at most once per device and boot.
- Battery field ages are wrap-safe. Stale fields stop being republished to MQTT, and Home Assistant expiry follows the configured publish interval.
- JSON commands accept exactly one typed mutation per request. Config import validates broad integers before narrowing and reports invalid/skipped values.
- Native parser tests run normally and under ASan/UBSan; release tooling has executable negative tests.
- Release packaging validates a clean pinned WLED tree, byte-identical Usermod source, ESP32 image integrity, PlatformIO environment/toolchain, both linked Usermods, ELF/map, and nonempty SPDX evidence.
- GitHub CI binds the OTA app and release package to signed Sigstore provenance and binds the firmware to the generated SPDX SBOM.

## Compatibility Build

```text
WLED commit: e39118333d3d5a9c93286243b3aa227e5ee4a713
PlatformIO environment: esp32dev_onewire_audio
AudioReactive linked: yes
OneWire Accessory Bus linked: yes
WLED core changes: none
```

Final RAM/flash figures, public source commit, checksums, SBOM, metadata, and esptool validation are recorded in the release assets and provenance generated from the tagged source.

## Hardware Scope

The passive reference path has been tested with an ESP32, DS2502, GPIO19, an external 4.7 kOhm pullup to 3.3 V, approximately 20 cm wiring, profile hotplug, LED count/ledmap, button handling, AudioReactive, MQTT, and Home Assistant. An automated post-OTA HTTP smoke test confirmed a fresh scan, the expected profile/LED count, and zero CRC errors. Manual button and detach/reattach behavior passed on the preceding firmware candidate; repeating those two physical checks after the final OTA remains recommended. The final release-only source changes do not alter the Usermod firmware code.

Four/eight-device electrical HIL and an independent smart/battery slave are still pending. Smart and battery behavior therefore remains a protocol draft, not a production hardware claim.

## Deliberate Boundaries

- OTA/app binary only. No universal factory image is included.
- LED rollback is best-effort; power-loss atomic persistence would require a WLED core transaction hook.
- `inputMode=inherit` is the exact native WLED button path. Managed modes intentionally approximate it.
- WLED does not expose an unambiguous distinction between custom ledmap 0 and no custom map.
- A dedicated WLED `PinOwner` would require a core enum change.
- Retained Home Assistant discovery is not deleted on normal hot-unplug so entity identity/history survives reattachment.

## Installation

Use the clearly named `*_OTA-app.bin` only through WLED's firmware update flow on a compatible classic ESP32 4-MB installation. Back up `/cfg.json`, `/presets.json`, and ledmap data first. Do not write the app image to an arbitrary flash offset.
