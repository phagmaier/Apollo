#!/usr/bin/env bash
# One entry point for local runtime verification and the owner's session.
set -euo pipefail
umask 077
export PYTHONDONTWRITEBYTECODE=1
case ${1:-} in
  --help|-h)
    printf '%s\n' 'Usage: tools/check-owner-handoff.sh [--smoke]' \
      'Run required checks, package locally, verify native relocation and writing/recovery/export controls.' \
      'Default: then open a disposable project for a manual writing session and record your observations.' \
      '--smoke: automated checks only; no owner comfort acceptance.' \
      'Requires a Wayland desktop. QT_IM_MODULE defaults to compose; set fcitx explicitly if available.'
    exit 0 ;;
  ''|--smoke) ;;
  *) printf 'Unknown option: %s\n' "$1" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || exit 2
mode=${1:-}
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
cd -- "$project_dir"
for command in mise uv timeout tee mktemp pkg-config pacman pdftotext pdffonts; do
  command -v "$command" >/dev/null || { printf 'Missing command: %s\n' "$command" >&2; exit 1; }
done
input_module=${QT_IM_MODULE:-compose}
source tools/check-wayland.sh
prepare_wayland
export QT_IM_MODULE=$input_module
mkdir -p build
APOLLO_RUN_DIR=$(mktemp -d "$project_dir/build/owner-handoff-XXXXXX")
trap 'printf "Stopped; saved logs: %s\n" "$APOLLO_RUN_DIR" >&2' ERR
trap 'printf "Interrupted; saved logs: %s\n" "$APOLLO_RUN_DIR" >&2; exit 130' INT
printf 'Logs and disposable runtime: %s\n' "$APOLLO_RUN_DIR"
printf 'Native Wayland/OpenGL, input module: %s. Test windows will open and close.\n' "$QT_IM_MODULE"
printf 'Leave keyboard/mouse alone until the manual checkpoint.\n'
{
  date -Iseconds
  printf 'Wayland: %s; input: %s\n' "$WAYLAND_DISPLAY" "$QT_IM_MODULE"
  printf 'Owner comfort: pending; automated input is Qt Test, not physical/compositor input.\n'
} > "$APOLLO_RUN_DIR/results.txt"
run_logged release env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic mise run check
run_logged debug env QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic mise run check-debug
run_logged engine mise run check-engine
run_logged package tools/package-local.sh "$APOLLO_RUN_DIR/runtime"
runtime=$APOLLO_RUN_DIR/runtime/lib/apollo
run_logged native-runtime timeout --kill-after=5s 90s uv run tests/local_runtime_tests.py \
  --build build/release --cmake "$(mise which cmake)" --spelling-tests build/release/spelling_tests \
  --package "$APOLLO_RUN_DIR/runtime" \
  --platform wayland --output "$APOLLO_RUN_DIR/native-runtime"
# Keep the behavior checks' personal dictionary and application state local too.
export XDG_DATA_HOME="$APOLLO_RUN_DIR/data" XDG_CONFIG_HOME="$APOLLO_RUN_DIR/config" XDG_CACHE_HOME="$APOLLO_RUN_DIR/cache"
# Run existing behavior checks against the packaged QML/dictionary and helpers.
# The verification binaries are removed afterwards and are not deliverables.
for executable in project_access_flow_tests spelling_tests; do
  cp -- "build/release/$executable" "$runtime/$executable"
done
run_logged native-writing timeout --kill-after=5s 120s "$runtime/project_access_flow_tests" \
  title_form_apply_cancel_undo_reopen_and_export \
  find_replace_autosaves_reopens_and_exports \
  suggestions_acceptance_autosaves_reopens_and_stays_project_local \
  scene_drag_autosaves_reopens_and_exports \
  damaged_save_requires_explicit_timestamped_recovery_and_preserves_evidence \
  both_save_destinations_failing_keep_memory_and_copy_can_enable_safe_departure \
  home_flushes_latest_writing_and_switches_without_cross_project_edits
run_logged native-spelling timeout --kill-after=5s 30s "$runtime/spelling_tests" checking_replacement_and_personal_words
rm -- "$runtime/project_access_flow_tests" "$runtime/spelling_tests"
printf 'Automated release, sanitizer, engine, native runtime and writing/recovery/export checks passed.\n' | tee -a "$APOLLO_RUN_DIR/results.txt"

# Prepare and verify the exact manual commands even in automated mode. Repeated
# synthetic scenes provide several page boundaries without using private work.
launcher=$APOLLO_RUN_DIR/runtime/bin/apollo
writer=$APOLLO_RUN_DIR/Owner-Writer
sample=$APOLLO_RUN_DIR/owner-sample.fountain
{
  printf 'Title: THE LAST BROADCAST\n\n'
  for ((part=0; part<6; ++part)); do
    cat -- "$project_dir/prototype/The Last Broadcast.fountain"
    printf '\n'
  done
} > "$sample"
run_logged create "$launcher" "$writer" --create "$sample"
run_logged sample-fountain "$launcher" "$writer" --export-fountain "$APOLLO_RUN_DIR/sample.fountain"
run_logged sample-pdf "$launcher" "$writer" --agreement "$APOLLO_RUN_DIR/sample-pdf"
run_logged sample-pdf-text pdftotext -layout "$APOLLO_RUN_DIR/sample-pdf/editor.pdf" -
rg -q -F 'THE LAST BROADCAST' "$APOLLO_RUN_DIR/sample-pdf-text.log"
rg -q -F 'INT. RADIO STATION' "$APOLLO_RUN_DIR/sample-pdf-text.log"
printf 'Disposable multipage owner project prepared; PDF/Fountain commands verified.\n' | tee -a "$APOLLO_RUN_DIR/results.txt"
if [[ $mode == --smoke ]]; then
  printf 'Owner session not run; comfort remains pending. Logs: %s\n' "$APOLLO_RUN_DIR"
  exit 0
fi

# Ordinary future launch uses the owner's normal state. This manual session
# retains the isolated paths above and HOME/runtime for the input method.
printf '\n%s\n' \
  'MANUAL CHECKPOINT — only the disposable Owner-Writer project will be changed.' \
  'Write for a realistic session, including across a page boundary. Check caret, selection and comfort.' \
  'Try suggestions (Tab accepts, Enter writes), F7 spelling, Ctrl+F/H find/replace, and F1 shortcuts.' \
  'Open Scenes (Ctrl+\), navigate and drag a scene; Undo/Redo. Try title, type and emphasis controls.' \
  'Export PDF and Fountain from Export to the run directory; keep writing while each runs.' \
  'Imported writing and unsupported snapshots retain their existing export gates.' \
  'Close normally; the runner then reopens the same project and cursor through isolated resume.'
printf 'Press Enter to begin. '
read -r _
run_logged owner-session "$launcher" "$writer"
printf '\nCheck the reopened writing and cursor, then close normally. Press Enter to reopen. '
read -r _
run_logged owner-reopen "$launcher"
printf '\nRecord comfort/appearance/input observations and any failures (one line): '
IFS= read -r observations
printf 'Owner observations: %s\n' "$observations" >> "$APOLLO_RUN_DIR/results.txt"
printf '\nSaved observations and logs: %s/results.txt\nNo milestone acceptance is inferred automatically.\n' "$APOLLO_RUN_DIR"
