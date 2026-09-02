#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MAGI80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAGI80_BUILD_DIR="$MAGI80_PROJECT_ROOT/build/fs-uae-smoke"
MAGI80_STAGING_DIR="$MAGI80_PROJECT_ROOT/build/staging"
MAGI80_BASE_CONFIG="$MAGI80_PROJECT_ROOT/build/fs-uae/a1200-pal-ks30-hd.fs-uae"
MAGI80_TEST_CONFIG="$MAGI80_BUILD_DIR/a1200-pal-ks30-runtime-smoke.fs-uae"
MAGI80_TEST_ADF="$MAGI80_BUILD_DIR/runtime-smoke.adf"
MAGI80_MARKER="$MAGI80_STAGING_DIR/fs-uae-smoke.out"
MAGI80_EXPECTED="$MAGI80_PROJECT_ROOT/tests/smoke/hosted-bootstrap/expected.txt"
MAGI80_TIMEOUT_SECONDS="${MAGI80_FS_UAE_TIMEOUT_SECONDS:-30}"
MAGI80_PIPX_BIN="${MAGI80_PIPX_BIN:-/Users/fra/.local/bin}"
MAGI80_EMULATOR_PID=""

export PATH="$MAGI80_PIPX_BIN:$PATH"

stop_emulator() {
  local attempt
  local process_state

  if [ -z "$MAGI80_EMULATOR_PID" ] || ! /bin/kill -0 "$MAGI80_EMULATOR_PID" 2>/dev/null; then
    return
  fi

  /bin/kill -INT "$MAGI80_EMULATOR_PID" 2>/dev/null || true
  for attempt in {1..20}; do
    if ! /bin/kill -0 "$MAGI80_EMULATOR_PID" 2>/dev/null; then
      wait "$MAGI80_EMULATOR_PID" 2>/dev/null || true
      return
    fi
    process_state="$(/bin/ps -p "$MAGI80_EMULATOR_PID" -o state= 2>/dev/null | /usr/bin/awk '{ print $1 }')"
    case "$process_state" in
      ''|Z*)
        wait "$MAGI80_EMULATOR_PID" 2>/dev/null || true
        return
        ;;
    esac
    /bin/sleep 0.25
  done

  /bin/kill -TERM "$MAGI80_EMULATOR_PID" 2>/dev/null || true
  wait "$MAGI80_EMULATOR_PID" 2>/dev/null || true
}

trap stop_emulator EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

for command in fs-uae gmake xdftool xdfscan; do
  if ! command -v "$command" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "$command" >&2
    exit 1
  fi
done

case "$MAGI80_TIMEOUT_SECONDS" in
  ''|*[!0-9]*)
    printf 'MAGI80_FS_UAE_TIMEOUT_SECONDS must be a positive integer.\n' >&2
    exit 1
    ;;
esac
if [ "$MAGI80_TIMEOUT_SECONDS" -eq 0 ]; then
  printf 'MAGI80_FS_UAE_TIMEOUT_SECONDS must be greater than zero.\n' >&2
  exit 1
fi

gmake -C "$MAGI80_PROJECT_ROOT" stage >/dev/null
"$MAGI80_PROJECT_ROOT/scripts/configure-fs-uae.sh" >/dev/null

if [ ! -f "$MAGI80_BASE_CONFIG" ]; then
  printf 'The Kickstart 3.0 HD profile is unavailable; configure MAGI80_WORKBENCH_30.\n' >&2
  exit 1
fi

/bin/mkdir -p "$MAGI80_BUILD_DIR" "$MAGI80_STAGING_DIR"
/bin/rm -f "$MAGI80_MARKER"

xdftool -f "$MAGI80_TEST_ADF" \
  create \
  + format MAGI80TEST ofs \
  + makedir S \
  + write "$MAGI80_PROJECT_ROOT/tests/smoke/fs-uae/Startup-Sequence" S/Startup-Sequence \
  + boot install >/dev/null
xdfscan "$MAGI80_TEST_ADF" >/dev/null

/bin/cp "$MAGI80_BASE_CONFIG" "$MAGI80_TEST_CONFIG"
printf 'floppy_drive_0 = %s\n' "$MAGI80_TEST_ADF" >>"$MAGI80_TEST_CONFIG"

fs-uae "$MAGI80_TEST_CONFIG" >"$MAGI80_BUILD_DIR/fs-uae-output.txt" 2>&1 &
MAGI80_EMULATOR_PID="$!"

for ((second = 0; second < MAGI80_TIMEOUT_SECONDS; ++second)); do
  if [ -f "$MAGI80_MARKER" ]; then
    break
  fi
  if ! /bin/kill -0 "$MAGI80_EMULATOR_PID" 2>/dev/null; then
    break
  fi
  /bin/sleep 1
done

stop_emulator
MAGI80_EMULATOR_PID=""

if [ ! -f "$MAGI80_MARKER" ]; then
  printf 'FS-UAE did not produce the runtime marker within %s seconds.\n' "$MAGI80_TIMEOUT_SECONDS" >&2
  printf 'Emulator output: %s\n' "$MAGI80_BUILD_DIR/fs-uae-output.txt" >&2
  exit 1
fi

if ! /usr/bin/diff -u "$MAGI80_EXPECTED" "$MAGI80_MARKER"; then
  printf 'The full-system runtime output did not match the expected result.\n' >&2
  exit 1
fi

printf 'PASS  staged MAGI-80 Hunk executable launched from MAGI80: in FS-UAE\n'
printf 'PASS  AmigaOS redirected output reached the writable host directory\n'
