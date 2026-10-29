#!/usr/bin/env bash
# Automated native-session runner, using the wizard template's stage/log helpers.
# --preflight omits the full typing measurements.
set -euo pipefail

TOTAL_STAGES=6
_STAGE_INDEX=0

stage() {
  _STAGE_INDEX=$((_STAGE_INDEX + 1))
  printf '\nStage %s/%s: %s\n\n' "$_STAGE_INDEX" "$TOTAL_STAGES" "$1"
}

say() { printf '%s\n' "$1"; }
error() { printf 'Apollo Wayland checks: %s\n' "$1" >&2; }

# Prefer the inherited display when valid; otherwise select the sole socket
# owned by this user. Never guess between multiple desktop sessions.
prepare_wayland() {
  local runtime=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
  local display=${WAYLAND_DISPLAY:-} socket candidate
  local sockets=()
  [[ -d "$runtime" && -O "$runtime" ]] || {
    error "No runtime directory owned by you at $runtime. Run from your desktop session."
    return 1
  }
  socket=$display
  [[ "$display" == /* ]] || socket=$runtime/$display
  if [[ -z "$display" || ! -S "$socket" || ! -O "$socket" ]]; then
    for candidate in "$runtime"/wayland-*; do
      if [[ -S "$candidate" && -O "$candidate" ]]; then sockets+=("$candidate"); fi
    done
    case ${#sockets[@]} in
      0) error "No Wayland socket found in $runtime. Run on the machine with Hyprland open."; return 1 ;;
      1) display=${sockets[0]##*/} ;;
      *) error "Multiple Wayland sockets found in $runtime. Open a fresh Hyprland terminal and rerun this script."; return 1 ;;
    esac
  fi
  export XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY="$display"
  export QT_QPA_PLATFORM=wayland QT_IM_MODULE=fcitx
  export QSG_RENDER_LOOP=threaded QSG_RHI_BACKEND=opengl QT_FORCE_STDERR_LOGGING=1
  # A software backend inherited from headless testing would hide the native
  # render-thread path and invalidate the timing run.
  unset QT_QUICK_BACKEND QSG_RHI_PREFER_SOFTWARE_RENDERER
}

run_logged() {
  local name=$1 status
  shift
  if "$@" 2>&1 | tee "$APOLLO_RUN_DIR/$name.log"; then
    return 0
  else
    status=$?
    printf '\n%s failed (exit %s). See %s/%s.log\n' "$name" "$status" "$APOLLO_RUN_DIR" "$name" >&2
    return "$status"
  fi
}

run_wizard() {
  local mode=${1:-} scenes editor workload timing_text
  case "$mode" in
    --help|-h)
      say 'Usage: tools/check-wayland.sh [--preflight]'
      say 'Default: build/check, native interaction/input tests, first-paint checks, typing timings, then export while typing.'
      say '--preflight: build/check and native interaction/rendering checks, without full typing measurements.'
      return 0 ;;
    --preflight) TOTAL_STAGES=3 ;;
    '') ;;
    *) error "Unknown option: $mode"; return 2 ;;
  esac
  [[ $# -le 1 ]] || { error 'Expected at most one option; see --help.'; return 2; }
  for command in mise uv pgrep tee timeout mktemp date wtype hyprctl; do
    command -v "$command" >/dev/null || { error "Required command is missing: $command"; return 1; }
  done

  local project_dir
  project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
  cd -- "$project_dir"
  prepare_wayland
  pgrep -u "$(id -u)" -x fcitx5 >/dev/null || {
    error 'fcitx5 is not running for your user. Start it before testing compose input.'
    return 1
  }
  mkdir -p build
  APOLLO_RUN_DIR=$(mktemp -d "$project_dir/build/wayland-check-$(date +%Y%m%d-%H%M%S)-XXXXXX")
  APOLLO_RESULTS=$APOLLO_RUN_DIR/results.txt
  trap 'printf "\nStopped. Session logs: %s\n" "$APOLLO_RUN_DIR" >&2' ERR
  trap 'printf "\nInterrupted. Session logs: %s\n" "$APOLLO_RUN_DIR" >&2; exit 130' INT
  {
    printf 'Apollo native Wayland session\nStarted: %s\n' "$(date -Iseconds)"
    printf 'Display: %s/%s\nInput module: %s\nRender loop: %s, backend: %s\n' \
      "$XDG_RUNTIME_DIR" "$WAYLAND_DISPLAY" "$QT_IM_MODULE" "$QSG_RENDER_LOOP" "$QSG_RHI_BACKEND"
  } | tee "$APOLLO_RESULTS"
  say "Logs and results: $APOLLO_RUN_DIR"
  say 'Test windows will open and close automatically. Leave the keyboard/mouse alone while they run.'
  say 'The interaction tests also use the clipboard.'

  stage 'Build and automated checks'
  run_logged build mise run check
  editor=$project_dir/build/release/apollo_editor

  stage 'Native interaction and input-method checks'
  run_logged native-interaction timeout 90s env APOLLO_TEST_OPENGL=1 APOLLO_TEST_WAYLAND_INPUT=1 \
    "$project_dir/build/release/editor_tests"
  run_logged native-export-lifecycle timeout 90s "$project_dir/build/release/export_job_tests"
  run_logged native-project-access timeout 90s "$project_dir/build/release/project_access_flow_tests"
  run_logged native-emphasis timeout 180s env PYTHONDONTWRITEBYTECODE=1 uv run tests/emphasis_native_smoke.py \
    --editor "$editor" --output "$APOLLO_RUN_DIR/native-emphasis"
  printf '\nNative emphasis: real wtype Ctrl+B/I/U, exact saved spans, undo/redo, caret-only plain typing, autosave and fresh-process reopen passed.\n' >> "$APOLLO_RESULTS"
  run_logged native-title-page timeout 180s env PYTHONDONTWRITEBYTECODE=1 uv run tests/title_page_native_smoke.py \
    --editor "$editor" --output "$APOLLO_RUN_DIR/native-title-page"
  printf '\nNative title page: real wtype six-field editing, Tab focus/scroll, Apply/Cancel, one-step undo/redo, autosave, fresh-process reopen, PDF content and exact Fountain passed.\n' >> "$APOLLO_RESULTS"
  run_logged native-export-snapshots timeout 180s env PYTHONDONTWRITEBYTECODE=1 uv run tests/editor_pdf_tests.py \
    --editor "$editor" --output "$APOLLO_RUN_DIR/native-editor-pdf-tests"
  while IFS= read -r line; do
    if [[ "$line" == Totals:* ]]; then printf '\nNative interaction: %s\n' "$line" >> "$APOLLO_RESULTS"; fi
  done < "$APOLLO_RUN_DIR/native-interaction.log"

  stage 'Native rendering preflight'
  say 'Two test windows will open briefly and close automatically.'
  for scenes in 175 444; do
    workload=$project_dir/build/release/editor-pdf-tests/workload-$scenes.fountain
    run_logged "startup-$scenes" timeout 30s env QT_FATAL_WARNINGS=1 QSG_INFO=1 \
      "$editor" "$workload" --agreement "$APOLLO_RUN_DIR/startup-$scenes"
  done
  say 'Native first-paint and export checks completed.'
  if [[ "$mode" == --preflight ]]; then
    printf '\nAutomated preflight completed. Full typing measurements were not run.\n' | tee -a "$APOLLO_RESULTS"
    return 0
  fi

  stage 'Native typing timings'
  say 'Each window will type automatically for about 20 seconds and close.'
  say 'Keep it visible and focused. Do not type during these runs.'
  for scenes in 175 444; do
    workload=$project_dir/build/release/editor-pdf-tests/workload-$scenes.fountain
    run_logged "timing-$scenes" timeout 60s "$editor" "$workload" --measure 200
    timing_text=$(<"$APOLLO_RUN_DIR/timing-$scenes.log")
    [[ "$timing_text" == *'platform wayland, renderer OpenGL'* ]] || {
      error "Timing run $scenes did not report native Wayland/OpenGL; inspect its log."
      return 1
    }
    printf '\nTiming workload: %s scenes\n%s\n' "$scenes" "$timing_text" >> "$APOLLO_RESULTS"
  done

  stage 'PDF export while typing'
  say 'Each window repeatedly exports real snapshots while typing 200 keys at a page boundary.'
  say 'Keep it visible and focused. Snapshot capture/dispatch is included in the key timing.'
  for scenes in 175 444; do
    workload=$project_dir/build/release/editor-pdf-tests/workload-$scenes.fountain
    run_logged "export-timing-$scenes" timeout 90s "$editor" "$workload" --measure-export "$APOLLO_RUN_DIR/export-$scenes" 200
    timing_text=$(<"$APOLLO_RUN_DIR/export-timing-$scenes.log")
    [[ "$timing_text" == *'platform wayland, renderer OpenGL'* && "$timing_text" == *'in-flight overlap:'* ]] || {
      error "Export timing run $scenes did not report native rendering and worker overlap; inspect its log."
      return 1
    }
    printf '\nExport while typing: %s scenes\n%s\n' "$scenes" "$timing_text" >> "$APOLLO_RESULTS"
  done

  stage 'Saved results'
  say "Send this file back: $APOLLO_RESULTS"
  say 'Recorded results still need review against the spike criteria.'
  say "Automated checks do not replace the owner's comfort or visual acceptance."
  say 'See docs/spike-report.md for the recommendation and tools/inspect-writing-desk.sh for visual review.'
  say 'The owner accepted the Writing desk foundation and chose to keep Qt; the report records the remaining limits.'
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  run_wizard "$@"
fi
