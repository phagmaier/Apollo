#!/usr/bin/env bash
# Render Fountain to PDF with afterwriting, an independent open-source formatter,
# configured with the Academy-measured line widths and page capacity
# (docs/export-profile.md). Use it as a cross-check of line wrapping and rough
# page counts. Its page-break rules differ from Apollo's (see fixtures/README.md).
#
#   tools/afterwriting-oracle.sh input.fountain output.pdf [--no-title-page]
#
# The tool is installed once into ~/.cache/apollo/afterwriting, outside the repo.
set -euo pipefail

src=${1:?usage: afterwriting-oracle.sh input.fountain output.pdf [--no-title-page]}
out=${2:?usage: afterwriting-oracle.sh input.fountain output.pdf [--no-title-page]}
title_page=true
[[ ${3:-} == --no-title-page ]] && title_page=false

dir=${XDG_CACHE_HOME:-$HOME/.cache}/apollo/afterwriting
if [[ ! -x $dir/node_modules/.bin/afterwriting ]]; then
  mkdir -p "$dir"
  printf '{"name":"apollo-afterwriting-oracle","private":true}\n' > "$dir/package.json"
  # No install scripts are needed; deny snyk's (an afterwriting dependency) explicitly.
  printf 'minimumReleaseAge: 1440\nallowBuilds:\n  snyk: false\n' > "$dir/pnpm-workspace.yaml"
  (cd "$dir" && pnpm add afterwriting@1.17.3 >/dev/null)
fi

profile=$(readlink -f "$dir/node_modules/afterwriting")/js/utils/print-profiles.js
if ! grep -q 'lines_per_page = 56' "$profile"; then
  # Academy sample: 56 body lines, dialogue 35 columns at 3.0 in, parenthetical 17 at 3.4 in, cue at 4.0 in.
  sed -i 's/letter.lines_per_page = 55;/letter.lines_per_page = 56; letter.dialogue.max = 35; letter.parenthetical.max = 17; letter.dialogue.feed = 3.0; letter.parenthetical.feed = 3.4; letter.character.feed = 4.0;/' "$profile"
  grep -q 'lines_per_page = 56' "$profile" || { echo "profile patch failed: $profile" >&2; exit 1; }
fi

"$dir/node_modules/.bin/afterwriting" --source "$src" --pdf "$out" --overwrite \
  --setting print_profile=usletter --setting double_space_between_scenes=true \
  --setting print_title_page=$title_page >/dev/null
echo "$out"
