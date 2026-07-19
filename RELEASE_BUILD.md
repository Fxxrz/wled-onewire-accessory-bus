# Release Build Definition

This file defines the `v0.1.0-beta.2` source/build inputs after the pre-release privacy reset.

## Pinned Inputs

| Input | Candidate value |
|---|---|
| WLED repository | `https://github.com/Aircoookie/WLED.git` |
| WLED commit | `e39118333d3d5a9c93286243b3aa227e5ee4a713` |
| Usermod source | final public commit/tag of this repository; record full SHA in the release manifest |
| PlatformIO Core | `6.1.19` |
| Platform | `platform-espressif32 2024.6.0` from WLED's pinned URL |
| Arduino ESP32 framework | `2.0.18+sha.8ba0dc0` |
| Xtensa toolchain | `8.4.0+2021r2-patch5` |
| esptool.py | `4.7.4` |
| OneWire | exactly `2.3.8` |
| CI OS | `ubuntu-24.04` |
| CI Node.js | `20.19.5` |
| CI Python | `3.11.9` |
| Environment | `esp32dev_onewire_audio` |

The WLED commit is unmodified. The complete project modification is the copied `usermods/onewire_accessory_bus` folder.

## Clean Build Procedure

```sh
git clone https://github.com/Aircoookie/WLED.git WLED
git -C WLED checkout e39118333d3d5a9c93286243b3aa227e5ee4a713
cp -R usermod-export/usermods/onewire_accessory_bus WLED/usermods/
cp usermod-export/examples/platformio_override.example.ini WLED/platformio_override.ini
cd WLED
npm ci
npm run build
pio run -e esp32dev_onewire_audio
```

The final public CI performs this integration from separate clean checkouts. It records PlatformIO JSON metadata before the firmware build, generates an SPDX JSON SBOM from the assembled source/build tree, validates the ESP32 image through esptool, and packages only when the WLED revision, public Usermod bytes, environment, toolchain, linked Usermods, metadata, SBOM, ELF, map, and firmware all agree. GitHub CI then creates Sigstore-backed provenance and SBOM attestations for the OTA app and release package through its short-lived workflow identity.

GitHub's `ubuntu-24.04` job is the authoritative public release build and retained evidence source. Forgejo repeats the same source, protocol, image, SBOM, and packaging checks on the private `macos-latest` runner as an independent portability signal. The Forgejo job verifies its generated evidence and checksums in place instead of uploading a redundant artifact through the runner's artifact service; it is not the source of GitHub attestations.

## Source Correspondence

For a distributed candidate binary, corresponding public source is:

1. the exact pinned WLED commit above;
2. the exact public tag/commit of this repository;
3. `examples/platformio_override.example.ini` copied as `platformio_override.ini`;
4. the tool/dependency versions in this file and the final generated dependency inventory.

Do not reference a private integration commit as the only source pointer.

## Artifact Types

- `.pio/build/esp32dev_onewire_audio/firmware.bin` is an ESP32 application image intended for OTA/manual update from a compatible existing WLED installation.
- It is not a universal bare-device image and must not be advertised as one.
- A factory image may be released only after a same-build merged image and exact board/chip/4-MB-flash/partition/offset/recovery matrix have been generated and tested on erased hardware.
- Forced-GPIO and serial-debug builds are diagnostic variants and must be named as such.

## Packaging Gate

Use `tools/package_release.py` with a new empty staging directory, the exact esptool path, and each public source remote. Include one provenance manifest, EUPL license, `THIRD_PARTY_NOTICES.md`, generated SBOM/dependency metadata, exact verified source links, and hashes for every final file. Reject duplicate archive paths. After upload, download every asset and compare hashes and archive manifests before announcing the release.

For the GitHub mirror, verify the published OTA app against the repository identity with `gh attestation verify <OTA-app.bin> -R Fxxrz/wled-onewire-accessory-bus`. This complements the immutable release checksums; it does not turn the OTA/app image into a factory image.
