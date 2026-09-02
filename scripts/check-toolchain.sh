#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -uo pipefail

# C.UTF-8 is common on Linux but is not available on all supported macOS
# versions. A stable built-in locale also keeps version output parseable.
export LC_ALL=C
export LANG=C

MIGA80_FAILURES=0
MIGA80_TOOLCHAIN_PREFIX="${MIGA80_TOOLCHAIN_PREFIX:-/Users/fra/.local/m68k-amigaos}"

find_brew() {
  if command -v brew >/dev/null 2>&1; then
    command -v brew
  elif [ -x /opt/homebrew/bin/brew ]; then
    printf '%s\n' /opt/homebrew/bin/brew
  elif [ -x /usr/local/bin/brew ]; then
    printf '%s\n' /usr/local/bin/brew
  else
    return 1
  fi
}

first_version_line() {
  "$1" --version 2>&1 | /usr/bin/head -n 1
}

check_required_command() {
  local label="$1"
  local executable="$2"
  local supplied_version="${3:-}"
  local resolved=""
  local version=""

  resolved="$(command -v "$executable" 2>/dev/null || true)"
  if [ -z "$resolved" ] && [ -x "$executable" ]; then
    resolved="$executable"
  fi

  if [ -z "$resolved" ]; then
    printf 'MISSING  %-18s %s\n' "$label" "$executable"
    MIGA80_FAILURES=$((MIGA80_FAILURES + 1))
    return
  fi

  if [ -n "$supplied_version" ]; then
    version="$supplied_version"
  else
    version="$(first_version_line "$resolved" || true)"
  fi
  printf 'OK       %-18s %s' "$label" "$resolved"
  if [ -n "$version" ]; then
    printf ' — %s' "$version"
  fi
  printf '\n'
}

check_future_command() {
  local label="$1"
  local executable="$2"
  local resolved=""

  resolved="$(command -v "$executable" 2>/dev/null || true)"
  if [ -n "$resolved" ]; then
    printf 'READY    %-18s %s\n' "$label" "$resolved"
  else
    printf 'PENDING  %-18s not installed yet\n' "$label"
  fi
}

MIGA80_BREW_BIN="$(find_brew || true)"
if [ -z "$MIGA80_BREW_BIN" ]; then
  printf 'MISSING  Homebrew was not found in PATH or a standard installation path.\n'
  exit 1
fi

MIGA80_BREW_PREFIX="$($MIGA80_BREW_BIN --prefix)"
export PATH="$MIGA80_BREW_PREFIX/bin:$MIGA80_BREW_PREFIX/sbin:$PATH"

for formula in bison flex gettext texinfo; do
  MIGA80_FORMULA_PREFIX="$($MIGA80_BREW_BIN --prefix "$formula" 2>/dev/null || true)"
  if [ -n "$MIGA80_FORMULA_PREFIX" ]; then
    export PATH="$MIGA80_FORMULA_PREFIX/bin:$PATH"
  fi
done

for formula in coreutils gnu-sed gnu-tar grep make; do
  MIGA80_FORMULA_PREFIX="$($MIGA80_BREW_BIN --prefix "$formula" 2>/dev/null || true)"
  if [ -d "$MIGA80_FORMULA_PREFIX/libexec/gnubin" ]; then
    export PATH="$MIGA80_FORMULA_PREFIX/libexec/gnubin:$PATH"
  fi
done

if $MIGA80_BREW_BIN list --versions gcc@12 >/dev/null 2>&1; then
  MIGA80_GCC_FORMULA="gcc@12"
else
  MIGA80_GCC_FORMULA="gcc"
fi

MIGA80_GCC_PREFIX="$($MIGA80_BREW_BIN --prefix "$MIGA80_GCC_FORMULA" 2>/dev/null || true)"
if [ -n "$MIGA80_GCC_PREFIX" ]; then
  export PATH="$MIGA80_GCC_PREFIX/bin:$PATH"
fi

MIGA80_GCC_RELEASE="$($MIGA80_BREW_BIN list --versions "$MIGA80_GCC_FORMULA" 2>/dev/null | /usr/bin/awk '{print $NF}')"
MIGA80_GCC_MAJOR="${MIGA80_GCC_RELEASE%%.*}"
if [ -n "$MIGA80_GCC_MAJOR" ]; then
  MIGA80_GCC_COMMAND="gcc-$MIGA80_GCC_MAJOR"
  MIGA80_GXX_COMMAND="g++-$MIGA80_GCC_MAJOR"
else
  MIGA80_GCC_COMMAND="gcc"
  MIGA80_GXX_COMMAND="g++"
fi

printf 'MAGI-80 host toolchain check\n'
printf 'Host:     %s %s (%s)\n' "$(/usr/bin/sw_vers -productName)" "$(/usr/bin/sw_vers -productVersion)" "$(/usr/bin/uname -m)"
printf 'Homebrew: %s\n' "$MIGA80_BREW_PREFIX"
printf 'SDK:      %s\n' "$(/usr/bin/xcrun --show-sdk-path 2>/dev/null || printf 'not found')"
printf '\n'

check_required_command "Homebrew" "$MIGA80_BREW_BIN"
check_required_command "Git" git
check_required_command "Apple Clang" clang
check_required_command "GNU Bash" bash
check_required_command "GNU Make" gmake
check_required_command "Host GCC" "$MIGA80_GCC_COMMAND"
check_required_command "Host G++" "$MIGA80_GXX_COMMAND"
check_required_command "Autoconf" autoconf
check_required_command "Automake" automake
check_required_command "Bison" bison
check_required_command "Flex" flex
check_required_command "GNU sed" gsed
check_required_command "GNU tar" gtar
check_required_command "GNU grep" ggrep
check_required_command "wget" wget
check_required_command "XZ" xz
check_required_command "LHA" lha "$($MIGA80_BREW_BIN list --versions lhasa 2>/dev/null)"
check_required_command "Python" python3
check_required_command "pipx" pipx
check_required_command "FS-UAE" fs-uae

if [ -d "$MIGA80_TOOLCHAIN_PREFIX/bin" ]; then
  export PATH="$MIGA80_TOOLCHAIN_PREFIX/bin:$PATH"
fi

printf '\nAmiga target and disk tools\n'
check_future_command "Target GCC" m68k-amigaos-gcc
check_future_command "Target GDB" m68k-amigaos-gdb
check_future_command "Target linker" m68k-amigaos-ld
check_future_command "Target objdump" m68k-amigaos-objdump
check_future_command "VASM" vasmm68k_mot
check_future_command "ADF tools" xdftool
check_future_command "Hunk tools" hunktool
check_future_command "vamos" vamos

printf '\n'
if [ "$MIGA80_FAILURES" -ne 0 ]; then
  printf 'Host environment is not ready: %d required command(s) missing.\n' "$MIGA80_FAILURES"
  exit 1
fi

printf 'Required host environment is ready.\n'
