#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MIGA80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIGA80_LOCAL_CONFIG="${MIGA80_LOCAL_CONFIG:-$MIGA80_PROJECT_ROOT/config/fs-uae/local.env}"
MIGA80_SOURCE_ADF="${1:-$MIGA80_PROJECT_ROOT/build/distribution/miga80-exclusive-graphics-test.adf}"
MIGA80_RUN_DIR="$MIGA80_PROJECT_ROOT/build/fs-uae-physical-adf"
MIGA80_RUN_ADF="$MIGA80_RUN_DIR/miga80-exclusive-graphics-test-run.adf"
MIGA80_SNAPSHOT_ADF="$MIGA80_RUN_DIR/poll-snapshot.adf"
MIGA80_CONFIG="$MIGA80_RUN_DIR/a1200-pal-physical-adf.fs-uae"
MIGA80_REPORT="$MIGA80_PROJECT_ROOT/build/reports/exclusive-graphics-adf-fs-uae.txt"
MIGA80_CANDIDATE_REPORT="$MIGA80_RUN_DIR/candidate-report.txt"
MIGA80_TIMEOUT_SECONDS="${MIGA80_FS_UAE_TIMEOUT_SECONDS:-240}"
MIGA80_POLL_SECONDS="${MIGA80_FS_UAE_POLL_SECONDS:-5}"
MIGA80_EMULATOR_PID=""

stop_emulator() {
  local attempt

  if [ -z "$MIGA80_EMULATOR_PID" ] ||
     ! /bin/kill -0 "$MIGA80_EMULATOR_PID" 2>/dev/null; then
    return
  fi
  /bin/kill -INT "$MIGA80_EMULATOR_PID" 2>/dev/null || true
  for attempt in {1..20}; do
    if ! /bin/kill -0 "$MIGA80_EMULATOR_PID" 2>/dev/null; then
      wait "$MIGA80_EMULATOR_PID" 2>/dev/null || true
      return
    fi
    /bin/sleep 0.25
  done
  /bin/kill -TERM "$MIGA80_EMULATOR_PID" 2>/dev/null || true
  wait "$MIGA80_EMULATOR_PID" 2>/dev/null || true
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

if [ ! -f "$MIGA80_LOCAL_CONFIG" ]; then
  printf 'Local FS-UAE configuration not found: %s\n' "$MIGA80_LOCAL_CONFIG" >&2
  exit 1
fi
if [ ! -f "$MIGA80_SOURCE_ADF" ]; then
  printf 'Physical-test ADF not found: %s\n' "$MIGA80_SOURCE_ADF" >&2
  exit 1
fi

# shellcheck disable=SC1090
source "$MIGA80_LOCAL_CONFIG"
MIGA80_KICKSTART="${MIGA80_KICKSTART_30:-${MIGA80_KICKSTART_31:-}}"
if [ -z "$MIGA80_KICKSTART" ] || [ ! -f "$MIGA80_KICKSTART" ]; then
  printf 'Configure a licensed A1200 Kickstart 3.0 or 3.1 ROM.\n' >&2
  exit 1
fi

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
case "$MIGA80_POLL_SECONDS" in
  ''|*[!0-9]*)
    printf 'MIGA80_FS_UAE_POLL_SECONDS must be a positive integer.\n' >&2
    exit 1
    ;;
esac
if [ "$MIGA80_POLL_SECONDS" -eq 0 ]; then
  printf 'MIGA80_FS_UAE_POLL_SECONDS must be greater than zero.\n' >&2
  exit 1
fi

/bin/mkdir -p "$MIGA80_RUN_DIR" "$(dirname "$MIGA80_REPORT")"
/bin/cp "$MIGA80_SOURCE_ADF" "$MIGA80_RUN_ADF"
/bin/rm -f "$MIGA80_REPORT" "$MIGA80_CANDIDATE_REPORT" \
  "$MIGA80_SNAPSHOT_ADF"

{
  printf '[fs-uae]\n'
  printf 'amiga_model = A1200\n'
  printf 'chip_memory = 2048\n'
  printf 'fast_memory = 0\n'
  printf 'ntsc_mode = 0\n'
  printf 'joystick_port_1 = none\n'
  printf 'kickstart_file = %s\n' "$MIGA80_KICKSTART"
  printf 'floppy_drive_0 = %s\n' "$MIGA80_RUN_ADF"
  printf 'writable_floppy_images = 1\n'
} >"$MIGA80_CONFIG"

fs-uae "$MIGA80_CONFIG" >"$MIGA80_RUN_DIR/fs-uae-output.txt" 2>&1 &
MIGA80_EMULATOR_PID="$!"

for ((second = 0; second < MIGA80_TIMEOUT_SECONDS;
      second += MIGA80_POLL_SECONDS)); do
  if ! /bin/kill -0 "$MIGA80_EMULATOR_PID" 2>/dev/null; then
    break
  fi
  /bin/sleep "$MIGA80_POLL_SECONDS"
  /bin/cp "$MIGA80_RUN_ADF" "$MIGA80_SNAPSHOT_ADF"
  if xdftool "$MIGA80_SNAPSHOT_ADF" type RESULT.TXT \
       >"$MIGA80_CANDIDATE_REPORT" 2>/dev/null &&
     /usr/bin/tail -n 1 "$MIGA80_CANDIDATE_REPORT" |
       /usr/bin/grep -Eq '^result=(pass|fail)$'; then
    break
  fi
done

stop_emulator
MIGA80_EMULATOR_PID=""
xdfscan "$MIGA80_RUN_ADF" >/dev/null

if ! xdftool "$MIGA80_RUN_ADF" read RESULT.TXT "$MIGA80_REPORT" \
     >/dev/null 2>&1; then
  printf 'The booted ADF did not contain RESULT.TXT after %s seconds.\n' \
    "$MIGA80_TIMEOUT_SECONDS" >&2
  printf 'FS-UAE log: %s\n' "$MIGA80_RUN_DIR/fs-uae-output.txt" >&2
  exit 1
fi

MIGA80_RESULT=$(/usr/bin/tail -n 1 "$MIGA80_REPORT")
case "$MIGA80_RESULT" in
  result=pass)
    "$MIGA80_PROJECT_ROOT/scripts/validate-exclusive-graphics-benchmark-report.sh" \
      "$MIGA80_REPORT"
    ;;
  result=fail)
    printf 'The ADF wrote a controlled failure report:\n' >&2
    /bin/cat "$MIGA80_REPORT" >&2
    exit 1
    ;;
  result=running)
    printf 'The ADF is writable and the benchmark started, but it remained incomplete after %s seconds.\n' \
      "$MIGA80_TIMEOUT_SECONDS" >&2
    printf 'Extracted marker: %s\n' "$MIGA80_REPORT" >&2
    exit 1
    ;;
  *)
    printf 'The ADF contains a malformed or incomplete result report.\n' >&2
    printf 'Extracted report: %s\n' "$MIGA80_REPORT" >&2
    exit 1
    ;;
esac
printf 'PASS standalone writable ADF boot and on-disk report extraction\n'
