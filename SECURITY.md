# Security Policy

## Supported Versions

This project is experimental beta software. Only the latest public development branch and newest beta candidate receive fixes. Superseded assets may be removed when required to remediate privacy or security metadata; published release notes must disclose such a reset.

## Reporting A Vulnerability

For a potentially exploitable issue, use GitHub's private **Report a vulnerability** / Security Advisory flow for this repository when available. Do not include credentials, private network data, real device dumps, or personally identifying information unless strictly necessary and explicitly agreed.

For non-sensitive defects, open a normal issue with the WLED commit, board/chip, build environment/defines, Usermod commit, configuration with secrets removed, expected behavior, observed behavior, and recovery result.

Please avoid public disclosure of a credible security issue until a fix or coordinated advisory is ready. This hobby project cannot promise a commercial response SLA, but reports will be acknowledged and triaged on a best-effort basis.

## Security Boundary

- OneWire CRC detects transfer corruption; it does not authenticate physical modules.
- WLED's Settings PIN is a device-global trusted-LAN change barrier, not strong per-user authorization.
- MQTT security depends on the broker and WLED configuration.
- Firmware and hardware are provided without warranty under the project license.
