#!/usr/bin/env bash
# Build a relocatable, user-owned runtime for this Arch/Omarchy machine.
set -euo pipefail
umask 077
if [[ ${1:-} == --help || ${1:-} == -h ]]; then
  printf '%s\n' 'Usage: tools/package-local.sh NEW_DIRECTORY' \
    'Build and stage the release editor, helpers, assets and notices in a new directory.' \
    'Run NEW_DIRECTORY/bin/apollo [PROJECT]. Uses system Qt; no system installation.'
  exit 0
fi
[[ $# == 1 && -n $1 ]] || { printf 'Expected one new destination; see --help.\n' >&2; exit 2; }
for command in mise realpath mktemp ldd pkg-config pacman rg sha256sum sort xargs; do
  command -v "$command" >/dev/null || { printf 'Missing command: %s\n' "$command" >&2; exit 1; }
done
[[ ! -e $1 && ! -L $1 ]] || { printf 'Destination already exists: %s\n' "$1" >&2; exit 1; }
destination=$(realpath -m -- "$1")
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
cd -- "$project_dir"
mise exec -- cmake --preset release
mise exec -- cmake --build --preset release --target apollo_editor
mkdir -p -- "$(dirname -- "$destination")"
staging=$(mktemp -d "$(dirname -- "$destination")/.apollo-package-XXXXXX")
trap 'rm -rf -- "$staging"' EXIT
mise exec -- cmake --install build/release --prefix "$staging" --component Runtime
{
  printf 'Apollo local runtime\nBuilt: %s\n' "$(date -Iseconds)"
  printf 'Source revision: '; git rev-parse HEAD
  printf 'Working tree (empty means clean):\n'; git status --short
  printf '\nSystem: '; uname -srmo
  printf '\nBuild tools:\n'; mise exec -- cmake --version; mise exec -- ninja --version; c++ --version
  printf '\nQt build dependencies:\n'; pkg-config --modversion Qt6Core Qt6Gui Qt6Quick
  printf '\nArch runtime package versions:\n'; pacman -Q qt6-base qt6-declarative qt6-wayland qt6-svg gcc-libs glibc
  printf '\nDynamic runtime dependencies (system paths):\n'
  for executable in apollo_editor pdf_export fountain_export; do
    printf '\n%s\n' "$executable"
    ldd "$staging/lib/apollo/$executable"
  done
} > "$staging/BUILD-INFO.txt"
if rg -q '=> not found' "$staging/BUILD-INFO.txt"; then
  printf 'Unresolved shared library; package not published:\n' >&2
  cat "$staging/BUILD-INFO.txt" >&2
  exit 1
fi
cp -- docs/local-launch.md "$staging/LOCAL-LAUNCH.md"
(
  cd -- "$staging"
  rg --files --hidden --no-ignore --follow -0 -g '!SHA256SUMS' | sort -z | xargs -0 sha256sum > SHA256SUMS
)
# Refuse a destination created while the build ran, including an empty folder.
mv -T --no-clobber -- "$staging" "$destination"
[[ ! -e $staging ]] || { printf 'Destination appeared during packaging; nothing replaced.\n' >&2; exit 1; }
printf '\nLocal runtime: %s\nLaunch: "%s/bin/apollo"\n' "$destination" "$destination"
