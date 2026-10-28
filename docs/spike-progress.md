# Editor/export spike progress

The spike is in progress. **Acceptance check 1 (engine fixtures) passes at the
engine line level**, including the independent Academy comparison completed on
2026-10-27. The other six checks in
[spike-brief.md](spike-brief.md#passfail-checks) remain incomplete.

All eight hand-derived layout
fixtures match page, slot, element, text, and the style annotations in their
unchanged expected files. All 11 Academy body pages match after page anchoring
and only the four documented deviations. These results do not establish
PDF/editor agreement, geometric correctness, complete profile conformance, or
performance.

- CMake/Ninja release and sanitizer presets, mise tasks, and the four pinned
  Courier Prime fonts with build-time hash checks are in place.
- The Qt-independent layout library and importer cover the fixture constructs,
  including emphasis spans, lyrics, centered text, transitions, title fields,
  speech and action splits, continuation cues, and dual dialogue. Tests compare
  all eight fixtures end to end through `import_fountain` and `layout`. The
  importer is a spike subset, not a full Fountain compatibility implementation.
- Regenerated `--scenes 175 --seed 1` and `--scenes 444 --seed 1` inputs import and
  lay out without an error: 202 and 506 body pages respectively. These counts are
  observations, not independent proofs of page correctness or performance. Use
  `mise exec -- cmake --build --preset release`, then
  `build/release/layout_probe INPUT.fountain > OUTPUT.tsv` to repeat the dump.
- The Academy comparison is now repeatable with `tools/compare_academy.py` (see
  below). It checks the local TSV against the PDF's measured rows and the
  Fountain transcription against those rows before comparing engine output.
- Release and sanitizer tests pass, including the new regressions. The Academy
  comparison also passes through both probes. Sanitizer execution ran outside
  the tracing sandbox so LeakSanitizer could run; no sanitizer diagnostics were
  reported. No Qt editor or Apollo PDF has been produced or measured.

## Independent Academy verification

The comparison follows the fixture guide: restart from each reference page's
first line, retain all later source text, and compare the resulting first page.
Keeping the suffix rather than truncating at the page end exercises the real
split/push decision. The continuation-page suffix includes its cue. The tool
also checks the full unanchored document's 245 body elements for exact
paragraph/element text preservation, excluding generated page continuations and
rewrapping. It never rewrites the reference files.

The earlier deviation descriptions used PDF page numbers, including the title;
the TSV numbers body pages. The fixture guide now labels both explicitly. All
numbers below are **body pages**.

| Reference body page | Result |
| --- | --- |
| 1–3 | Exact page/slot/element/text match: 39, 39, and 44 lines |
| 4 | 42 lines match with the continuation cue at slot 1 instead of margin slot 0; following slots +1 |
| 5 | All 41 reference lines match; the next three-line speech fits in slots 54–56, as documented |
| 6 | 37 lines match; automatic spacing after the centered date shifts subsequent slots −2 |
| 7 | All 36 reference lines match; the next Shot-as-action line fits at slot 56, as documented |
| 8–11 | Exact match: 36, 41, 44, and 37 lines |

One genuine discrepancy remained after anchoring: the final centered line was
at slot 46 rather than 47. The local PDF confirms two blank slots after the
transition at slot 44. The profile's blanket one-blank rule was too broad;
the profile and engine now use two before centered text immediately following
a transition. This is a measured rule correction, not a fifth allowed deviation.
The regression failed before the fix and passes afterward. Boundary coverage
also checks that the extra spacing counts toward fit and drops at a page top.

The boundary review reproduced a separate B2 defect: a heading could remain at
slot 54 while a following three-line centered or lyric element moved to page 2.
The heading now moves with either unsplittable element. Both regressions failed
before the fix; companion cases confirm that a one-line element still fits
exactly at slot 56. Existing action, speech, dual-dialogue, parenthetical, and
transition tests continue to pass. No small-fixture expectations changed.

In the unanchored full run, keeping the three-line speech on body page 5 starts
the next reference page's remaining content four slots earlier. Omitting the
two extra blanks after the date makes later content six slots earlier. This
allows the next speech to start at slot 52 on Apollo page 6: two dialogue lines
at 53–54, `(MORE)` at 55, and eight rewrapped dialogue lines after the cue on
page 7. That is the latest qualifying B3 sentence split, not an unexplained
defect or an additional reference exception. The full run has 11 body pages
and preserves all source text; its later page boundaries are not expected to
match the reference without anchoring.

Reproduction (Poppler and the three local `reference/academy/` files required):

```sh
mise run check
uv run tools/compare_academy.py --probe build/release/layout_probe
mise run check-debug
uv run tools/compare_academy.py --probe build/debug/layout_probe
```

Both check tasks passed, as did both Academy comparisons. Before the spacing
fix, the comparator failed specifically on body page 11. The independent local
PDF used here has SHA-256
`650d0a9a3b24048f761cf05f1c7f5873a3f74a8bb926d23de700c8fbd51993ba`.
The PDF, transcription, expected TSV, and diagnostic dumps remain local;
no Academy screenplay text was added to the tests.

## Remaining work and next step

The model still carries page/slot/type/text/styles rather than geometric
positions. Title-page placement beyond the fixture, unsupported or malformed
Fountain, emphasis beyond the exercised cases, multi-element lyric grouping,
broader heading/transition boundary combinations, and large-document performance
still need verification. Passing check 1 is not complete layout-engine or
importer conformance.

The next concrete PDF step is to add shared point geometry to the engine's
output: line x positions and baselines, parenthetical hanging offsets, dual
columns, right/center alignment using the pinned font's natural advance, and
title-page/page-number placement. Keep this model Qt-independent and preserve
the existing line expectations. Then build the minimal `QPdfWriter` painter
using the bundled fonts, check Letter size and font embedding/Unicode mapping,
and compare independent PDF line dumps and x positions against the fixtures
and profile. No editor/PDF agreement or performance result exists yet.

The full spike still requires PDF quality, editor/PDF agreement, interaction and
performance checks, the QML visual screen, the owner's manual session, code
shape review of the completed integration, and a final keep/switch recommendation.
