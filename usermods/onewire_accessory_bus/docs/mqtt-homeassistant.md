# MQTT And Home Assistant Contract

MQTT output uses WLED's configured broker, device topic, client ID, connection, and Last Will status topic. The usermod adds no broker credentials of its own.

## Publish Policy

- Default interval: 30 seconds; configured by `mqttIntervalMs`, minimum 1000 ms.
- QoS: 0 for state and discovery.
- Compact JSON status: not retained.
- Flat bus/profile state: retained.
- Per-device telemetry: not retained.
- Home Assistant discovery configuration: retained.
- Boolean payloads: lowercase `true` and `false`.

All topics are rooted at WLED's `<mqttDeviceTopic>`.

## Status Topics

```text
<mqttDeviceTopic>/onewire/status
<mqttDeviceTopic>/onewire/device_count
<mqttDeviceTopic>/onewire/device_overflow
<mqttDeviceTopic>/onewire/profile_count
<mqttDeviceTopic>/onewire/fallback_led_count
<mqttDeviceTopic>/onewire/fallback_ledmap
<mqttDeviceTopic>/onewire/active_profile
<mqttDeviceTopic>/onewire/apply_status
<mqttDeviceTopic>/onewire/input_status
```

The JSON document contains device/profile counts, overflow, active-profile fields, apply/input/command status, bus health, fallback settings, and an array of device identity plus descriptor/telemetry health. A fixed 4096-byte reusable ArduinoJson document is used; overflow publishes an explicit error document instead of truncated JSON.

Only received battery fields are published:

```text
<mqttDeviceTopic>/onewire/<rom>/battery_voltage
<mqttDeviceTopic>/onewire/<rom>/battery_percent
<mqttDeviceTopic>/onewire/<rom>/battery_charging
<mqttDeviceTopic>/onewire/<rom>/battery_temperature
<mqttDeviceTopic>/onewire/<rom>/battery_status
<mqttDeviceTopic>/onewire/<rom>/battery_stale
```

Voltage is in volts with three decimals, temperature is degrees Celsius with one decimal, percentage/status are decimal integers, and stale/charging are booleans. Each field has its own wrap-safe age and is no longer republished after it becomes stale (60 seconds by default). `battery_stale=true` when any previously received field is stale. The compact JSON status also exposes aggregate and per-field freshness through the JSON API.

## Home Assistant Discovery

Discovery is sent after MQTT reconnect and whenever the stable entity shape changes. Failed discovery publishes remain pending and are retried on the next MQTT interval.

Root entities include device count, profile count, fallback settings, active profile, apply/input status, and a binary `device_overflow` sensor. Per-ROM entities are created only for battery fields that have actually been received. Measurement sensors include `state_class: measurement`; binary entities use explicit true/false payloads.

Discovery uses WLED's `<mqttDeviceTopic>/status` availability topic with `online`/`offline`. Entity `expire_after` is three publish intervals, bounded to 180..3600 seconds, so a removed or stale accessory becomes unavailable even while the WLED base remains online. Per-ROM entity names include the final four ROM characters, while unique IDs include the sanitized WLED MQTT client ID and full ROM.

## Discovery Cleanup Policy

Normal accessory removal deliberately does not delete retained discovery configuration. A removable shade or battery should keep the same Home Assistant entity and history when reattached; `expire_after` represents its temporary absence.

The current beta does not persist an inventory of every previously published discovery topic. Therefore disabling Home Assistant discovery, changing WLED's MQTT client ID/device topic, or permanently retiring a ROM can leave old retained discovery entries at the broker. Those entries must currently be removed with the broker/Home Assistant MQTT tooling.

Automatic cleanup on every hot-unplug was not implemented because it would cause entity churn and history fragmentation. A future cleanup command should maintain a bounded persisted discovery inventory and explicitly distinguish temporary removal from permanent retirement.
