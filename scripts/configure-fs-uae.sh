#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

MIGA80_PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MIGA80_LOCAL_CONFIG="${MIGA80_LOCAL_CONFIG:-$MIGA80_PROJECT_ROOT/config/fs-uae/local.env}"
MIGA80_TEMPLATE_DIR="$MIGA80_PROJECT_ROOT/config/fs-uae"
MIGA80_OUTPUT_DIR="${MIGA80_FS_UAE_OUTPUT_DIR:-$MIGA80_PROJECT_ROOT/build/fs-uae}"

if [ ! -f "$MIGA80_LOCAL_CONFIG" ]; then
  printf 'Local FS-UAE configuration not found: %s\n' "$MIGA80_LOCAL_CONFIG" >&2
  printf 'Copy config/fs-uae/local.env.example to local.env and fill in local paths.\n' >&2
  exit 1
fi

# shellcheck disable=SC1090
source "$MIGA80_LOCAL_CONFIG"

MIGA80_KICKSTART_30="${MIGA80_KICKSTART_30:-}"
MIGA80_KICKSTART_31="${MIGA80_KICKSTART_31:-}"
MIGA80_KICKSTART_30_SHA256="${MIGA80_KICKSTART_30_SHA256:-}"
MIGA80_KICKSTART_31_SHA256="${MIGA80_KICKSTART_31_SHA256:-}"
MIGA80_WORKBENCH_30="${MIGA80_WORKBENCH_30:-}"
MIGA80_WORKBENCH_31="${MIGA80_WORKBENCH_31:-}"
MIGA80_ADF="${MIGA80_ADF:-}"
MIGA80_STAGING_DIR="${MIGA80_STAGING_DIR:-$MIGA80_PROJECT_ROOT/build/staging}"

if [ -z "$MIGA80_KICKSTART_30" ] && [ -z "$MIGA80_KICKSTART_31" ]; then
  printf 'Configure at least one licensed stock A1200 Kickstart ROM.\n' >&2
  exit 1
fi

if ! command -v romtool >/dev/null 2>&1; then
  printf 'romtool is required; install amitools before generating profiles.\n' >&2
  exit 1
fi

validate_rom() {
  local rom_file="$1"
  local expected_sha256="$2"
  local expected_revision="$3"
  local actual_sha256=""
  local actual_revision=""
  local rom_info=""

  if [ ! -f "$rom_file" ]; then
    printf 'Kickstart ROM not found: %s\n' "$rom_file" >&2
    exit 1
  fi

  if [ "$(/usr/bin/stat -f '%z' "$rom_file")" -ne 524288 ]; then
    printf 'Kickstart ROM is not 512 KiB: %s\n' "$rom_file" >&2
    exit 1
  fi

  if [ -n "$expected_sha256" ]; then
    actual_sha256="$(LC_ALL=C /usr/bin/shasum -a 256 "$rom_file" | /usr/bin/awk '{print $1}')"
    if [ "$actual_sha256" != "$expected_sha256" ]; then
      printf 'Kickstart ROM checksum mismatch: %s\n' "$rom_file" >&2
      exit 1
    fi
  fi

  rom_info="$(romtool info "$rom_file")"
  if ! /usr/bin/grep -Eq '^chk_sum[[:space:]]+ok$' <<<"$rom_info" ||
     ! /usr/bin/grep -Eq '^is_kick[[:space:]]+ok$' <<<"$rom_info"; then
    printf 'Kickstart ROM structure or internal checksum is invalid: %s\n' "$rom_file" >&2
    exit 1
  fi
  actual_revision="$(/usr/bin/awk '$1 == "rom_rev" { print $2 }' <<<"$rom_info")"
  if [ "$actual_revision" != "$expected_revision" ]; then
    printf 'Unexpected Kickstart revision in: %s\n' "$rom_file" >&2
    printf 'Expected %s, found %s.\n' "$expected_revision" "${actual_revision:-unknown}" >&2
    exit 1
  fi
}

render_template() {
  local template_file="$1"
  local output_file="$2"
  local rendered=""
  shift 2

  rendered="$(<"$template_file")"
  while [ "$#" -gt 0 ]; do
    rendered="${rendered//"$1"/"$2"}"
    shift 2
  done

  if /usr/bin/grep -q '@[A-Z0-9_]*@' <<<"$rendered"; then
    printf 'Unresolved placeholder while rendering %s\n' "$template_file" >&2
    exit 1
  fi

  printf '%s\n' "$rendered" >"$output_file"
  printf 'GENERATED %s\n' "$output_file"
}

render_media_profile() {
  local profile="$1"
  local kickstart_file="$2"
  local media_placeholder="$3"
  local media_path="$4"

  if [ -z "$media_path" ]; then
    printf 'SKIPPED   %s (local media path is not configured)\n' "$profile"
    return
  fi
  if [ ! -e "$media_path" ]; then
    printf 'SKIPPED   %s (local media does not exist: %s)\n' "$profile" "$media_path"
    return
  fi

  if [ "$media_placeholder" = '@AMIGAOS_SYSTEM@' ]; then
    /bin/mkdir -p "$MIGA80_STAGING_DIR"
    render_template \
      "$MIGA80_TEMPLATE_DIR/$profile.fs-uae.in" \
      "$MIGA80_OUTPUT_DIR/$profile.fs-uae" \
      '@KICKSTART_ROM@' "$kickstart_file" \
      "$media_placeholder" "$media_path" \
      '@PROJECT_STAGING@' "$MIGA80_STAGING_DIR"
  else
    render_template \
      "$MIGA80_TEMPLATE_DIR/$profile.fs-uae.in" \
      "$MIGA80_OUTPUT_DIR/$profile.fs-uae" \
      '@KICKSTART_ROM@' "$kickstart_file" \
      "$media_placeholder" "$media_path"
  fi
}

/bin/mkdir -p "$MIGA80_OUTPUT_DIR"

# These files are generated and ignored. Clear this exact set first so that a
# removed local ROM or media path cannot leave behind a misleading profile.
for profile in \
  a1200-pal-ks30-rom a1200-pal-ks31-rom \
  a1200-pal-ks30-hd a1200-pal-ks31-hd \
  a1200-pal-ks30-adf a1200-pal-ks31-adf; do
  /bin/rm -f "$MIGA80_OUTPUT_DIR/$profile.fs-uae"
done

if [ -n "$MIGA80_KICKSTART_30" ]; then
  validate_rom "$MIGA80_KICKSTART_30" "$MIGA80_KICKSTART_30_SHA256" '39.106'
  render_template \
    "$MIGA80_TEMPLATE_DIR/a1200-pal-ks30-rom.fs-uae.in" \
    "$MIGA80_OUTPUT_DIR/a1200-pal-ks30-rom.fs-uae" \
    '@KICKSTART_ROM@' "$MIGA80_KICKSTART_30"
  render_media_profile a1200-pal-ks30-hd "$MIGA80_KICKSTART_30" '@AMIGAOS_SYSTEM@' "$MIGA80_WORKBENCH_30"
  render_media_profile a1200-pal-ks30-adf "$MIGA80_KICKSTART_30" '@MIGA80_ADF@' "$MIGA80_ADF"
else
  printf 'SKIPPED   Kickstart 3.0 profiles (stock 39.106 ROM is not configured)\n'
fi

if [ -n "$MIGA80_KICKSTART_31" ]; then
  validate_rom "$MIGA80_KICKSTART_31" "$MIGA80_KICKSTART_31_SHA256" '40.68'
  render_template \
    "$MIGA80_TEMPLATE_DIR/a1200-pal-ks31-rom.fs-uae.in" \
    "$MIGA80_OUTPUT_DIR/a1200-pal-ks31-rom.fs-uae" \
    '@KICKSTART_ROM@' "$MIGA80_KICKSTART_31"
  render_media_profile a1200-pal-ks31-hd "$MIGA80_KICKSTART_31" '@AMIGAOS_SYSTEM@' "$MIGA80_WORKBENCH_31"
  render_media_profile a1200-pal-ks31-adf "$MIGA80_KICKSTART_31" '@MIGA80_ADF@' "$MIGA80_ADF"
else
  printf 'SKIPPED   Kickstart 3.1 profiles (stock 40.068 ROM is not configured)\n'
fi
