#!/usr/bin/env bash
# One entry point for the owner's QML/prototype comparison. --smoke is automated.
set -euo pipefail

case ${1:-} in
  --help|-h)
    printf '%s\n' 'Usage: tools/inspect-writing-desk.sh [--smoke]' \
      'Default: build, open prototype B in your browser, then open the native QML sample.' \
      '--smoke: build and exercise the same native screen/export without manual checkpoints.'
    exit 0 ;;
  --smoke|'') ;;
  *) printf 'Unknown option: %s\n' "$1" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || { printf 'Expected at most one option.\n' >&2; exit 2; }
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
# Reuse the tested socket selection and native rendering environment.
source "$project_dir/tools/check-wayland.sh"
cd -- "$project_dir"
prepare_wayland
for command in mise uv tee mktemp timeout; do
  command -v "$command" >/dev/null || { printf 'Missing command: %s\n' "$command" >&2; exit 1; }
done
mkdir -p build
APOLLO_RUN_DIR=$(mktemp -d "$project_dir/build/desk-review-XXXXXX")
trap 'printf "Stopped. Logs: %s\n" "$APOLLO_RUN_DIR" >&2' ERR
printf 'Review files: %s\n' "$APOLLO_RUN_DIR"
run_logged configure mise exec -- cmake --preset release
run_logged build mise exec -- cmake --build --preset release
cp -- "$project_dir/prototype/The Last Broadcast.fountain" "$APOLLO_RUN_DIR/The Last Broadcast.fountain"
editor=$project_dir/build/release/apollo_editor
sample="$APOLLO_RUN_DIR/The Last Broadcast.fountain"
if [[ ${1:-} == --smoke ]]; then
  run_logged native-look timeout 30s env APOLLO_TEST_OPENGL=1 "$project_dir/build/release/editor_tests" writing_desk_outline_and_focus
  run_logged native-export timeout 30s env QT_FATAL_WARNINGS=1 "$editor" "$sample" --agreement "$APOLLO_RUN_DIR/export"
  printf 'Native screen, outline/focus checks, and sample export completed. These checks do not record new owner observations.\n'
  exit 0
fi
command -v xdg-open >/dev/null || { printf 'Missing command: xdg-open\n' >&2; exit 1; }
printf '\n%s\n' \
  'Enlarge each window in turn to at least 1100 px wide for the full page with the outline open.' \
  'Compare the slate header, narrow left rail, quiet gray desk, centered paper, and pale scene outline.' \
  'In each screen use the second rail icon to show/hide Scenes; QML also has Ctrl+\ and the × close button.' \
  'The outline is a read-only opening preview. Notes, stash, history, and scene navigation are outside this spike.' \
  'QML uses the real Courier Prime/profile layout. The HTML uses sample page 12 and approximate text layout.' \
  'Type/select briefly; Ctrl+Z undoes and Ctrl+Shift+Z redoes. Export opens options; Ctrl+E exports the current snapshot.' \
  'The source is never saved by this spike.' \
  'The sample and any PDF are in the review directory above.' \
  'The Writing desk foundation and Qt stack are accepted. Note any further refinements you want after inspection.'
printf '\nPress Enter to open prototype B and the QML screen. '
read -r _
prototype_url=$(uv run python -c 'import pathlib,sys; print(pathlib.Path(sys.argv[1]).as_uri() + "?variant=b")' "$project_dir/prototype/editor.html")
xdg-open "$prototype_url" > "$APOLLO_RUN_DIR/browser.log" 2>&1
run_logged editor "$editor" "$sample"
printf '\nInspection ended. No new owner observations were recorded. See docs/spike-report.md and docs/spike-handoff.md.\n'
