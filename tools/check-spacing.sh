#!/usr/bin/env bash
# Disposable native spacing review; no private inputs or owner session state.
set -euo pipefail

case ${1:-} in
  --help|-h)
    printf '%s\n' 'Usage: tools/check-spacing.sh [--smoke]' \
      'Default: build release, create an isolated synthetic project, then open it for manual spacing review.' \
      '--smoke: build and run focused native spacing/rendering and export checks without owner input.' \
      'All project work, exports and logs remain in a new ignored build/spacing-check-* directory.'
    exit 0 ;;
  --smoke|'') ;;
  *) printf 'Unknown option: %s\n' "$1" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || { printf 'Expected at most one option; see --help.\n' >&2; exit 2; }
mode=${1:-}
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
source "$project_dir/tools/check-wayland.sh"
cd -- "$project_dir"
for command in mise tee mktemp timeout; do
  command -v "$command" >/dev/null || { error "Required command is missing: $command"; exit 1; }
done
prepare_wayland
mkdir -p build
APOLLO_RUN_DIR=$(mktemp -d "$project_dir/build/spacing-check-XXXXXX")
trap 'printf "\nStopped (exit %s). Work and logs preserved: %s\n" "$?" "$APOLLO_RUN_DIR" >&2' ERR
trap 'printf "\nInterrupted. Work and logs preserved: %s\n" "$APOLLO_RUN_DIR" >&2; exit 130' INT
trap 'printf "\nTerminated. Work and logs preserved: %s\n" "$APOLLO_RUN_DIR" >&2; exit 143' TERM
printf 'Work and logs: %s\n' "$APOLLO_RUN_DIR"
run_logged configure mise exec -- cmake --preset release
run_logged build mise exec -- cmake --build --preset release

# Qt stores resume/recents in XDG_DATA_HOME. Keep HOME and the runtime socket
# for the desktop/input method, but isolate every writable XDG application path.
export XDG_DATA_HOME="$APOLLO_RUN_DIR/xdg/data"
export XDG_CONFIG_HOME="$APOLLO_RUN_DIR/xdg/config"
export XDG_STATE_HOME="$APOLLO_RUN_DIR/xdg/state"
export XDG_CACHE_HOME="$APOLLO_RUN_DIR/xdg/cache"
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_STATE_HOME" "$XDG_CACHE_HOME" "$APOLLO_RUN_DIR/exports"
editor=$project_dir/build/release/apollo_editor
sample=$APOLLO_RUN_DIR/spacing.fountain
project=$APOLLO_RUN_DIR/project
output=$APOLLO_RUN_DIR/exports/spacing.fountain
# --create is the ordinary-project fixture CLI, not the product import path.
# The manual Enter BEFORE the existing space creates authored leading spacing.
printf 'INT. SYNTHETIC ROOM - DAY\n\nA lamp glows.\n\nMARA\nAt the same time.\n\nThe lamp goes dark.\n' > "$sample"
run_logged create "$editor" "$project" --create "$sample"
{
  printf 'Synthetic ordinary project: %s\nExport OUTSIDE project: %s\n' "$project" "$output"
  printf 'Native platform: %s; render loop: %s; backend: %s\n' "$QT_QPA_PLATFORM" "$QSG_RENDER_LOOP" "$QSG_RHI_BACKEND"
  printf '%s\n' \
    'Provisional spacing presentation: authored spaces stay saved; line-edge spaces are hidden and disclosed on clean Fountain export.' \
    '1. Click in the dialogue immediately AFTER "the", BEFORE its following space (At the| same time.). Press Enter once: the same speech continues on the next line, with its existing space hidden before "same", not a new element.' \
    '2. Before moving the caret, press Enter again: one blank dialogue line appears inside the same speech. Undo once removes only that blank line; redo restores it (Ctrl+Z / Ctrl+Shift+Z).' \
    '3. With the caret still at the continued line start after the second Enter (or redo), press Right once to cross the hidden leading space, then Left once to return. Text stays at the dialogue column while the caret can reach just left of it. Judge whether this feels comfortable.' \
    '4. Press Ctrl+Shift+E (Fountain opens selected). Inspect the spacing disclosure for the leading space created by step 1. Replace the Fountain destination field with the OUTSIDE-project path above, then click Export Fountain.' \
    '   Expect visible text without line-edge spaces, and the internal blank dialogue line represented by two spaces in Fountain. The saved writing must retain authored spacing.' \
    '5. Close Apollo normally to finish saving. Reopen with the isolated command printed below if you want to inspect persistence. Do not open owner projects in this review session.' \
    'This is a visual/comfort review, not an acceptance decision. Imported PDF remains blocked; this ordinary synthetic project does not change that gate.'
  printf '\nReopen safely:\n'
  printf 'env -u QT_QUICK_BACKEND -u QSG_RHI_PREFER_SOFTWARE_RENDERER XDG_DATA_HOME=%q XDG_CONFIG_HOME=%q XDG_STATE_HOME=%q XDG_CACHE_HOME=%q QT_QPA_PLATFORM=wayland QT_IM_MODULE=fcitx QSG_RENDER_LOOP=threaded QSG_RHI_BACKEND=opengl %q %q\n' \
    "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_STATE_HOME" "$XDG_CACHE_HOME" "$editor" "$project"
} | tee "$APOLLO_RUN_DIR/checkpoints.txt"

if [[ "$mode" == --smoke ]]; then
  printf '\nNative test windows open and close automatically; leave keyboard/mouse alone.\n'
  run_logged native-spacing timeout 90s env APOLLO_TEST_OPENGL=1 \
    "$project_dir/build/release/editor_tests" deliberate_break_splits_speech_at_page_end \
    enter_twice_makes_blank_dialogue_line enter_before_space_keeps_engine_breaks
  # This existing real-controls flow checks disclosure, persistence and export;
  # its scenegraph is explicitly software, unlike native-spacing and native-project.
  run_logged spacing-export-flow timeout 90s "$project_dir/build/release/project_access_flow_tests" \
    typed_spacing_is_kept_and_exported_as_presented
  run_logged native-project timeout 30s "$editor" "$project" --agreement "$APOLLO_RUN_DIR/exports/native-project"
  run_logged fountain-export timeout 30s "$editor" "$project" --export-fountain "$output"
  printf '\nSmoke checks completed; no owner comfort/visual acceptance recorded.\n'
else
  [[ -t 0 ]] || { error 'Manual review needs a terminal; use --smoke for automation.'; exit 1; }
  printf '\nPress Enter to open the synthetic project; keep these checkpoints visible. '
  read -r _
  run_logged editor "$editor" "$project"
  printf '\nReview closed. No owner acceptance has been recorded automatically.\n'
fi
printf 'All work preserved: %s\n' "$APOLLO_RUN_DIR"
