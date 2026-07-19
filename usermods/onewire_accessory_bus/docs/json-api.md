# JSON API Contract

The usermod extends WLED's existing JSON API. It does not add a separate HTTP API version or custom authentication layer.

## Endpoints

- `GET /json/state`: `OneWireAccessory` runtime state.
- `POST /json/state`: normal WLED state update containing OneWire commands.
- `GET /json/info`: `onewire` diagnostics and counters.
- WLED's Usermod settings/config endpoints persist the `OneWireAccessory` configuration object.

Use `Content-Type: application/json` for POST requests. WLED commonly returns HTTP 200 even when a usermod command is rejected; clients MUST inspect `commandCode` and `commandMessage` in the returned state.

## Runtime State

`GET /json/state` contains:

```json
{
  "OneWireAccessory": {
    "enabled": true,
    "activePin": 19,
    "internalPullup": false,
    "allowLegacyTelemetry": false,
    "autoEnrollSmartShades": false,
    "autoUpdateSmartProfiles": false,
    "rebootRequired": false,
    "busStateKnown": true,
    "busHealth": "devices",
    "deviceCount": 1,
    "deviceOverflow": false,
    "profileConfigOverflow": false,
    "profileConfigDuplicates": 0,
    "configInvalidValues": 0,
    "configSkippedProfiles": 0,
    "wireWorkerBusy": false,
    "wireQueueFailures": 0,
    "maxWireTransactionUs": 0,
    "stateLockFailures": 0,
    "profileCount": 1,
    "activeProfileIndex": 0,
    "activeProfileMatches": 1,
    "activeProfile": "Small shade",
    "applyStatus": "applied",
    "inputStatus": "input inherited",
    "commandCode": "ok",
    "commandMessage": "idle",
    "fallbackLedCount": 96,
    "fallbackLedmap": -2,
    "writeLocked": false,
    "limits": {
      "maxLedsPerBus": 2048,
      "maxButtons": 4,
      "maxLedmaps": 16,
      "maxProfiles": 8
    },
    "devices": ["09000000000000CC"],
    "deviceDetails": [],
    "profiles": []
  }
}
```

The numeric limits are examples; clients MUST use the values returned by the running build.

`busHealth` is one of `unknown`, `empty_clean`, `devices`, `stuck_low`, `presence_invalid`, `crc_error`, `overflow`, or `search_limit`. Fallback is eligible only for a stable `empty_clean` state.

Each `profiles` entry contains `rom`, `name`, `ledCount`, `ledmap`, `touch`, `inputMode`, `buttonIndex`, and `active`.

Each `deviceDetails` entry contains `rom`, numeric/string family and kind fields, `profileIndex`, and profile data when matched. Smart devices additionally expose descriptor/telemetry validity, age, failure counts, sequence, descriptor stability/profile-mismatch state, and `lastError`. The nested `battery` object contains only fields actually received; each value has its own `...AgeMs` and `...Stale` field.

`GET /json/info` exposes hardware/configured pin state, `scanCount`, `crcErrors`, `applyCount`, bus health, stable-scan count, overflow/config diagnostics, worker busy/queue/maximum-transaction diagnostics, lock failures, and the same command/apply status information.

## Mutating Commands And PIN

Manual scan, apply, learn, profile changes, profile clearing, and debug injection are mutating commands. If WLED's Settings PIN is configured and currently locked, put `pin` at the JSON root:

```json
{
  "pin": "1234",
  "OneWireAccessory": {
    "scan": true
  }
}
```

This uses WLED's normal settings-PIN helper. Its unlock flag is device-global and time-limited, not an isolated per-client HTTP session. It is an accidental-change barrier on a trusted LAN, not strong multi-user authorization.

Manual scans are limited to one request per second. On ESP32, a scan only enqueues a full bounded enumeration on the dedicated OneWire worker; the main loop later stabilizes the result. Smart descriptor/telemetry servicing uses the same single-job worker and is scheduled separately.

## Create Or Update Profiles

The synthetic example ROM below has a valid Dallas CRC8.

```json
{
  "OneWireAccessory": {
    "profileSet": {
      "rom": "09000000000000CC",
      "name": "Small shade",
      "ledCount": 96,
      "inputMode": "inherit",
      "buttonIndex": 0,
      "ledmap": -1
    }
  }
}
```

`profileSet` may also be an array. The complete array is preflighted for valid values, duplicate ROMs, and remaining profile capacity before any item is changed.

Validation rules:

- `rom`: exactly 16 hexadecimal characters and valid Dallas CRC8.
- `name`: valid UTF-8, maximum 32 bytes, no control characters.
- `ledCount`: integer `1..limits.maxLedsPerBus`; required for a new profile.
- `buttonIndex`: integer `0..limits.maxButtons-1`.
- `ledmap`: integer `-2..limits.maxLedmaps-1`.
- `touch`: boolean when supplied.
- `inputMode`: `inherit`, `disabled`, `buttonLow`, `buttonHigh`, `touch`, or `touchSwitch`.

Omitted fields preserve an existing profile value. Invalid values are rejected rather than clamped.

## Learn, Delete, Clear, And Apply

Learn the only stable device, or supply its ROM explicitly:

```json
{
  "OneWireAccessory": {
    "learn": true,
    "rom": "09000000000000CC",
    "name": "Small shade",
    "ledCount": 96,
    "inputMode": "buttonLow",
    "buttonIndex": 0,
    "ledmap": -1
  }
}
```

Delete one profile:

```json
{"OneWireAccessory":{"profileDelete":"09000000000000CC"}}
```

Clear every profile or apply the uniquely active profile:

```json
{"OneWireAccessory":{"clearProfiles":true}}
```

```json
{"OneWireAccessory":{"apply":true}}
```

## Ledmap Semantics

- `-2`: disable custom mapping by requesting WLED's sentinel map slot.
- `-1`: preserve the currently active map, including across a bus-length rebuild.
- `0`: apply `/ledmap.json`.
- Positive values: apply `/ledmapN.json` up to the runtime `maxLedmaps` limit.

## Command Errors

Exactly one mutation command is accepted per request. Stable machine-readable codes include `ok`, `no_command`, `multiple_commands`, `invalid_command_type`, `settings_pin_required`, `rate_limited`, `invalid_rom`, `invalid_name`, `invalid_led_count`, `missing_led_count`, `invalid_button`, `invalid_ledmap`, `invalid_touch`, `invalid_input_mode`, `duplicate_rom`, `too_many_profiles`, `profile_not_found`, `profile_storage_full`, and `state_unavailable`.

`applyStatus` describes the asynchronous LED-bus operation. Scheduling success is not final success: the usermod verifies the rebuilt digital bus and ledmap, then reports `applied`, a rollback result, or a timeout/failure state.
