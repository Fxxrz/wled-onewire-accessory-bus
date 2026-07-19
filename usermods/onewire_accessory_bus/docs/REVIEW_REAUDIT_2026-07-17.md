# 2026-07-17 Re-Audit Disposition

This is the sanitized disposition of the independent re-audit. It records which findings changed the beta 2 source and which limits remain outside a standalone WLED Usermod. It is not a replacement for the full review report or hardware evidence.

## Implemented In Beta 2

| Finding | Disposition |
|---|---|
| RA-H-01 | ESP32 ROM enumeration moved to a dedicated FreeRTOS OneWire worker. A complete scan is bounded by eight accepted ROMs and a hard search-iteration limit, not by the former impossible 50-ms total budget. |
| RA-M-01 | Apply verification always advances the active transaction; a newer `applyRequested` target remains pending until apply/rollback reaches idle. Runtime reinitialization requeues automatic apply. |
| RA-M-02/03 | `handleButton` now uses the state guard. OneWire I/O left the guarded main-loop path, callbacks no longer silently abandon state after 50 ticks, and mutex/queue failures are diagnosed. |
| RA-M-04 | Telemetry age is calculated per field with unsigned wrap-safe subtraction. Stale individual fields are not periodically republished; aggregate and any-field stale states are separate. |
| RA-M-05 | Smart descriptor configuration needs three identical semantic observations. Existing-profile updates are separately default-off and limited to once per device and boot. |
| RA-M-06 | Descriptor and telemetry transactions run on the worker; telemetry is capped at 48 bytes and descriptor data at 96 bytes. `maxWireTransactionUs` reports actual worker duration. |
| RA-M-07 | Packaging rejects the wrong WLED SHA, tracked/unexpected source changes, mismatched Usermod bytes/override, non-ESP images, wrong PlatformIO environment/toolchain, missing linked Usermods, invalid SPDX, and unverified source remotes. Firmware, ELF, map, metadata, SBOM, and commits are hash-bound in provenance. |
| RA-L-01..05 | Mutation exits report explicit results; command types/count are strict; config values are validated before narrowing; skipped/invalid config values are counted; MQTT discovery is requeued on runtime enable; pre-setup and post-setup mutex absence differ. |
| RA-L-07/09/10 | Source links are verified remotes; parser length/CRC/partial/wrap cases run under ASan/UBSan; negative packaging tests execute; the library manifest now carries version, license, repository, platform/framework, and exact OneWire metadata. |

## Release Operations

| Finding | Required release action |
|---|---|
| RA-M-08 | CI uses an explicit Ubuntu image, exact tool versions, and full action commit IDs. A release still requires a green remote run and downloaded evidence verification. |
| RA-M-09 | GitHub private vulnerability reporting must be enabled before announcing beta 2; `SECURITY.md` points to that private channel. |
| RA-M-10 | The withdrawn predecessor tag/assets were removed during the explicitly approved privacy reset. Beta 2 is rebuilt and republished from the reviewed root history. |
| RA-L-06/08 | Public manifest wording distinguishes release-only binaries from source. Release QA scans the exact commit plus worktree/image/archive and Git identity metadata; CI rejects non-Noreply commit/tag emails. |

## Deliberate WLED-Core Boundaries

The following review observations were not implemented because doing so would require a WLED core contract and the project explicitly remains Usermod-only:

- a dedicated `PinOwner` enum value;
- exact delegation into every native WLED button mode (use `inherit` for the native path);
- atomic LED bus/config persistence across power loss;
- an exact distinction between ledmap 0 and no custom ledmap when the core exposes the same observable state.

The callback design also remains a guarded short-state model rather than a full command mailbox. The long OneWire operations that motivated callback timeouts now run outside the main loop; commands are strictly validated under one mutex and no longer report silent timeout success. Queue and lock failure counters make residual pressure observable.

## Remaining Evidence Limits

- Passive one-device HIL must be repeated on the final tagged binary.
- Four/eight-ROM electrical HIL, bus fault injection, and long-duration stress remain pending.
- Smart/battery claims remain draft-only until an independent slave passes interoperability and fault tests.
- Branch/tag protection, tag/checksum signing, remote CI, host security settings, and release asset redownload are hosting/release gates rather than source changes.
