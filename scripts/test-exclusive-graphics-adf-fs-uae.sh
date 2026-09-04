#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MAGI80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MAGI80_LOCAL_CONFIG="${MAGI80_LOCAL_CONFIG:-$MAGI80_PROJECT_ROOT/config/fs-uae/local.env}"
MAGI80_SOURCE_ADF="${1:-$MAGI80_PROJECT_ROOT/build/distribution/magi80-exclusive-graphics-test.adf}"
MAGI80_RUN_DIR="$MAGI80_PROJECT_ROOT/build/fs-uae-physical-adf"
MAGI80_RUN_ADF="$MAGI80_RUN_DIR/magi80-exclusive-graphics-test-run.adf"
MAGI80_SNAPSHOT_ADF="$MAGI80_RUN_DIR/poll-snapshot.adf"
MAGI80_CONFIG="$MAGI80_RUN_DIR/a1200-pal-physical-adf.fs-uae"
MAGI80_REPORT="$MAGI80_PROJECT_ROOT/build/reports/exclusive-graphics-adf-fs-uae.txt"
MAGI80_CANDIDATE_REPORT="$MAGI80_RUN_DIR/candidate-report.txt"
MAGI80_TIMEOUT_SECONDS="${MAGI80_FS_UAE_TIMEOUT_SECONDS:-240}"
MAGI80_POLL_SECONDS="${MAGI80_FS_UAE_POLL_SECONDS:-5}"
MAGI80_EMULATOR_PID=""

stop_emulator() {
  local attempt

  if [ -z "$MAGI80_EMULATOR_PID" ] ||
     ! /bin/kill -0 "$MAGI80_EMULATOR_PID" 2>/dev/null; then
    return
  fi
  /bin/kill -INT "$MAGI80_EMULATOR_PID" 2>/dev/null || true
  for attempt in {1..20}; do
    if ! /bin/kill -0 "$MAGI80_EMULATOR_PID" 2>/dev/null; then
      wait "$MAGI80_EMULATOR_PID" 2>/dev/null || true
      return
    fi
    /bin/sleep 0.25
  done
  /bin/kill -TERM "$MAGI80_EMULATOR_PID" 2>/dev/null || true
  wait "$MAGI80_EMULATOR_PID" 2>/dev/null || true
}

cleanup() {
  stop_emulator
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

for command in fs-uae xdftool xdfscan; do
  if ! command -v "$command" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "$command" >&2
    exit 1
  fi
done

if [ ! -f "$MAGI80_LOCAL_CONFIG" ]; then
  printf 'Local FS-UAE configuration not found: %s\n' "$MAGI80_LOCAL_CONFIG" >&2
  exit 1
fi
if [ ! -f "$MAGI80_SOURCE_ADF" ]; then
  printf 'Physical-test ADF not found: %s\n' "$MAGI80_SOURCE_ADF" >&2
  exit 1
fi

# shellcheck disable=SC1090
source "$MAGI80_LOCAL_CONFIG"
MAGI80_KICKSTART="${MAGI80_KICKSTART_30:-${MAGI80_KICKSTART_31:-}}"
if [ -z "$MAGI80_KICKSTART" ] || [ ! -f "$MAGI80_KICKSTART" ]; then
  printf 'Configure a licensed A1200 Kickstart 3.0 or 3.1 ROM.\n' >&2
  exit 1
fi

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
case "$MAGI80_POLL_SECONDS" in
  ''|*[!0-9]*)
    printf 'MAGI80_FS_UAE_POLL_SECONDS must be a positive integer.\n' >&2
    exit 1
    ;;
esac
if [ "$MAGI80_POLL_SECONDS" -eq 0 ]; then
  printf 'MAGI80_FS_UAE_POLL_SECONDS must be greater than zero.\n' >&2
  exit 1
fi

/bin/mkdir -p "$MAGI80_RUN_DIR" "$(dirname "$MAGI80_REPORT")"
/bin/cp "$MAGI80_SOURCE_ADF" "$MAGI80_RUN_ADF"
/bin/rm -f "$MAGI80_REPORT" "$MAGI80_CANDIDATE_REPORT" \
  "$MAGI80_SNAPSHOT_ADF"

{
  printf '[fs-uae]\n'
  printf 'amiga_model = A1200\n'
  printf 'chip_memory = 2048\n'
  printf 'fast_memory = 0\n'
  printf 'ntsc_mode = 0\n'
  printf 'joystick_port_1 = none\n'
  printf 'kickstart_file = %s\n' "$MAGI80_KICKSTART"
  printf 'floppy_drive_0 = %s\n' "$MAGI80_RUN_ADF"
  printf 'writable_floppy_images = 1\n'
} >"$MAGI80_CONFIG"

fs-uae "$MAGI80_CONFIG" >"$MAGI80_RUN_DIR/fs-uae-output.txt" 2>&1 &
MAGI80_EMULATOR_PID="$!"

for ((second = 0; second < MAGI80_TIMEOUT_SECONDS;
      second += MAGI80_POLL_SECONDS)); do
  if ! /bin/kill -0 "$MAGI80_EMULATOR_PID" 2>/dev/null; then
    break
  fi
  /bin/sleep "$MAGI80_POLL_SECONDS"
  /bin/cp "$MAGI80_RUN_ADF" "$MAGI80_SNAPSHOT_ADF"
  if xdftool "$MAGI80_SNAPSHOT_ADF" type RESULT.TXT \
       >"$MAGI80_CANDIDATE_REPORT" 2>/dev/null &&
     /usr/bin/tail -n 1 "$MAGI80_CANDIDATE_REPORT" |
       /usr/bin/grep -Eq '^result=(pass|fail)$'; then
    break
  fi
done

stop_emulator
MAGI80_EMULATOR_PID=""
xdfscan "$MAGI80_RUN_ADF" >/dev/null

if ! xdftool "$MAGI80_RUN_ADF" read RESULT.TXT "$MAGI80_REPORT" \
     >/dev/null 2>&1; then
  printf 'The booted ADF did not contain RESULT.TXT after %s seconds.\n' \
    "$MAGI80_TIMEOUT_SECONDS" >&2
  printf 'FS-UAE log: %s\n' "$MAGI80_RUN_DIR/fs-uae-output.txt" >&2
  exit 1
fi

MAGI80_RESULT=$(/usr/bin/tail -n 1 "$MAGI80_REPORT")
case "$MAGI80_RESULT" in
  result=pass)
    "$MAGI80_PROJECT_ROOT/scripts/validate-exclusive-graphics-benchmark-report.sh" \
      "$MAGI80_REPORT"
    ;;
  result=fail)
    printf 'The ADF wrote a controlled failure report:\n' >&2
    /bin/cat "$MAGI80_REPORT" >&2
    exit 1
    ;;
  result=running)
    printf 'The ADF is writable and the benchmark started, but it remained incomplete after %s seconds.\n' \
      "$MAGI80_TIMEOUT_SECONDS" >&2
    printf 'Extracted marker: %s\n' "$MAGI80_REPORT" >&2
    exit 1
    ;;
  *)
    printf 'The ADF contains a malformed or incomplete result report.\n' >&2
    printf 'Extracted report: %s\n' "$MAGI80_REPORT" >&2
    exit 1
    ;;
esac
printf 'PASS standalone writable ADF boot and on-disk report extraction\n'
