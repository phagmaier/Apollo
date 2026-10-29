#!/usr/bin/env bash
# The only terminal output is aggregate JSON. Logs/screenshots may be private.
set +x
set -euo pipefail
umask 077
ulimit -c 0

fail() {
  printf '{"success":false,"harness":"fountain-workflow","failure":"%s"}\n' "$1"
  exit 1
}
usage() {
  printf '%s\n' 'Usage: tools/measure-fountain-workflow.sh --source FILE --output NEW_DIRECTORY [--keys N] [--offscreen]' \
    'Default: isolated native Wayland/OpenGL real-QML import, edit, autosave/reopen and Fountain export measurement.' \
    'Keep the measurement window focused; do not type or move the mouse. Focus loss fails the run.' \
    '--offscreen: exercise the actual application runtime with software rendering, not native display/performance evidence.' \
    'The source is never modified. Output must not exist; its parent must exist. All generated files are private under output.' \
    'Empty-speech corrections apply only to the disposable measurement, not as author acceptance. No PDF is exported.'
}
source_path= output= keys=120 offscreen=0
while (($#)); do
  case "$1" in
    --source) (($# >= 2)) || fail invalid_arguments; [[ -z "$source_path" ]] || fail invalid_arguments; source_path=$2; shift 2 ;;
    --output) (($# >= 2)) || fail invalid_arguments; [[ -z "$output" ]] || fail invalid_arguments; output=$2; shift 2 ;;
    --keys) (($# >= 2)) || fail invalid_arguments; keys=$2; shift 2 ;;
    --offscreen) offscreen=1; shift ;;
    --help|-h) usage; exit 0 ;;
    *) fail invalid_arguments ;;
  esac
done
[[ -n "$source_path" && -n "$output" ]] || fail invalid_arguments
[[ "$keys" =~ ^[0-9]{1,4}$ ]] || fail invalid_key_count
((10#$keys >= 1 && 10#$keys <= 2000)) || fail invalid_key_count
for command in mise timeout realpath mkdir cat; do
  command -v "$command" >/dev/null || fail required_command_missing
done
source_path=$(realpath -e -- "$source_path" 2>/dev/null) || fail source_unavailable
[[ -f "$source_path" && -r "$source_path" ]] || fail source_unavailable
output=$(realpath -m -- "$output" 2>/dev/null) || fail invalid_output
[[ ! -e "$output" && ! -L "$output" ]] || fail output_already_exists
mkdir -m 700 -- "$output" 2>/dev/null || fail output_creation_failed
[[ "$source_path" != "$output/"* ]] || fail input_must_be_outside_disposable_output
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." 2>/dev/null && pwd -P) || fail runner_location_unavailable
cd -- "$project_dir" 2>/dev/null || fail runner_location_unavailable
trap 'fail interrupted' INT
trap 'fail terminated' TERM

# The executable isolates writable XDG application paths before Qt starts.
# Keep tool-manager paths while building; changing them reinstalls the tools.
if ((offscreen)); then
  export QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RENDER_LOOP=basic
  unset QSG_RHI_BACKEND QSG_RHI_PREFER_SOFTWARE_RENDERER QT_IM_MODULE
else
  # Reuse the owned-socket discovery and native backend policy; never control
  # the compositor or steal focus back after the initial activation request.
  source "$project_dir/tools/check-wayland.sh"
  prepare_wayland >"$output/preflight.log" 2>&1 || fail native_wayland_unavailable
fi
export QT_FORCE_STDERR_LOGGING=1
unset QT_FATAL_WARNINGS QSG_INFO QT_LOGGING_TO_CONSOLE
if ! timeout 300s mise exec -- cmake --preset release >"$output/build.log" 2>&1; then
  fail configure_failed_see_private_log
fi
if ! timeout 300s mise exec -- cmake --build --preset release --target measure_fountain_workflow >>"$output/build.log" 2>&1; then
  fail build_failed_see_private_log
fi
status=0
timeout --signal=TERM --kill-after=5s 260s "$project_dir/build/release/measure_fountain_workflow" \
  --source "$source_path" --output "$output" --keys "$keys" \
  >"$output/aggregate.stdout.json" 2>"$output/runtime.stderr.log" || status=$?
if ((status == 124 || status == 137)); then fail workflow_deadline_exceeded; fi
if [[ ! -s "$output/aggregate.stdout.json" ]]; then fail workflow_exited_without_aggregate; fi
cat -- "$output/aggregate.stdout.json"
exit "$status"
