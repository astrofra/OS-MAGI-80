#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MIGA80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIGA80_PROFILE="${1:-}"

if [ -z "$MIGA80_PROFILE" ]; then
  printf 'Usage: %s <profile-name>\n' "$0" >&2
  printf 'Example: %s a1200-pal-ks30-rom\n' "$0" >&2
  exit 1
fi

if ! command -v fs-uae >/dev/null 2>&1; then
  printf 'FS-UAE is not installed or is not in PATH.\n' >&2
  exit 1
fi

MIGA80_CONFIG="$MIGA80_PROJECT_ROOT/build/fs-uae/$MIGA80_PROFILE.fs-uae"
if [ ! -f "$MIGA80_CONFIG" ]; then
  "$MIGA80_PROJECT_ROOT/scripts/configure-fs-uae.sh"
fi
if [ ! -f "$MIGA80_CONFIG" ]; then
  printf 'Profile was not generated; check its local media path: %s\n' "$MIGA80_PROFILE" >&2
  exit 1
fi

exec fs-uae "$MIGA80_CONFIG"
