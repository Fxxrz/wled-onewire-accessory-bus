# OneWire Accessory Bus Usermod

Experimental WLED usermod for removable lamp shades and accessory modules identified through one additional OneWire data contact.

The validated path uses passive OneWire ROM chips such as DS2502. A draft smart-module protocol is included for future battery, sensor, and smart-shade controllers. The implementation is self-contained in this Usermod folder and does not require WLED core changes.

## Features

- Configurable OneWire GPIO and optional internal pullup.
- Dallas ROM CRC8 validation and stable multi-scan hotplug detection.
- Separate diagnostics for clean empty, stuck-low, invalid presence, CRC, overflow, and search-iteration-limit failures.
- Up to eight stable devices and eight persistent shade profiles.
- Profile name, LED count, ledmap, input mode, and WLED button slot keyed by ROM ID.
- Optional automatic profile apply and stable-clean-empty fallback for older shades.
- Conservative single-digital-bus validation, asynchronous result verification, and best-effort rollback.
- `/onewire` manager, WLED JSON state/info, MQTT status, and Home Assistant discovery.
- ESP32 OneWire worker for full bounded scans and smart transfers without blocking WLED's main loop.
- Fair scheduled smart descriptors/telemetry with per-field freshness and failure diagnostics.
- AudioReactive compatibility when both usermods are selected at build time.

## Documentation

- [Hardware and safety boundary](docs/hardware.md)
- [JSON API contract](docs/json-api.md)
- [MQTT and Home Assistant](docs/mqtt-homeassistant.md)
- [Smart accessory protocol draft v1](docs/smart-accessory-protocol.md)
- [Next beta candidate notes](docs/beta-release-notes.md)
- [Public release checklist](docs/public-release-checklist.md)
- [2026-07-16 audit disposition](docs/REVIEW_AUDIT_2026-07-16.md)
- [2026-07-17 re-audit disposition](docs/REVIEW_REAUDIT_2026-07-17.md)

## Build

Copy the folder to WLED's `usermods/` directory and add both desired usermods to `platformio_override.ini`:

```ini
[env:esp32dev_onewire_audio]
extends = env:esp32dev
custom_usermods =
  audioreactive
  onewire_accessory_bus
```

Then build the named environment:

```sh
pio run -e esp32dev_onewire_audio
```

The generic build keeps the GPIO runtime-configurable. A forced GPIO plus serial-debug build is diagnostic and should not be presented as the generic release binary.

## Hardware

OneWire data must idle at 3.3 V. The validated reference uses GPIO19, approximately 20 cm wiring, and a 4.7 kOhm external pullup to 3.3 V. Never use a 5-V pullup on a normal ESP32 GPIO. The internal pullup is useful for short passive-ID bring-up but has weaker and device-dependent margins.

See [hardware.md](docs/hardware.md) before designing a smart or battery module. Those variants remain conceptual until their schematic, protection, and measurements are public.

## Manager

The usermod serves:

```text
http://wled.local/onewire
```

It shows stable devices and full ROM IDs, performs a protected/rate-limited scan, assigns profiles, applies the unique active profile, and deletes profiles with confirmation. GPIO/pullup settings are changed in WLED's Usermod Setup and are reinitialized by the usermod at runtime.

## Passive Profile Example

The following synthetic ROM has a valid Dallas CRC8:

```json
{
  "OneWireAccessory": {
    "profileSet": {
      "rom": "09000000000000CC",
      "name": "Small shade",
      "ledCount": 96,
      "inputMode": "buttonLow",
      "buttonIndex": 0,
      "ledmap": -1
    }
  }
}
```

`inputMode` values:

- `inherit`: leave WLED's configured button behavior untouched.
- `disabled`: consume the selected button slot while the profile is active.
- `buttonLow`: active-low momentary button.
- `buttonHigh`: active-high momentary button; external pulldown is required where the MCU has no usable internal pulldown.
- `touch`: ESP32 touch input.
- `touchSwitch`: ESP32 touch switch.

Managed modes approximate WLED's native button handler. Use `inherit` when exact current WLED button semantics are required. Legacy stored `touch` profile values remain readable.

## Auto Apply And Fallback

`autoApplyProfile` is opt-in. A profile is selected only when exactly one stable detected ROM matches; ambiguous multiple matches do not change LEDs or input handling.

`fallbackLedCount` is also applied only with auto-apply enabled and only after the bus is stably known as electrically healthy and empty. Unknown devices or electrical bus faults do not trigger fallback. `fallbackLedmap` and profile `ledmap` use these values:

- `-2`: disable custom mapping.
- `-1`: preserve the currently active map, including across LED-bus rebuild.
- `0..N`: load the corresponding WLED map, bounded by the running build's API limit.

LED apply supports exactly one valid physical digital WLED bus. It checks start/length bounds and available heap, preserves the previous bus configuration, verifies WLED's asynchronous reinitialization, and attempts rollback on failure. A power loss between WLED's own config write and verification cannot be made fully transactional from a core-independent usermod.

## Smart Modules And Trust

Smart devices use the configurable private family code `0x7A` by default. Descriptor and telemetry parsers have separate field allowlists; framed CRC-protected telemetry is the default and unframed legacy telemetry is disabled.

`autoEnrollSmartShades` is off by default. Without explicit enrollment, a new smart module can be observed but cannot create a profile. `autoUpdateSmartProfiles` is a separate, default-off option for an existing profile and accepts only three matching descriptor observations, at most once per device and boot. `autoApplyProfile` is another independent opt-in before LED configuration can change.

CRC detects transfer errors but does not authenticate hardware. The complete frame, timing, ready/busy, UTF-8, duplicate, sequence, and compatibility rules are in [smart-accessory-protocol.md](docs/smart-accessory-protocol.md).

## Settings PIN

Scan, apply, learn, profile changes, and debug injection are treated as mutations. If WLED's Settings PIN is locked, place it at the JSON root or enter it transiently in the manager. The manager does not store it in browser storage.

WLED's settings unlock is device-global and time-limited, not a per-browser security session. It is suitable as a trusted-LAN change barrier, not as strong authorization against hostile LAN clients.

## Debug Device

With `debugEnabled` explicitly set, a CRC-valid synthetic module can exercise JSON/MQTT/HA without smart hardware:

```json
{
  "OneWireAccessory": {
    "debugInject": true,
    "rom": "7A00000000000058",
    "kind": 2,
    "name": "Debug battery",
    "batteryVoltage": 3.91,
    "batteryPercent": 74,
    "charging": true,
    "temperature": 24.5,
    "status": 0
  }
}
```

Debug injection is disabled by default and does not replace a hardware timing test.

## Tests

The Node suite compiles and executes native C++ protocol tests normally and under ASan/UBSan, runs negative release-packaging tests, then checks source-level integration contracts. Node.js 18+, Python 3, and a sanitizer-capable C++ compiler are required.

```sh
node usermods/onewire_accessory_bus/onewire_accessory_bus.test.js
pio run -e esp32dev_onewire_audio
```

The live smoke test requires an expected ROM so a disconnected bus cannot pass silently:

```sh
node usermods/onewire_accessory_bus/smoke-test.js http://wled.local 09000000000000CC
```

Use `OWAB_SETTINGS_PIN` when required. An intentional empty-bus test must explicitly set `OWAB_ALLOW_EMPTY=1`. The script waits for `scanCount` to advance and for the expected stable state instead of relying on a fixed delay.
