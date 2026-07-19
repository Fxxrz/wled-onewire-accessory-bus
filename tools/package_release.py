#!/usr/bin/env python3
"""Validate and package a deterministic OneWire Accessory Bus release."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile


PINNED_WLED_SHA = "e39118333d3d5a9c93286243b3aa227e5ee4a713"
ENVIRONMENT = "esp32dev_onewire_audio"
USERMOD_PATH = Path("usermods/onewire_accessory_bus")


def run(command: list[str], *, cwd: Path | None = None) -> str:
  return subprocess.check_output(command, cwd=cwd, text=True, stderr=subprocess.STDOUT).strip()


def git(repo: Path, *arguments: str) -> str:
  return run(["git", "-C", str(repo), *arguments])


def sha256(path: Path) -> str:
  digest = hashlib.sha256()
  with path.open("rb") as stream:
    for chunk in iter(lambda: stream.read(1024 * 1024), b""):
      digest.update(chunk)
  return digest.hexdigest()


def load_json(path: Path, label: str) -> object:
  try:
    return json.loads(path.read_text(encoding="utf-8"))
  except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
    raise SystemExit(f"{label} is not valid UTF-8 JSON: {path}: {error}") from error


def validate_esp32_image(firmware: Path, esptool: Path) -> str:
  header = firmware.read_bytes()[:24]
  if len(header) < 24 or header[0] != 0xE9 or not 1 <= header[1] <= 16:
    raise SystemExit(f"firmware is not an ESP application image: {firmware}")
  if not esptool.is_file():
    raise SystemExit(f"esptool.py is missing: {esptool}")
  try:
    output = run([sys.executable, str(esptool), "image_info", "--version", "2", str(firmware)])
  except subprocess.CalledProcessError as error:
    raise SystemExit(f"esptool image validation failed:\n{error.output}") from error
  required = ["Detected image type: ESP32", "Checksum:", "(valid)", "Validation hash:", "Chip ID: 0 (ESP32)"]
  if any(marker not in output for marker in required):
    raise SystemExit("esptool did not confirm a valid ESP32 image, checksum, hash, and chip ID")
  return output


def validate_metadata(path: Path, firmware_elf: Path) -> dict:
  data = load_json(path, "PlatformIO metadata")
  if not isinstance(data, dict) or ENVIRONMENT not in data or not isinstance(data[ENVIRONMENT], dict):
    raise SystemExit(f"PlatformIO metadata does not contain environment {ENVIRONMENT}")
  environment = data[ENVIRONMENT]
  if environment.get("env_name") != ENVIRONMENT or environment.get("build_type") != "release":
    raise SystemExit("PlatformIO metadata environment name/build type mismatch")
  defines = environment.get("defines")
  if not isinstance(defines, list) or "ARDUINO_ARCH_ESP32" not in defines or "ESP32" not in defines:
    raise SystemExit("PlatformIO metadata does not describe an ESP32 Arduino build")
  compiler = str(environment.get("cxx_path", ""))
  if "xtensa-esp32-elf-g++" not in compiler:
    raise SystemExit("PlatformIO metadata does not use the expected ESP32 toolchain")
  program = Path(str(environment.get("prog_path", "")))
  if program.name != firmware_elf.name:
    raise SystemExit("PlatformIO metadata does not point to firmware.elf")
  build_includes = environment.get("includes", {}).get("build", []) if isinstance(environment.get("includes"), dict) else []
  include_text = "\n".join(str(item) for item in build_includes)
  for marker in ("/OneWire", "/usermods/onewire_accessory_bus", "/usermods/audioreactive"):
    if marker not in include_text:
      raise SystemExit(f"PlatformIO metadata is missing required build input {marker}")
  return data


def validate_spdx(path: Path) -> dict:
  data = load_json(path, "SPDX SBOM")
  if not isinstance(data, dict) or not str(data.get("spdxVersion", "")).startswith("SPDX-2."):
    raise SystemExit("SBOM is not an SPDX 2.x JSON document")
  if data.get("SPDXID") != "SPDXRef-DOCUMENT" or not str(data.get("documentNamespace", "")).startswith(("https://", "http://")):
    raise SystemExit("SBOM document identity/namespace is invalid")
  packages = data.get("packages")
  if not isinstance(packages, list) or not packages:
    raise SystemExit("SBOM does not contain any packages")
  for package in packages:
    if not isinstance(package, dict) or not package.get("name") or not str(package.get("SPDXID", "")).startswith("SPDXRef-"):
      raise SystemExit("SBOM contains an invalid package entry")
  creation = data.get("creationInfo")
  if not isinstance(creation, dict) or not creation.get("creators"):
    raise SystemExit("SBOM creationInfo/creators is missing")
  return data


def compare_tree(source: Path, assembled: Path) -> None:
  source_files = sorted(path.relative_to(source) for path in source.rglob("*") if path.is_file())
  assembled_files = sorted(path.relative_to(assembled) for path in assembled.rglob("*") if path.is_file())
  if source_files != assembled_files:
    raise SystemExit("assembled WLED Usermod file list differs from the public source")
  for relative in source_files:
    if sha256(source / relative) != sha256(assembled / relative):
      raise SystemExit(f"assembled WLED Usermod differs from public source: {relative}")


def validate_wled_tree(project: Path, wled_tree: Path) -> str:
  wled_sha = git(wled_tree, "rev-parse", "HEAD")
  if wled_sha != PINNED_WLED_SHA:
    raise SystemExit(f"WLED SHA {wled_sha} does not match pinned {PINNED_WLED_SHA}")
  changed = git(wled_tree, "diff", "--name-only", PINNED_WLED_SHA, "--")
  staged = git(wled_tree, "diff", "--cached", "--name-only")
  if changed or staged:
    raise SystemExit(f"tracked WLED files differ from the pinned commit: {changed or staged}")
  untracked = [line for line in git(wled_tree, "ls-files", "--others", "--exclude-standard").splitlines() if line]
  generated_evidence = {"platformio-metadata.json", "owab.spdx.json"}
  unexpected = [
    path for path in untracked
    if not path.startswith(f"{USERMOD_PATH.as_posix()}/") and path not in generated_evidence
  ]
  if unexpected:
    raise SystemExit(f"unexpected untracked files in WLED source: {', '.join(unexpected)}")
  compare_tree(project / USERMOD_PATH, wled_tree / USERMOD_PATH)
  override = wled_tree / "platformio_override.ini"
  expected_override = project / "examples/platformio_override.example.ini"
  if not override.is_file() or sha256(override) != sha256(expected_override):
    raise SystemExit("WLED platformio_override.ini differs from the public build definition")
  return wled_sha


def validate_source_remotes(remotes: list[str], public_sha: str) -> list[str]:
  source_links: list[str] = []
  for remote in remotes:
    try:
      refs = run(["git", "ls-remote", remote])
    except subprocess.CalledProcessError as error:
      raise SystemExit(f"cannot inspect source remote {remote}: {error.output}") from error
    if public_sha not in {line.split()[0] for line in refs.splitlines() if line.split()}:
      raise SystemExit(f"public commit {public_sha} is not reachable from source remote {remote}")
    web = re.sub(r"\.git$", "", remote)
    source_links.append(f"{web}/tree/{public_sha}")
  return source_links


def zip_entry(archive: zipfile.ZipFile, name: str, content: bytes) -> None:
  info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
  info.compress_type = zipfile.ZIP_DEFLATED
  info.external_attr = 0o100644 << 16
  archive.writestr(info, content, compresslevel=9)


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser()
  parser.add_argument("--version", required=True, help="Release version without spaces")
  parser.add_argument("--wled-tree", required=True, type=Path)
  parser.add_argument("--metadata", required=True, type=Path)
  parser.add_argument("--sbom", required=True, type=Path)
  parser.add_argument("--esptool", required=True, type=Path)
  parser.add_argument("--source-remote", action="append", default=[], help="Public Git remote containing the exact commit")
  parser.add_argument("--output-dir", required=True, type=Path)
  return parser.parse_args()


def main() -> None:
  args = parse_args()
  if not re.fullmatch(r"[A-Za-z0-9._-]+", args.version):
    raise SystemExit("version may contain only letters, digits, dot, underscore, and hyphen")

  project = Path(__file__).resolve().parents[1]
  if git(project, "status", "--porcelain"):
    raise SystemExit("public source worktree must be clean before packaging")
  public_sha = git(project, "rev-parse", "HEAD")
  wled_sha = validate_wled_tree(project, args.wled_tree)

  build_dir = args.wled_tree / ".pio" / "build" / ENVIRONMENT
  firmware = build_dir / "firmware.bin"
  firmware_elf = build_dir / "firmware.elf"
  firmware_map = build_dir / "firmware.map"
  required = [firmware, firmware_elf, firmware_map, args.metadata, args.sbom,
              project / "LICENSE", project / "THIRD_PARTY_NOTICES.md"]
  for path in required:
    if not path.is_file():
      raise SystemExit(f"required release input is missing: {path}")
  esptool_output = validate_esp32_image(firmware, args.esptool)
  validate_metadata(args.metadata, firmware_elf)
  validate_spdx(args.sbom)
  map_text = firmware_map.read_text(encoding="utf-8", errors="replace")
  if "onewire_accessory_bus" not in map_text or "audioreactive" not in map_text:
    raise SystemExit("linker map does not contain both required Usermods")
  source_links = validate_source_remotes(args.source_remote, public_sha) if args.source_remote else []

  if args.output_dir.exists() and any(args.output_dir.iterdir()):
    raise SystemExit("output directory must not exist or must be empty")
  args.output_dir.mkdir(parents=True, exist_ok=True)

  ota_name = f"WLED_ESP32_onewire_audio_{args.version}_OTA-app.bin"
  ota_output = args.output_dir / ota_name
  shutil.copyfile(firmware, ota_output)
  outputs = [ota_output]
  archive_entries: dict[str, bytes] = {
    ota_name: firmware.read_bytes(),
    firmware_elf.name: firmware_elf.read_bytes(),
    firmware_map.name: firmware_map.read_bytes(),
    "LICENSE": (project / "LICENSE").read_bytes(),
    "THIRD_PARTY_NOTICES.md": (project / "THIRD_PARTY_NOTICES.md").read_bytes(),
    "platformio-metadata.json": args.metadata.read_bytes(),
    "owab.spdx.json": args.sbom.read_bytes(),
  }

  evidence = {
    "OTA/app image": (ota_name, sha256(firmware)),
    "ELF build evidence": (firmware_elf.name, sha256(firmware_elf)),
    "Linker map evidence": (firmware_map.name, sha256(firmware_map)),
    "PlatformIO metadata": (args.metadata.name, sha256(args.metadata)),
    "SPDX SBOM": (args.sbom.name, sha256(args.sbom)),
  }
  provenance_lines = [
    "# Release Provenance", "", f"Version: {args.version}", f"WLED commit: {wled_sha}",
    f"Usermod public commit: {public_sha}", f"Build environment: {ENVIRONMENT}",
    f"OTA/app image: {ota_name}", "Factory/merged image: not included",
    f"WLED source: https://github.com/Aircoookie/WLED/tree/{wled_sha}",
  ]
  provenance_lines.extend(f"Public Usermod source: {link}" for link in source_links)
  provenance_lines.extend(["", "## Bound Build Evidence", ""])
  provenance_lines.extend(f"- {label}: `{name}` SHA-256 `{digest}`" for label, (name, digest) in evidence.items())
  provenance_lines.extend([
    "", "## Image Validation", "", "```text", esptool_output, "```", "",
    "This package is intentionally OTA/app only. The image is not a universal bare-device image and must not be written to an arbitrary offset.", ""
  ])
  provenance = "\n".join(provenance_lines).encode("utf-8")
  archive_entries["PROVENANCE.md"] = provenance

  standalone_evidence = {
    "PROVENANCE.md": provenance,
    "platformio-metadata.json": args.metadata.read_bytes(),
    "owab.spdx.json": args.sbom.read_bytes(),
  }
  for name, content in standalone_evidence.items():
    path = args.output_dir / name
    path.write_bytes(content)
    outputs.append(path)

  archive_path = args.output_dir / f"wled-onewire-accessory-bus-{args.version}-firmware.zip"
  with zipfile.ZipFile(archive_path, "w") as archive:
    for name in sorted(archive_entries):
      zip_entry(archive, name, archive_entries[name])
  outputs.append(archive_path)

  with zipfile.ZipFile(archive_path) as archive:
    names = archive.namelist()
    if len(names) != len(set(names)):
      raise SystemExit("duplicate archive entry detected")
    for name, expected in archive_entries.items():
      if archive.read(name) != expected:
        raise SystemExit(f"archive content verification failed: {name}")

  checksum_path = args.output_dir / "SHA256SUMS"
  checksum_path.write_text(
    "".join(f"{sha256(path)}  {path.name}\n" for path in sorted(outputs)), encoding="ascii"
  )
  print(f"packaged {len(outputs)} artifact(s) plus SHA256SUMS in {args.output_dir}")


if __name__ == "__main__":
  main()
