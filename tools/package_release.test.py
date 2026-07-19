#!/usr/bin/env python3
"""Negative tests for release input validation."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("package_release.py")
SPEC = importlib.util.spec_from_file_location("package_release", SCRIPT)
package_release = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(package_release)


class PackageValidationTests(unittest.TestCase):
  def test_rejects_non_esp_binary(self) -> None:
    with tempfile.TemporaryDirectory() as directory:
      fake = Path(directory) / "firmware.bin"
      fake.write_bytes(b"not an ESP image")
      with self.assertRaisesRegex(SystemExit, "not an ESP application image"):
        package_release.validate_esp32_image(fake, Path(directory) / "esptool.py")

  def test_rejects_wrong_platformio_environment(self) -> None:
    with tempfile.TemporaryDirectory() as directory:
      metadata = Path(directory) / "metadata.json"
      metadata.write_text(json.dumps({"some_other_env": {"env_name": "some_other_env"}}), encoding="utf-8")
      with self.assertRaisesRegex(SystemExit, "does not contain environment"):
        package_release.validate_metadata(metadata, Path(directory) / "firmware.elf")

  def test_rejects_dummy_sbom(self) -> None:
    with tempfile.TemporaryDirectory() as directory:
      sbom = Path(directory) / "sbom.json"
      sbom.write_text(json.dumps({"packages": []}), encoding="utf-8")
      with self.assertRaisesRegex(SystemExit, "not an SPDX"):
        package_release.validate_spdx(sbom)

  def test_rejects_wrong_wled_commit(self) -> None:
    with tempfile.TemporaryDirectory() as directory:
      root = Path(directory)
      project = root / "project"
      wled = root / "WLED"
      (project / package_release.USERMOD_PATH).mkdir(parents=True)
      wled.mkdir()
      subprocess.run(["git", "init", "-q"], cwd=wled, check=True)
      subprocess.run(["git", "config", "user.name", "Release Test"], cwd=wled, check=True)
      subprocess.run(["git", "config", "user.email", "release-test@example.invalid"], cwd=wled, check=True)
      (wled / "README").write_text("fixture\n", encoding="utf-8")
      subprocess.run(["git", "add", "README"], cwd=wled, check=True)
      subprocess.run(["git", "commit", "-q", "-m", "fixture"], cwd=wled, check=True)
      with self.assertRaisesRegex(SystemExit, "does not match pinned"):
        package_release.validate_wled_tree(project, wled)


if __name__ == "__main__":
  unittest.main()
