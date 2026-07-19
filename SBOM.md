# SBOM Policy

The standalone Usermod directly depends on exactly `paulstoffregen/OneWire 2.3.8` and is designed to be linked into WLED with optional AudioReactive support. The complete dependency set is determined by the exact WLED commit, PlatformIO environment, board, and resolved packages.

CI therefore generates release evidence from the assembled pinned WLED tree instead of committing a quickly stale hand-written component list:

- PlatformIO project metadata after dependency resolution;
- SPDX JSON generated from the complete assembled source/build tree;
- the exact candidate application binary built in the same job.

For a release, the generated SBOM and metadata must be attached to the release, hashed with every other asset, and referenced from the provenance manifest. `THIRD_PARTY_NOTICES.md` covers the Usermod's direct OneWire dependency; release review must confirm notices for all components in the generated inventory.
