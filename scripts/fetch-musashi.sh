#!/bin/sh

set -eu

LC_ALL=C
LANG=C
export LC_ALL LANG

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
lock_file="$project_root/toolchain/versions.lock"
destination=${1:-"$project_root/build/deps/musashi"}

lock_value()
{
    key=$1
    value=$(sed -n "s/^${key}=//p" "$lock_file")
    if [ -z "$value" ]; then
        printf 'Missing %s in %s\n' "$key" "$lock_file" >&2
        exit 1
    fi
    printf '%s\n' "$value"
}

commit=$(lock_value musashi_commit)
expected_sha256=$(lock_value musashi_archive_sha256)
archive_url="https://github.com/kstenerud/Musashi/archive/${commit}.tar.gz"
stamp="$destination/.magi80-source-revision"

if [ -f "$stamp" ] && [ "$(sed -n '1p' "$stamp")" = "$commit" ]; then
    printf '%s\n%s\n' "$commit" "$expected_sha256" >"$stamp"
    exit 0
fi

if [ -e "$destination" ]; then
    printf 'Musashi destination exists without the expected revision: %s\n' \
        "$destination" >&2
    printf 'Remove the stale build dependency with gmake clean and retry.\n' >&2
    exit 1
fi

work_dir=$(mktemp -d "${TMPDIR:-/tmp}/magi80-musashi.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT HUP INT TERM
archive="$work_dir/musashi.tar.gz"
source_dir="$work_dir/source"

if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$archive_url" -o "$archive"
elif command -v wget >/dev/null 2>&1; then
    wget -q "$archive_url" -O "$archive"
else
    printf 'curl or wget is required to fetch Musashi.\n' >&2
    exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
    actual_sha256=$(sha256sum "$archive" | awk '{print $1}')
else
    actual_sha256=$(shasum -a 256 "$archive" | awk '{print $1}')
fi

if [ "$actual_sha256" != "$expected_sha256" ]; then
    printf 'Musashi archive checksum mismatch.\n' >&2
    printf 'Expected: %s\nActual:   %s\n' "$expected_sha256" \
        "$actual_sha256" >&2
    exit 1
fi

mkdir -p "$source_dir" "$(dirname -- "$destination")"
tar -xzf "$archive" -C "$source_dir" --strip-components=1
printf '%s\n%s\n' "$commit" "$expected_sha256" \
    >"$source_dir/.magi80-source-revision"
mv "$source_dir" "$destination"

printf 'Fetched Musashi %s\n' "$commit"
