#!/usr/bin/env bash
# One native runner for task 5c; build/check first, measure only after it exits.
set -euo pipefail
umask 077
export PYTHONDONTWRITEBYTECODE=1
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
cd -- "$project_dir"
mode=${1:-}
case "$mode" in
  --help|-h) printf '%s\n' 'Usage: tools/check-writing-responsiveness.sh [--smoke]' \
    'Build and run release checks, then current-writer native primary/stress timings and output verification.' \
    'Uses isolated generated projects. Keep each window visible and focused; leave keyboard/mouse alone.' \
    'QT_IM_MODULE defaults to compose; set fcitx only when its Qt plugin and daemon are installed.' \
    '--smoke: short primary-workload harness check; no performance acceptance.'; exit 0 ;;
  ''|--smoke) ;;
  *) printf 'Unknown option: %s\n' "$mode" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || exit 2
for command in mise uv timeout tee mktemp node pdftotext pdftohtml pdfinfo pdffonts date uname lscpu pkg-config; do
  command -v "$command" >/dev/null || { printf 'Missing required command: %s\n' "$command" >&2; exit 1; }
done
input_module=${QT_IM_MODULE:-compose}
source tools/check-wayland.sh
prepare_wayland
export QT_IM_MODULE=$input_module
mkdir -p build
output=$(mktemp -d "$project_dir/build/writing-responsiveness-XXXXXX")
trap 'printf "Stopped; saved logs: %s\n" "$output" >&2' ERR
printf 'Logs: %s\nNative Wayland/OpenGL; input module: %s\n' "$output" "$QT_IM_MODULE"
printf 'Checkpoint: keep the disposable measurement window focused; leave keyboard/mouse alone.\n'
{ date -Iseconds; uname -a; lscpu; mise exec -- cmake --version; pkg-config --modversion Qt6Quick; } > "$output/environment.log"
# Do not overlap measurement with compilation or another test suite.
env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic mise run check 2>&1 | tee "$output/release.log"
mise exec -- cmake --build --preset release --target measure_fountain_workflow 2>&1 | tee "$output/build.log"
arguments=(--output "$output")
if [[ "$mode" == --smoke ]]; then arguments+=(--smoke --keys 12); fi
timeout --signal=TERM --kill-after=5s 600s uv run tools/check-writing-responsiveness.py "${arguments[@]}" 2>&1 | tee "$output/measurement.log"
printf 'Checkpoint: review %s/summary.json; owner comfort and physical-input latency remain separate.\n' "$output"
