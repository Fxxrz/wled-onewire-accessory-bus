# Hardware Notes And Safety Boundary

The reference lamp adds one OneWire data contact to the existing power, ground, LED-data, and button/touch contacts. "OneWire contact" therefore means one additional signal contact, not a complete one-contact electrical interface. WLED/ESP32 remains the bus master.

## Validated Reference Path

- ESP32 with OneWire on GPIO19.
- External 4.7 kOhm pullup from OneWire data to 3.3 V.
- Approximately 20 cm local wiring.
- Passive DS2502 identification chip.
- Hotplug profile switching with a 101-LED shade and a 96-LED no-accessory fallback.
- AudioReactive and the OneWire usermod in the same build.

The mechanical reference design prevents the OneWire contact from touching ground, supply, or LED data during a shade change. That is a property of this geometry, not a general guarantee for other connectors or replicas.

## Required Electrical Rules

- OneWire logic and pullup voltage MUST be 3.3 V. Do not pull the ESP32 GPIO to 5 V; ordinary ESP32 GPIOs are not 5-V tolerant.
- Base and accessory MUST share ground.
- Use a dedicated external pullup where possible. 4.7 kOhm is the validated starting value for the short reference bus, not a universal value.
- Smart accessories SHOULD use direct regulated power and local decoupling. Parasite-powered smart MCUs are outside the supported design.
- Never route battery voltage directly to the OneWire GPIO.
- Confirm the selected GPIO is free and suitable on the exact board. GPIO19 is only the tested reference assignment; board-level USB, JTAG, flash, PSRAM, strapping, Ethernet, display, or peripheral use can make another pin necessary.

The ESP internal pullup may work for short passive-ID prototypes, but its resistance is weak and device-dependent. It gives slower rise time and lower noise margin than the external 4.7 kOhm reference. The usermod can switch it at runtime for bring-up, but production hardware should use the external resistor and validate rise time at the accessory contact.

## Hotplug And Protection

Software debounce cannot replace electrical protection. A public derivative should evaluate:

- ESD protection appropriate for an exposed user-touchable contact;
- a small series resistor near the ESP pin where signal-integrity measurements permit it;
- inrush, back-powering, and reverse-current paths when powered modules are inserted;
- current limiting, fusing, connector rating, and wire rating for the separate power contacts;
- local bulk and high-frequency decoupling for smart or battery modules;
- behavior while the base is unpowered but an accessory battery is present.

Exact TVS type, series resistance, contact current, power limit, insertion sequencing, and cable capacitance cannot be specified responsibly without the final schematic, connector data, and measurements. Until those are published, the smart/battery hardware is a concept interface rather than a reproducible certified reference design.

## Bus Topology

Use a short linear connection. Avoid star branches and long stubs. The current firmware stores up to eight devices, but that software limit is not an electrical guarantee that eight modules plus their wiring will meet OneWire timing.

The usermod distinguishes a clean empty bus from stuck-low, invalid-presence, CRC, overflow, and hard search-iteration-limit failures. Fallback LED settings are applied only after a stable clean empty state. Electrical faults do not intentionally masquerade as "old shade without OneWire."

## Passive ID Accessories

Passive chips such as DS2502 provide only their factory ROM ID. LED count, ledmap, name, and input mode remain stored in WLED. The usermod validates the Dallas ROM CRC8 before accepting an ID.

Programming DS2502 EPROM memory requires device-specific programming conditions and is not used by this project. Identity comes from the factory ROM, so no 12-V programming path is required in the lamp.

## Smart Accessories

A smart accessory needs a microcontroller with a real OneWire slave implementation and the draft protocol in [smart-accessory-protocol.md](smart-accessory-protocol.md). CRC is error detection, not authentication. A physically connected smart module is therefore a trusted participant; automatic enrollment and automatic LED application are separate settings and both default to the conservative path.

Before publishing a buildable battery-module design, add at minimum a reviewed schematic, BOM, cell protection and charger details, temperature limits, fault behavior, connector ratings, and measured hotplug/transient results.
