# Smart Accessory Protocol Draft v1

This document specifies the experimental protocol implemented by the WLED OneWire Accessory Bus usermod. It is a draft until at least one independent slave implementation has passed interoperability and fault-injection tests.

## Transport And Identity

- WLED/ESP32 is always the OneWire master; accessories are slaves.
- Standard-speed OneWire is used. Overdrive and parasite-powered smart modules are not supported.
- The default smart family code is the project-private value `0x7A`. It is configurable to avoid collisions and is not presented as an officially registered family code.
- ROM strings contain the eight ROM bytes in wire/library order as 16 uppercase hexadecimal characters. Byte 0 is the family code and byte 7 is the Dallas CRC8.
- The maximum descriptor payload is 96 bytes and the maximum telemetry payload is 48 bytes. A length of zero is invalid.

After selecting a ROM and writing a command, the master waits 1 ms before reading the response length. A slave that is not ready returns `0xFF`; the master waits another 1 ms and retries. At most five busy bytes are accepted, giving the slave up to approximately 5 ms. The first non-`0xFF` byte is the payload length and must be `1..96`, followed immediately by that many payload bytes.

## Commands

| Command | Value | Response |
|---|---:|---|
| Read descriptor | `0xA1` | `length`, then a `WLA1` frame |
| Read telemetry | `0xB1` | `length`, then a `WLT1` frame |

The master resets and selects the ROM for every command. A failed transfer is discarded in full; partial state is never committed.

## Integer And CRC Encoding

- Multi-byte integers are little-endian.
- Signed temperature uses two's-complement `int16` in tenths of a degree Celsius.
- Frame CRC is CRC-16/CCITT-FALSE: polynomial `0x1021`, initial value `0xFFFF`, no reflection, XOR-out `0x0000`.
- The numeric CRC is appended low byte first, despite the non-reflected CRC calculation.
- Test vector: ASCII `123456789` produces `0x29B1`, transmitted as bytes `B1 29`.

The OneWire ROM CRC8 remains the Dallas/Maxim CRC8 handled by the OneWire library and is independent of the frame CRC16.

## Descriptor Frame

```text
57 4C 41 31  version  tlv_length  tlv_payload  crc16_le
 W  L  A  1
```

- `version` MUST be `1`.
- `tlv_length` MUST exactly match the TLV bytes in the frame.
- Total frame length MUST be `tlv_length + 8`.

Descriptor TLVs:

| Type | Meaning | Length | Rules |
|---|---|---:|---|
| `0x01` | Module class | 1 | Required; `1` smart shade, `2` battery, `3` sensor |
| `0x02` | LED count | 2 | Required for smart shade; `1..MAX_LEDS_PER_BUS` |
| `0x03` | Touch present | 1 | Optional; exactly `0` or `1` |
| `0x04` | Display name | 1..32 | Optional valid UTF-8 text |
| `0x20` | Hardware version | 1..16 | Optional valid UTF-8 text |
| `0x21` | Firmware version | 1..16 | Optional valid UTF-8 text |

Battery telemetry TLVs are forbidden in descriptors. A duplicate known TLV invalidates the entire descriptor. Unknown TLVs are ignored for forward compatibility. Text MUST be well-formed UTF-8 and MUST NOT contain NUL, ASCII control bytes, or `DEL`.

## Telemetry Frame

```text
57 4C 54 31  version  frame_type  sequence  tlv_length  tlv_payload  crc16_le
 W  L  T  1
```

- `version` MUST be `1`.
- `frame_type` MUST be `1` for telemetry.
- Total frame length MUST be `tlv_length + 10`.
- At least one recognized telemetry TLV is required.

Telemetry TLVs:

| Type | Meaning | Length | Rules |
|---|---|---:|---|
| `0x10` | Battery voltage | 2 | Unsigned millivolts |
| `0x11` | Battery percentage | 1 | `0..100` |
| `0x12` | Charging | 1 | Exactly `0` or `1` |
| `0x13` | Temperature | 2 | Signed deci-degrees Celsius |
| `0x14` | Status/error bitfield | 2 | Project-specific unsigned bits |

Descriptor fields are forbidden in telemetry. A duplicate known TLV invalidates the complete frame. Unknown TLVs are ignored. Each field has its own validity and timestamp; omitting one field does not refresh or synthesize it.

The master rejects an exact duplicate of the last accepted sequence number for that device. Sequence wraps naturally from `255` to `0`. Draft v1 deliberately accepts non-duplicate out-of-order values after reconnects or slave resets; sequence is freshness diagnostics, not authentication.

## Legacy Telemetry

Early prototypes may send plain telemetry TLVs without `WLT1`, sequence, or CRC. This mode is disabled by default through `allowLegacyTelemetry=false`. If explicitly enabled, the same telemetry-only allowlist and validation apply. Legacy telemetry can never change class, LED count, touch capability, names, or versions.

## Master Scheduling Defaults

- ROM search: configurable, 3000 ms default.
- Fair smart-service slot: one command every 250 ms.
- Valid descriptor refresh: 60 seconds.
- Invalid descriptor retry: 10 seconds.
- Descriptor stale threshold: 5 minutes.
- Telemetry refresh: 10 seconds.
- Telemetry stale threshold: 60 seconds.

The scheduler is round-robin across stable devices. On ESP32, ROM enumeration and smart transfers execute serially on a dedicated FreeRTOS worker pinned to core 0; the WLED main loop only enqueues jobs and commits completed results. The queue holds one job, and runtime pin reinitialization waits for the current generation to finish. Smart commands are not performed inside ROM enumeration, so a bad first device cannot consume every later device's slot.

## Trust And Persistence

A valid CRC proves transmission integrity, not module identity or authorization. Physical bus participants are trusted at the transport level.

`autoEnrollSmartShades` is disabled by default. With it disabled, a new smart shade is visible but cannot create a persistent profile. A previously learned profile acts as explicit enrollment but remains configuration-authoritative. `autoUpdateSmartProfiles` is separately disabled by default; when enabled, three identical semantic descriptor observations are required and at most one change per device and boot is accepted. Profile persistence remains rate-limited. `autoApplyProfile` is a separate opt-in before LED count or ledmap changes are scheduled.

## Compatibility Policy

Draft v1 readers ignore unknown TLVs but reject unknown frame versions, malformed known TLVs, duplicate known TLVs, forbidden cross-domain fields, invalid UTF-8, bad lengths, and bad CRCs. Any incompatible wire-format change must use a new frame version or magic value.
