#!/usr/bin/env python3

from __future__ import annotations

import argparse
import datetime as dt
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = REPO_ROOT / "NeuralAmpModeler"
PROJECT_SCRIPTS = PROJECT_ROOT / "scripts"
CONFIG_PATH = PROJECT_ROOT / "config.h"
CHANGELOG_PATH = PROJECT_ROOT / "installer" / "changelog.txt"

CONFIG_VERSION_RE = re.compile(r'(#define PLUG_VERSION_STR ")([^"]+)(")')
CONFIG_HEX_RE = re.compile(r'(#define PLUG_VERSION_HEX )(0x[0-9A-Fa-f]+)')
SEMVER_RE = re.compile(r'^(?:v)?(\d+)\.(\d+)(?:\.(\d+))?$')


@dataclass(frozen=True, order=True)
class Version:
    major: int
    minor: int
    patch: int

    @classmethod
    def parse(cls, raw: str) -> "Version":
        match = SEMVER_RE.fullmatch(raw.strip())
        if not match:
            raise ValueError(f"Unsupported version format: {raw!r}")
        major, minor, patch = match.groups()
        return cls(int(major), int(minor), int(patch or 0))

    def bump(self, bump_type: str) -> "Version":
        if bump_type == "major":
            return Version(self.major + 1, 0, 0)
        if bump_type == "minor":
            return Version(self.major, self.minor + 1, 0)
        if bump_type == "patch":
            return Version(self.major, self.minor, self.patch + 1)
        raise ValueError(f"Unsupported bump type: {bump_type}")

    def tag(self) -> str:
        return f"v{self}"

    def hex_value(self) -> str:
        value = ((self.major << 16) & 0xFFFF0000) + ((self.minor << 8) & 0x0000FF00) + (self.patch & 0x000000FF)
        return f"0x{value:08x}"

    def __str__(self) -> str:
        return f"{self.major}.{self.minor}.{self.patch}"


def run(command: list[str], *, cwd: Path = REPO_ROOT, capture_output: bool = False) -> str:
    result = subprocess.run(
        command,
        cwd=cwd,
        check=True,
        text=True,
        capture_output=capture_output,
    )
    return result.stdout if capture_output else ""


def read_current_version() -> Version:
    config_text = CONFIG_PATH.read_text(encoding="utf-8")
    match = CONFIG_VERSION_RE.search(config_text)
    if not match:
        raise RuntimeError(f"Could not find PLUG_VERSION_STR in {CONFIG_PATH}")
    return Version.parse(match.group(2))


def read_known_versions() -> set[Version]:
    tags_output = run(["git", "tag", "--list"], capture_output=True)
    versions: set[Version] = set()
    for raw_tag in tags_output.splitlines():
        raw_tag = raw_tag.strip()
        if not raw_tag:
            continue
        try:
            versions.add(Version.parse(raw_tag))
        except ValueError:
            continue
    return versions


def determine_target_version(explicit_version: str | None, bump_type: str) -> tuple[Version, Version]:
    current_version = read_current_version()
    known_versions = read_known_versions()
    latest_known = max(known_versions, default=current_version)
    base_version = max(current_version, latest_known)
    target_version = Version.parse(explicit_version) if explicit_version else base_version.bump(bump_type)
    if target_version in known_versions:
        raise RuntimeError(f"Release version v{target_version} already exists in git tags")
    return current_version, target_version


def replace_once(path: Path, pattern: re.Pattern[str], replacement: str) -> None:
    original = path.read_text(encoding="utf-8")
    updated, count = pattern.subn(replacement, original, count=1)
    if count != 1:
        raise RuntimeError(f"Expected one replacement in {path}, found {count}")
    path.write_text(updated, encoding="utf-8")


def update_config(version: Version) -> None:
    replace_once(CONFIG_PATH, CONFIG_VERSION_RE, rf'\g<1>{version}\g<3>')
    replace_once(CONFIG_PATH, CONFIG_HEX_RE, rf'\g<1>{version.hex_value()}')


def update_generated_metadata() -> None:
    python = sys.executable
    run([python, "update_version-mac.py"], cwd=PROJECT_SCRIPTS)
    run([python, "update_version-ios.py"], cwd=PROJECT_SCRIPTS)
    run([python, "update_installer-win.py", "0"], cwd=PROJECT_SCRIPTS)


def append_changelog(version: Version) -> str:
    today = dt.date.today().strftime("%m/%d/%Y")
    release_line = f"{today} - v{version} VoLum release. See GitHub release notes for included changes."
    changelog_text = CHANGELOG_PATH.read_text(encoding="utf-8").rstrip()
    CHANGELOG_PATH.write_text(changelog_text + "\n\n" + release_line + "\n", encoding="utf-8")
    return release_line


def write_outputs(output_path: Path, version: Version, changelog_line: str) -> None:
    with output_path.open("a", encoding="utf-8") as handle:
        handle.write(f"version={version}\n")
        handle.write(f"tag={version.tag()}\n")
        handle.write(f"changelog_line={changelog_line}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Prepare a VoLum release version for CI.")
    parser.add_argument("--version", help="Exact version to release, for example 0.3.0")
    parser.add_argument(
        "--bump",
        choices=("major", "minor", "patch"),
        default="minor",
        help="Version bump to apply when --version is omitted",
    )
    parser.add_argument("--github-output", help="Path to GITHUB_OUTPUT for workflow outputs")
    parser.add_argument("--dry-run", action="store_true", help="Print the computed release version without changing files")
    args = parser.parse_args()

    current_version, target_version = determine_target_version(args.version, args.bump)
    release_line = f"{dt.date.today().strftime('%m/%d/%Y')} - v{target_version} VoLum release. See GitHub release notes for included changes."

    print(f"Current version: {current_version}")
    print(f"Target version:  {target_version}")
    print(f"Release tag:     {target_version.tag()}")

    if args.dry_run:
        if args.github_output:
            write_outputs(Path(args.github_output), target_version, release_line)
        return 0

    update_config(target_version)
    update_generated_metadata()
    release_line = append_changelog(target_version)

    if args.github_output:
        write_outputs(Path(args.github_output), target_version, release_line)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
