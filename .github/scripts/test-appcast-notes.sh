#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
extractor="$script_dir/appcast-notes.sh"
mode="${1:-new}"

extract_notes() {
  if [[ "$mode" == "--old-awk" ]]; then
    awk 'NF { print; exit }'
  else
    "$extractor"
  fi
}

assert_notes() {
  local name="$1"
  local body="$2"
  local expected="$3"
  local actual
  actual="$(printf '%s' "$body" | extract_notes)"
  if [[ "$actual" != "$expected" ]]; then
    printf 'not ok - %s\n  expected: <%s>\n  actual:   <%s>\n' "$name" "$expected" "$actual" >&2
    return 1
  fi
  printf 'ok - %s\n' "$name"
}

assert_notes \
  "v1.2.3 release body" \
  $'## What\'s Changed\r\n* Let an empty SUPPORT lane actually accept an amp https://github.com/guitarlum/VoLum/pull/31\r\n\r\n**Full Changelog**: https://github.com/guitarlum/VoLum/compare/v1.2.2...v1.2.3\r\n' \
  "Let an empty SUPPORT lane actually accept an amp https://github.com/guitarlum/VoLum/pull/31"

assert_notes \
  "heading then linked bullet" \
  $'## Highlights\n\n- **Read** [release notes](https://example.test/notes) for `VoLum`' \
  "Read release notes for VoLum"

assert_notes \
  "headings only" \
  $'# VoLum\n## What\'s Changed\n### Fixes\n' \
  ""

assert_notes \
  "CRLF and numbered list" \
  $'## Fixes\r\n\r\n1. Fixed Windows update checks\r\n' \
  "Fixed Windows update checks"

assert_notes \
  "unicode" \
  $'## Neues\n* Größe, Bühne und 日本語 stay intact 🎸\n' \
  "Größe, Bühne und 日本語 stay intact 🎸"

assert_notes \
  "comments rules and image-only lines" \
  $'<!-- generated -->\n---\n[![Build](https://example.test/badge.svg)](https://example.test/build)\n<img src="hero.png">\n_Actual_ note\n' \
  "Actual note"

long_note="$(printf 'a%.0s' {1..130})"
capped_note="$(printf 'a%.0s' {1..117})..."
assert_notes "caps long notes at 120 characters" "$long_note" "$capped_note"

printf 'All appcast note tests passed.\n'
