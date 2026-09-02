#!/usr/bin/env -S LC_ALL=C LANG=C bash

set -euo pipefail

# `DEBUG=release` is commonly injected by macOS development shells.  Clib2
# treats DEBUG as raw compiler flags, which would pass the word "release" to
# m68k-amigaos-gcc and abort every compilation.
unset DEBUG

MIGA80_TOOLCHAIN_SOURCE="${MIGA80_TOOLCHAIN_SOURCE:-/Users/fra/dev/toolchains/m68k-amigaos-gcc}"
MIGA80_TOOLCHAIN_PREFIX="${MIGA80_TOOLCHAIN_PREFIX:-/Users/fra/.local/m68k-amigaos}"
MIGA80_TOOLCHAIN_NDK="${MIGA80_TOOLCHAIN_NDK:-3.2}"
MIGA80_TOOLCHAIN_TARGET="${MIGA80_TOOLCHAIN_TARGET:-all}"
MIGA80_TOOLCHAIN_UPDATE="${MIGA80_TOOLCHAIN_UPDATE:-0}"
MIGA80_TOOLCHAIN_JOBS="${MIGA80_TOOLCHAIN_JOBS:-$(/usr/sbin/sysctl -n hw.logicalcpu)}"
MIGA80_MIN_FREE_KIB="${MIGA80_MIN_FREE_KIB:-10485760}"
MIGA80_HOST_GCC_FORMULA="${MIGA80_HOST_GCC_FORMULA:-gcc@12}"

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

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    printf 'Required command not found: %s\n' "$1" >&2
    exit 1
  fi
}

check_free_space() {
  local available_kib=""

  available_kib="$(/bin/df -Pk "$MIGA80_SOURCE_PARENT" | /usr/bin/awk 'NR == 2 { print $4 }')"
  if [ "$available_kib" -lt "$MIGA80_MIN_FREE_KIB" ]; then
    printf 'Insufficient free space: %s KiB available, %s KiB required.\n' \
      "$available_kib" "$MIGA80_MIN_FREE_KIB" >&2
    exit 1
  fi
}

MIGA80_BREW_BIN="$(find_brew || true)"
if [ -z "$MIGA80_BREW_BIN" ]; then
  printf 'Homebrew was not found. Run scripts/check-toolchain.sh first.\n' >&2
  exit 1
fi

MIGA80_BREW_PREFIX="$($MIGA80_BREW_BIN --prefix)"
export PATH="$MIGA80_BREW_PREFIX/bin:$MIGA80_BREW_PREFIX/sbin:$PATH"

for formula in bison flex gettext texinfo; do
  MIGA80_FORMULA_PREFIX="$($MIGA80_BREW_BIN --prefix "$formula")"
  export PATH="$MIGA80_FORMULA_PREFIX/bin:$PATH"
done

for formula in coreutils gnu-sed gnu-tar grep make; do
  MIGA80_FORMULA_PREFIX="$($MIGA80_BREW_BIN --prefix "$formula")"
  export PATH="$MIGA80_FORMULA_PREFIX/libexec/gnubin:$PATH"
done

require_command git
require_command gmake
require_command bash

MIGA80_HOST_GCC_PREFIX="$($MIGA80_BREW_BIN --prefix "$MIGA80_HOST_GCC_FORMULA")"
export PATH="$MIGA80_HOST_GCC_PREFIX/bin:$PATH"

MIGA80_GCC_RELEASE="$($MIGA80_BREW_BIN list --versions "$MIGA80_HOST_GCC_FORMULA" | /usr/bin/awk '{print $NF}')"
MIGA80_GCC_MAJOR="${MIGA80_GCC_RELEASE%%.*}"
MIGA80_HOST_CC="gcc-$MIGA80_GCC_MAJOR"
MIGA80_HOST_CXX="g++-$MIGA80_GCC_MAJOR"
require_command "$MIGA80_HOST_CC"
require_command "$MIGA80_HOST_CXX"

# Keep the native compiler in the environment instead of MAKEFLAGS.  Several
# upstream runtime makefiles deliberately replace CC with m68k-amigaos-gcc;
# a command-line CC assignment would override those target-specific defaults.
export CC="$MIGA80_HOST_CC"
export CXX="$MIGA80_HOST_CXX"

MIGA80_SOURCE_PARENT="$(/usr/bin/dirname "$MIGA80_TOOLCHAIN_SOURCE")"
/bin/mkdir -p "$MIGA80_SOURCE_PARENT"
check_free_space

MIGA80_NEW_CLONE=0
if [ ! -e "$MIGA80_TOOLCHAIN_SOURCE" ]; then
  git clone https://github.com/AmigaPorts/m68k-amigaos-gcc "$MIGA80_TOOLCHAIN_SOURCE"
  MIGA80_NEW_CLONE=1
elif [ ! -d "$MIGA80_TOOLCHAIN_SOURCE/.git" ]; then
  printf 'Toolchain source path exists but is not a Git repository: %s\n' \
    "$MIGA80_TOOLCHAIN_SOURCE" >&2
  exit 1
fi

cd "$MIGA80_TOOLCHAIN_SOURCE"

if [ "$MIGA80_NEW_CLONE" -eq 1 ] || [ "$MIGA80_TOOLCHAIN_UPDATE" = "1" ]; then
  gmake update \
    "GDB_CC=$MIGA80_HOST_CC" \
    "GDB_CXX=$MIGA80_HOST_CXX" \
    "NDK=$MIGA80_TOOLCHAIN_NDK" \
    "PREFIX=$MIGA80_TOOLCHAIN_PREFIX" \
    "SHELL=$(brew --prefix bash)/bin/bash"
fi

check_free_space

# SFDC uses shared state files while generating NDK headers, so this phase is
# intentionally serial even when the compiler build itself is parallel.
gmake ndk ndk13 \
  "GDB_CC=$MIGA80_HOST_CC" \
  "GDB_CXX=$MIGA80_HOST_CXX" \
  "NDK=$MIGA80_TOOLCHAIN_NDK" \
  "PREFIX=$MIGA80_TOOLCHAIN_PREFIX" \
  "SHELL=$(brew --prefix bash)/bin/bash" \
  -j1

printf 'Building AmigaPorts toolchain\n'
printf '  source: %s\n' "$MIGA80_TOOLCHAIN_SOURCE"
printf '  source revision: %s\n' "$(git rev-parse HEAD)"
printf '  prefix: %s\n' "$MIGA80_TOOLCHAIN_PREFIX"
printf '  NDK: %s\n' "$MIGA80_TOOLCHAIN_NDK"
printf '  target: %s\n' "$MIGA80_TOOLCHAIN_TARGET"
printf '  jobs: %s\n' "$MIGA80_TOOLCHAIN_JOBS"
printf '  host compiler: %s\n' "$MIGA80_HOST_CC"

gmake \
  "$MIGA80_TOOLCHAIN_TARGET" \
  "GDB_CC=$MIGA80_HOST_CC" \
  "GDB_CXX=$MIGA80_HOST_CXX" \
  "NDK=$MIGA80_TOOLCHAIN_NDK" \
  "PREFIX=$MIGA80_TOOLCHAIN_PREFIX" \
  "SHELL=$(brew --prefix bash)/bin/bash" \
  -j"$MIGA80_TOOLCHAIN_JOBS"

printf 'Toolchain build completed: %s\n' "$MIGA80_TOOLCHAIN_PREFIX"
