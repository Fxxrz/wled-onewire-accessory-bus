# 2026-07-16 Deep Audit Disposition

Historical disposition for the first audit pass. Findings reopened by the 2026-07-17 re-audit and their beta 2 fixes are recorded in [REVIEW_REAUDIT_2026-07-17.md](REVIEW_REAUDIT_2026-07-17.md); timing and release statements below describe the pre-beta-2 candidate.

This is the maintainer disposition for the independent audit dated 2026-07-16. It records what was changed in the development branch and which findings remain release, hardware, WLED-core, or governance work. It is not a claim that the unreleased candidate is production-ready.

Status terms:

- **Implemented**: addressed in the Usermod and covered by executable tests, source contracts, integration build, or documentation as appropriate.
- **Partial**: the safe Usermod-local portion is implemented, but a stated boundary remains.
- **Deferred**: valid work that requires hardware, release infrastructure, current-upstream integration, or explicit approval for broader changes.
- **Different decision**: the finding is understood, but the chosen behavior intentionally differs and is documented.

## Code Findings

| ID | Disposition | Result |
|---|---|---|
| CODE-01 | Implemented | Invalid descriptors use a 10-second retry interval; a 250-ms round-robin scheduler services one stable smart device at a time. |
| CODE-02 | Implemented | Smart servicing occurs only after stable inventory commit. New smart shades require `autoEnrollSmartShades`; persistent writes are rate-limited. |
| CODE-03 | Implemented | Descriptor and telemetry have separate allowlist parsers. Legacy telemetry defaults off and can never change actuator/profile fields. |
| CODE-04 | Implemented | Fallback is not planned during config load and requires a stable known `empty_clean` bus. |
| CODE-05 | Implemented | Search has iteration/device/50-ms budgets. Manual scan is PIN-classified and limited to one request per second. Smart I/O is outside scan. |
| CODE-06 | Partial | Only one valid physical digital bus is accepted; bounds, heap margin, target verification, timeout, snapshot, and best-effort rollback were added. Fully atomic persistence across power loss needs a WLED core transaction hook and was not added. |
| CODE-07 | Implemented | Target comparison includes ledmap even when LED length is unchanged. |
| CODE-08 | Implemented | The current map ID is captured and reapplied when `ledmap=-1` spans a bus rebuild. |
| CODE-09 | Implemented | Electrical/error states are distinct and never trigger fallback. |
| CODE-10 | Implemented | Only fields present in a valid telemetry frame receive a new timestamp. |
| CODE-11 | Implemented | Battery validity and age are per field; absent values are not published as zero measurements. |
| CODE-12 | Implemented | Exact known lengths, class/value ranges, required fields, duplicate rejection, UTF-8 validation, and LED limits are enforced. |
| CODE-13 | Implemented | Smart service is fair and decoupled from scan cadence. |
| CODE-14 | Implemented | Descriptor/telemetry attempt, success, failure, stale, sequence, and last-error diagnostics are exposed. |
| CODE-15 | Partial/documented | All Usermod mutations use WLED's Settings PIN helper. Its device-global unlock semantics belong to WLED core and are now stated accurately. |
| CODE-16 | Implemented | ESP32 state shared by AsyncTCP/MQTT callbacks and the main loop is guarded by a FreeRTOS mutex. |
| CODE-17 | Implemented | Pin, enabled, and pullup changes teardown/reallocate/reinitialize in the main loop; configured and active values are reported separately. |
| CODE-18 | Implemented | API input is rejected on wrong type/range/ROM CRC/UTF-8 rather than silently clamped. Arrays are preflighted before mutation. |
| CODE-19 | Partial | MQTT uses a reusable 4096-byte static document and checks overflow; maximum-state HIL/load verification remains a release gate. |
| CODE-20 | Partial/different decision | Discovery retries and uses WLED availability plus expiry. Hot-unplug deliberately preserves discovery identity/history; permanent cleanup after topic/client-ID changes remains manual. |
| CODE-21 | Implemented | Overflow is a binary sensor, measurements have state class, per-ROM names are distinguishable, and reconnect no longer immediately double-publishes. |
| CODE-22 | Implemented/documented | ESP8266 active-high uses plain input and requires an external pulldown; ESP32 remains the primary target. |
| CODE-23 | Implemented | Family display follows the configured smart family code. |
| CODE-24 | Implemented | Config load deduplicates ROMs, reports duplicate/overflow state, and includes fallback fields in completeness handling. |
| CODE-25 | Implemented | UI consumes runtime limits, keeps PIN only in memory, confirms deletion, reports async errors, exposes full ROMs, and improves table accessibility. |
| CODE-26 | Deferred/core boundary | The local managed-input approximation remains. Reusing the exact native handler cleanly needs a WLED core hook; `inherit` is documented as the exact-semantics mode. |
| CODE-27 | Implemented | Changing CRC count no longer bypasses the serial heartbeat rate limit. |
| CODE-28 | Implemented as draft | Protocol now specifies CRC parameters/vector, byte order, 96-byte limit, 1-ms busy polling, 5-ms bound, TLV schemas, UTF-8, duplicate and sequence rules, enrollment, and compatibility. Independent slave HIL is still required before protocol stabilization. |

## Tests

- Native C++ tests execute CRC-16/CCITT-FALSE, descriptor/telemetry allowlists, duplicates, bounds, legacy policy, UTF-8, and timer wraparound.
- The Node suite compiles/runs those tests and then checks 15 integration source contracts.
- The hardware smoke script now requires an expected ROM or explicit empty-bus opt-in, requires Node 18+, advances `scanCount`, and waits for stable state with PIN support.
- `esp32dev_onewire_audio` builds and links both usermods. Current measured result: 89,560 bytes RAM and 1,346,553 bytes flash.
- A fresh temporary checkout of current WLED `main` (`e39118333d3d5a9c93286243b3aa227e5ee4a713`) also builds and links both usermods: 89,560 bytes RAM and 1,348,853 bytes flash. The development branch itself was not rebased.
- Executable multi-device smart-slave, noisy-bus, MQTT maximum-state, memory-failure, and long-duration HIL remain deferred until appropriate hardware/emulators are available.

## Documentation And Hardware

Implemented documentation corrections include valid synthetic ROM CRC8 values, runtime ledmap limits, global Settings-PIN semantics, mutating scan semantics, complete JSON command/status fields, MQTT QoS/retain/availability policy, smart enrollment trust, normative protocol timing/CRC, OTA-vs-factory wording, and explicit UI-mockup labeling requirements.

The hardware guide now forbids 5-V GPIO pullup, requires common ground, records the tested GPIO19/4.7-kOhm/20-cm passive path, and lists ESD, back-power, inrush, current, battery, and connector review requirements. A reproducible smart/battery design is still deferred until schematic, BOM, ratings, protection choices, and measurements exist; those cannot be invented in software documentation.

## Release, Compliance, And Governance

The following are valid findings but cannot be closed merely by editing the Usermod implementation:

- The withdrawn predecessor publication was removed during the explicitly approved privacy reset. The corrected retained publication is `beta.2` or later.
- The next release must distinguish OTA/app from factory/merged images, package from an empty staging directory, publish one manifest, hash every asset, and verify remote downloads.
- Exact public WLED base/patch/build definitions, third-party notices, and an SBOM are release requirements. The old private commit is not sufficient public provenance.
- Current-upstream compatibility was built in a clean temporary checkout. Rebasing the development repository remains a broad history operation and requires separate approval under workspace rules.
- Branch protection, required review, immutable-tag policy, and cryptographic signing are host/account governance settings; they are not hidden code changes.
- Public maintainer commits and annotated tags must use privacy-preserving Noreply metadata. An explicitly approved privacy incident response may replace public refs after complete private backup and renewed release verification.
- A full legal conclusion on corresponding source and linked dependency obligations requires qualified review; project files can provide source, notices, dependency inventory, and build provenance but cannot substitute for legal advice.

## Core-Change Decision

No WLED core file was changed for this remediation. A dedicated `PinOwner`, exact native managed-button delegation, and truly transactional LED-bus persistence were not implemented because each needs a WLED-core API/contract change. They remain documented boundaries rather than concealed Usermod behavior.
