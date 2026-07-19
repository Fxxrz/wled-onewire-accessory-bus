# Contributing

Contributions and test reports are welcome, especially for independent OneWire slave implementations, noisy/hotplug buses, WLED compatibility, MQTT/Home Assistant, and different ESP32 boards.

## Before A Pull Request

1. Base work on the public repository; do not include private remotes, credentials, addresses, real ROM IDs, or local paths.
2. Keep changes inside `usermods/onewire_accessory_bus` unless a WLED-core proposal is discussed first.
3. Preserve passive-profile/config compatibility or document a migration.
4. Add executable tests for protocol logic and focused source/integration contracts where hardware execution is not possible.
5. Run:

```sh
node usermods/onewire_accessory_bus/onewire_accessory_bus.test.js
pio run -e esp32dev_onewire_audio
git diff --check
```

6. For hardware changes, report board/chip, GPIO, pullup voltage/value, approximate bus length/topology, device families/count, power method, and measured failure behavior.

## Design Principles

- WLED/ESP remains the master.
- Scanning is bounded and side-effect-free; smart servicing is scheduled separately.
- Physical presence is not authorization for persistent actuator changes without enrollment.
- Electrical failure is never treated as a clean empty bus.
- API values are rejected rather than silently clamped.
- WLED core changes require a clear API need, compatibility analysis, and separate approval.
