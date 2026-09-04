#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MIGA80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIGA80_BUILD_DIR="$MIGA80_PROJECT_ROOT/build/fs-uae-smoke"
MIGA80_STAGING_DIR="$MIGA80_PROJECT_ROOT/build/staging"
MIGA80_BASE_CONFIG="$MIGA80_PROJECT_ROOT/build/fs-uae/a1200-pal-ks30-hd.fs-uae"
MIGA80_TEST_CONFIG="$MIGA80_BUILD_DIR/a1200-pal-ks30-runtime-smoke.fs-uae"
MIGA80_TEST_ADF="$MIGA80_BUILD_DIR/runtime-smoke.adf"
MIGA80_MARKER="$MIGA80_STAGING_DIR/fs-uae-smoke.out"
MIGA80_STAGED_PROGRAM="$MIGA80_STAGING_DIR/miga80"
MIGA80_STAGED_BACKUP="$MIGA80_BUILD_DIR/staged-program.backup"
MIGA80_SOURCE_PROGRAM="${1:-}"
MIGA80_EXPECTED="${2:-$MIGA80_PROJECT_ROOT/tests/smoke/hosted-bootstrap/expected.txt}"
MIGA80_TIMEOUT_SECONDS="${MIGA80_FS_UAE_TIMEOUT_SECONDS:-30}"
MIGA80_FAST_MEMORY_KIB="${MIGA80_FS_UAE_FAST_MEMORY_KIB:-0}"
MIGA80_PIPX_BIN="${MIGA80_PIPX_BIN:-$HOME/.local/bin}"
MIGA80_EMULATOR_PID=""
MIGA80_RESTORE_STAGED_PROGRAM=0
MIGA80_REMOVE_STAGED_PROGRAM=0

export PATH="$MIGA80_PIPX_BIN:$PATH"

if [ "$#" -gt 2 ]; then
  printf 'Usage: %s [amiga-program [expected-output|-]]\n' "$0" >&2
  exit 1
fi

stop_emulator() {
  local attempt
  local process_state

  if [ -z "$MIGA80_EMULATOR_PID" ] || ! /bin/kill -0 "$MIGA80_EMULATOR_PID" 2>/dev/null; then
    return
  fi

  /bin/kill -INT "$MIGA80_EMULATOR_PID" 2>/dev/null || true
  for attempt in {1..20}; do
    if ! /bin/kill -0 "$MIGA80_EMULATOR_PID" 2>/dev/null; then
      wait "$MIGA80_EMULATOR_PID" 2>/dev/null || true
      return
    fi
    process_state="$(/bin/ps -p "$MIGA80_EMULATOR_PID" -o state= 2>/dev/null | /usr/bin/awk '{ print $1 }')"
    case "$process_state" in
      ''|Z*)
        wait "$MIGA80_EMULATOR_PID" 2>/dev/null || true
        return
        ;;
    esac
    /bin/sleep 0.25
  done

  /bin/kill -TERM "$MIGA80_EMULATOR_PID" 2>/dev/null || true
  wait "$MIGA80_EMULATOR_PID" 2>/dev/null || true
}

restore_staged_program() {
  if [ "$MIGA80_RESTORE_STAGED_PROGRAM" = "1" ] && [ -f "$MIGA80_STAGED_BACKUP" ]; then
    /bin/cp "$MIGA80_STAGED_BACKUP" "$MIGA80_STAGED_PROGRAM"
    /bin/chmod 755 "$MIGA80_STAGED_PROGRAM"
    /bin/rm -f "$MIGA80_STAGED_BACKUP"
  elif [ "$MIGA80_REMOVE_STAGED_PROGRAM" = "1" ]; then
    /bin/rm -f "$MIGA80_STAGED_PROGRAM"
  fi
}

cleanup() {
  stop_emulator
  restore_staged_program
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

for command in fs-uae gmake xdftool xdfscan; do
  if ! command -v "$command" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "$command" >&2
    exit 1
  fi
done

case "$MIGA80_TIMEOUT_SECONDS" in
  ''|*[!0-9]*)
    printf 'MIGA80_FS_UAE_TIMEOUT_SECONDS must be a positive integer.\n' >&2
    exit 1
    ;;
esac
if [ "$MIGA80_TIMEOUT_SECONDS" -eq 0 ]; then
  printf 'MIGA80_FS_UAE_TIMEOUT_SECONDS must be greater than zero.\n' >&2
  exit 1
fi
case "$MIGA80_FAST_MEMORY_KIB" in
  ''|*[!0-9]*)
    printf 'MIGA80_FS_UAE_FAST_MEMORY_KIB must be a non-negative integer.\n' >&2
    exit 1
    ;;
esac

/bin/mkdir -p "$MIGA80_BUILD_DIR" "$MIGA80_STAGING_DIR"

if [ -z "$MIGA80_SOURCE_PROGRAM" ]; then
  gmake -C "$MIGA80_PROJECT_ROOT" stage >/dev/null
elif [ ! -f "$MIGA80_SOURCE_PROGRAM" ]; then
  printf 'Amiga program not found: %s\n' "$MIGA80_SOURCE_PROGRAM" >&2
  exit 1
else
  /bin/rm -f "$MIGA80_STAGED_BACKUP"
  if [ -f "$MIGA80_STAGED_PROGRAM" ]; then
    /bin/cp "$MIGA80_STAGED_PROGRAM" "$MIGA80_STAGED_BACKUP"
    MIGA80_RESTORE_STAGED_PROGRAM=1
  else
    MIGA80_REMOVE_STAGED_PROGRAM=1
  fi
  /bin/cp "$MIGA80_SOURCE_PROGRAM" "$MIGA80_STAGED_PROGRAM"
  /bin/chmod 755 "$MIGA80_STAGED_PROGRAM"
fi

if [ "$MIGA80_EXPECTED" != "-" ] && [ ! -f "$MIGA80_EXPECTED" ]; then
  printf 'Expected-output file not found: %s\n' "$MIGA80_EXPECTED" >&2
  exit 1
fi

"$MIGA80_PROJECT_ROOT/scripts/configure-fs-uae.sh" >/dev/null

if [ ! -f "$MIGA80_BASE_CONFIG" ]; then
  printf 'The Kickstart 3.0 HD profile is unavailable; configure MIGA80_WORKBENCH_30.\n' >&2
  exit 1
fi

/bin/rm -f "$MIGA80_MARKER"

xdftool -f "$MIGA80_TEST_ADF" \
  create \
  + format MIGA80TEST ofs \
  + makedir S \
  + write "$MIGA80_PROJECT_ROOT/tests/smoke/fs-uae/Startup-Sequence" S/Startup-Sequence \
  + boot install >/dev/null
xdfscan "$MIGA80_TEST_ADF" >/dev/null

/bin/cp "$MIGA80_BASE_CONFIG" "$MIGA80_TEST_CONFIG"
printf 'fast_memory = %s\n' "$MIGA80_FAST_MEMORY_KIB" >>"$MIGA80_TEST_CONFIG"
printf 'floppy_drive_0 = %s\n' "$MIGA80_TEST_ADF" >>"$MIGA80_TEST_CONFIG"

fs-uae "$MIGA80_TEST_CONFIG" >"$MIGA80_BUILD_DIR/fs-uae-output.txt" 2>&1 &
MIGA80_EMULATOR_PID="$!"

for ((second = 0; second < MIGA80_TIMEOUT_SECONDS; ++second)); do
  if [ -f "$MIGA80_MARKER" ]; then
    if [ "$MIGA80_EXPECTED" = "-" ]; then
      if /usr/bin/grep -Eq '^result=(pass|fail)$' "$MIGA80_MARKER" 2>/dev/null; then
        break
      fi
    elif /usr/bin/grep -qx 'result=fail' "$MIGA80_MARKER" 2>/dev/null ||
         /usr/bin/cmp -s "$MIGA80_EXPECTED" "$MIGA80_MARKER"; then
      break
    fi
  fi
  if ! /bin/kill -0 "$MIGA80_EMULATOR_PID" 2>/dev/null; then
    break
  fi
  /bin/sleep 1
done

stop_emulator
MIGA80_EMULATOR_PID=""

if [ ! -f "$MIGA80_MARKER" ]; then
  printf 'FS-UAE did not produce the runtime marker within %s seconds.\n' "$MIGA80_TIMEOUT_SECONDS" >&2
  printf 'Emulator output: %s\n' "$MIGA80_BUILD_DIR/fs-uae-output.txt" >&2
  exit 1
fi

if [ "$MIGA80_EXPECTED" = "-" ]; then
  if ! /usr/bin/tail -n 1 "$MIGA80_MARKER" |
       /usr/bin/grep -qx 'result=pass'; then
    printf 'The full-system runtime output did not report success.\n' >&2
    exit 1
  fi
else
  if ! /usr/bin/diff -u "$MIGA80_EXPECTED" "$MIGA80_MARKER"; then
    printf 'The full-system runtime output did not match the expected result.\n' >&2
    exit 1
  fi
fi

printf 'PASS  staged MIGA-80 Hunk executable launched from MIGA80: in FS-UAE\n'
printf 'PASS  AmigaOS redirected output reached the writable host directory\n'
