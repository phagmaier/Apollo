# Apollo editor/export feasibility report

2026-10-28. Agent implementation and technical review are complete. **The owner
accepted the QML Writing desk foundation and chose to keep Qt 6, C++20, and
QML** after comparing the two screens. This is a bounded experiment, not the
writing release. The [brief](spike-brief.md) supplies the criteria; the
[progress log](spike-progress.md) preserves the earlier investigations and
measurements. Use the [manual handoff](spike-handoff.md) to inspect the result.

## Recommendation

**Keep Qt/C++/QML; accepted by the owner on 2026-10-28.**
The exercised engine, PDF, editing, native input, and primary-workload
measurements support continuing. Stock QML `TextEdit` retained caret movement,
selection, input methods, text storage, and undo; Apollo did not have to fork
Qt or write a general text control. The approved Writing desk's key qualities
can be expressed in QML. No switch trigger in the brief has been demonstrated.

The owner found the screens very close, approved the look, and agreed to keep
this direction. That accepts the visual foundation, not every future visual
detail or extended writing comfort. The owner had not tested undo with Ctrl+Z
and reported trying Ctrl+U; the shortcut guidance is now explicit in the
handoff. Automated undo/redo evidence remains unchanged, and no successful
owner undo test is claimed. The stress case has less latency headroom, and the
editor/importer still have experiment-only rules and known gaps below.
Electron + ProseMirror remains the fallback if a later text-stack limitation
meets the switch criteria.
No comparative Electron implementation or benchmark was run, so there is no
claim that switching would improve speed or reduce total effort by a measured
amount. Engine correctness work would carry over either way.

## Results against each check

| # | Result and evidence | Qualification |
| --- | --- | --- |
| 1. Engine fixtures | Pass for all eight unchanged hand-derived `.expected` files. The independent Academy comparator passes all 11 anchored body pages and preserves all 245 body elements, using only the four documented deviations. Release and sanitizer comparisons pass. | Anchoring retains each page's full source suffix, exercising the actual split decision. The unanchored Academy document has cumulative pagination shifts explained in the progress log. This is not a full Fountain conformance claim. |
| 2. Editor/PDF agreement | Pass on all eight fixtures, both regenerated workloads, and the local Academy transcription, including 50 scripted boundary edits per workload and Academy input. Line dumps read actual `QTextLine`s/decorations; Poppler reads actual PDFs. Native threaded rendering is also exercised. | Shared-engine agreement alone is not correctness. Separate hand-derived fixture, numeric geometry, and anchored Academy checks supply independent expectations. |
| 3. PDF quality | Pass on the exercised fixtures: 612×792 pt Letter, embedded Courier Prime faces with Unicode mapping, extractable text, profile columns within 0.01 in, all four emphasis faces and underline raster checks. | Snapshot-export PDFs still match the original fixture after live text replacement, repagination, caret movement, undo, and redo. Title inclusion/exclusion is checked. Unsupported glyphs fail export explicitly. |
| 4. Interaction | The brief's Enter/Tab, paste kinds, typing/type-change/scene-move undo/redo, page-gap caret/selection/copy, and dual-dialogue document-order checks pass. Native fcitx5 compose and dead-key commits and scene-move/undo/redo use actual Wayland virtual-keyboard input. Earlier owner observations reported typing, selection/copy, and Enter/Tab good. | Physical Compose-key use remains unverified (none is mapped). The owner's earlier session predates this shell. Scripts and captures do not establish extended writing comfort or acceptance of the final screen. |
| 5. Responsiveness | Primary-workload measurements meet the observed numeric targets; final-screen values and methods are below. Both workloads complete typing and real export while typing. | Warm initialized-process opening, synthetic key dispatch, and sampled PSS have the limits below. The 500-page case is a stress observation, not a 33 ms typing-target pass. |
| 6. Code shape | Pass: no private Qt includes or copied Qt sources; engine-only build/tests pass and `ldd` lists no Qt library. | Apollo owns pagination and screenplay commands; Qt retains the general editor mechanisms. The unused `ScreenplayLayout::draw` path is not validated by `TextEdit` tests. |
| 7. Look | Agent comparison passes; the owner also found the QML and prototype very close and accepted the look on 2026-10-28. QML carries over B's slate header, compact rail, quiet desk, centered paper, and expandable/hideable pale outline. | Acceptance establishes the visual foundation; later refinements remain possible. Accurate Courier Prime/profile layout replaces HTML's approximate layout; the sample has two actual scenes. The read-only outline and dimmed placeholders preserve the spike boundary. |

Agreement counts (title pages included):

| Input | Before / after 50 edits: pages | Before / after: text lines | Later elements changing page during edits |
| --- | --- | --- | --- |
| 175 scenes, seed 1 | 203 / 204 | 9,226 / 9,273 | 4,737 |
| 444 scenes, seed 1 | 507 / 509 | 23,034 / 23,090 | 19,329 |
| Local Academy transcription | 12 / 13 | 442 / 469 | 413 |

The generated primary/stress files have **202/506 body pages**, or 203/507
with the title. These are the brief's approximately 200/500-page workloads;
their counts are observations, not independent pagination oracles. The eight
fixtures have 251 unedited text lines in total. Copyrighted reference material,
generated inputs, PDFs, and captures remain local and ignored.

## Responsiveness measurements

Final clean runner: `build/wayland-check-20261028-011329-aJ05ct/`, exit 0.
Every row uses 200 keys on the final Writing desk screen. Times are ms;
opening/PSS are recorded independently for each run.

| Input / mode | Key→frame p50 / p95 / max | Stalls >100 ms / missing frames | Warm open→first frame | Settled editor PSS |
| --- | --- | --- | --- | --- |
| 203 pages, typing | 13.7 / 15.3 / 21.9 | 0 / 0 | 267.8 | 89.2 MiB |
| 507 pages, typing | 34.1 / 36.4 / 39.2 | 0 / 0 | 490.9 | 138.4 MiB |
| 203 pages, export while typing | 16.5 / 19.9 / 24.2 | 0 / 0 | 271.0 | 89.4 MiB |
| 507 pages, export while typing | 39.7 / 43.4 / 48.5 | 0 / 0 | 489.4 | 144.6 MiB |

| Input | Export completion p50 / p95 / max | Capture/dispatch p50 / p95 / max | Real exports | Sampled max editor / combined PSS |
| --- | --- | --- | --- | --- |
| 203 pages | 271.7 / 419.6 / 718.3 | 3.5 / 4.1 / 4.5 | 47 | 89.5 / 109.4 MiB |
| 507 pages | 567.6 / 691.0 / 721.7 | 8.9 / 9.3 / 9.6 | 33 | 144.6 / 165.7 MiB |

Saved worker intervals independently recount **153 keys / 199 frames**
overlapping actual helper execution on the primary input and **167 / 199**
on the stress input. Overlapping-key p95 is 19.3/41.5 ms. The memory timer
obtained 675/869 helper samples. Each export PDF has the expected 203/507
Letter pages and embedded Courier Prime. Earlier pre-shell figures remain in
the progress log; this final-screen run is the basis of this recommendation.
Differences between separate desktop runs have not been isolated by profiling.

Interpret the 500-page result using the brief's actual requirement: the
numeric p95 ≤33 ms, no >100 ms stall, ≤1 s warm opening, ≤5 s export, and
≤250 MiB PSS targets are specified for the **200-page primary case**. At 500
pages the brief asks that the editor stay usable and that numbers be recorded;
it does not require p95 ≤33 ms there. Successful 200-key runs with correct
final text/caret, no missing frames, no >100 ms stall, and real overlapping
exports support bounded scripted usability of the stress case. Its slower
typing during export is a real limitation and reduces headroom. Extended
subjective usability remains for the owner; it is neither an automatic stack
failure nor a claim that all numeric primary targets also pass at 500 pages.

### Method and limits

Release build, native Wayland, fcitx5, threaded OpenGL, Intel i5-9400/UHD 630,
Mesa 26.2.5, Hyprland/Omarchy, 1920×1080 at 60 Hz and scale 1, about 12 GiB
installed RAM. CMake 4.4.4/Ninja 1.13.2, Qt 6.12.0, GCC 16.2.1, C++20, and
the four pinned Courier Prime hashes are unchanged. The normal check suite
uses deterministic offscreen rendering; it is not the native timing evidence.

`apollo_editor FILE --measure 200` and `--measure-export OUTDIR 200` load the
actual Writing desk screen, then put the caret at the first character on the
later page of a block spanning a page boundary after the document midpoint.
They send 200 synthetic keys, with 60 ms idle after each completed frame. Each
key must advance the caret; the final text must contain exactly the inserted
characters. Window focus is checked before every native key.

Key latency runs from GUI dispatch through `QQuickWindow::frameSwapped`.
The export variant starts its key timer before any snapshot capture and
dispatch on that iteration. It repeatedly uses the same real `PdfExportJob`
as the visible control, without artificial worker delay. Export duration runs
from before capture through helper completion and final atomic replacement.
Helper `steady_clock` intervals and saved individual key/frame timestamps
establish actual overlap. This measures Apollo/Qt rendering, not physical
keyboard, compositor presentation, or end-to-end input latency.

Opening starts inside the initialized Qt application and ends at its first
frame. It includes document/font loading, layout, and the QML screen, but
excludes process/application initialization. Caches are warm. PSS after typing
is settled memory; export maxima sum editor/helper `smaps_rollup` samples
from a 20 ms GUI timer. That timer can slip and miss short peaks. These are
sampled maxima, not guaranteed peak memory. Normal desktop applications remain
open; no controlled laboratory or extended-session repeatability is claimed.
All builds and other agent tests finish before the final typing measurements.

## Integration approach

The pure C++20 engine owns wrapping, element spacing, page-break decisions,
continuation text, point geometry, and UTF-8 source mapping. Minimal Fountain
import serves the fixtures and generated inputs. One live `QTextBlock` holds
each element's text/type/dual flag, with character formats for emphasis.

`ScreenplayLayout`, a public `QAbstractTextDocumentLayout` subclass, positions
`QTextLine`s at engine breaks. It prepares the document's lazily created root
frame on the GUI thread before threaded scene-graph rendering; this fixes the
original native startup crash. It rebuilds changed block lines and reuses the
others, but still reads and repaginates the whole document per edit. Setting
the public viewport-observation flag avoids rendering the entire large
document each time.

Stock QML `TextEdit` receives that document through public
`QQuickTextDocument::setTextDocument`. `EditorCommands` supplies the spike's
Enter/Tab, paste, grouped typing, and scene-move semantics using `QTextCursor`.
Qt supplies the remaining movement, selection, deletion, undo, and input-method
behavior. Generated labels/page numbers are QML decorations, never inserted
document text. The Writing desk shell surrounds this same editor; its fixed
presentation scale maps the engine's 96 dpi pixels to the prototype's 612 px
paper without changing engine/PDF geometry. Caret reveal and visual tests map
through that transform and the actual viewport.

Export captures owned elements, emphasis, dual flags, and title inclusion on
the document thread. A versioned binary message with checked enum/span bounds
goes to an offscreen `pdf_export` helper through `QProcess` stdin. This is a
temporary worker protocol, not the future source schema. The helper owns its Qt fonts and
paints engine pages with `QPdfWriter`; no live document/font/layout crosses
the process boundary. An accepted job is immutable while writing continues.
Only one job runs at a time. Only clean helper exit publishes its staged PDF
by atomic rename; failures preserve existing bytes. Closing drains an accepted
job before Qt teardown. This process choice isolates font/paint state and
keeps typing responsive; it is not a claim that all Qt painting needs the
GUI thread.

The new outline is only an opening snapshot of headings with a preview
highlight. Showing/hiding it preserves selection, caret, focus, and text.
There is no scene navigator, reordering UI, persistence, history, notes/stash,
settings, packaging, or other product feature implementation in this work.

## Outstanding risks and deferred work

- Whole-document extraction/layout/cache checking remains linear per key.
  The stress numbers make incremental model/pagination work a likely next
  optimization. The primary target is met in these runs; no Qt text-stack
  limitation triggering a switch was demonstrated.
- The brief's limited editing rules are provisional: empty-element export,
  literal CONT'D handling, plain-text paste paragraphs, out-of-cycle element
  types, and cue/dialogue merges need later product decisions. Full Fountain
  compatibility, source schema, and round trips are not established.
- Typing into a manual-break element can show text that PDF export omits;
  only ASCII letters are uppercased by the engine; preedit is positioned but
  not repaginated until commit; title fields have no dedicated editing UI.
  These remain explicit spike gaps, not production-ready behavior.
- Unsupported font glyphs reject export; the current engine-rejection path
  uses an unpaginated editor fallback with a visible error. General malformed
  imports and every possible in-progress editing state are not proven.
- No export cancellation/timeout policy exists. Shutdown waits for the helper;
  a stalled filesystem can prolong closing. A production export workflow and
  persisted-document durability are separate work.
- Native tests use virtual keyboard input through fcitx5, with earlier owner
  observations for manual typing. Extended comfort, physical Compose input,
  cold launch, peak memory, and other machines/Qt upgrades remain unverified.
- The QML visual foundation is accepted; specific future visual refinements
  remain open. The outline is static by design and the HTML's approximate
  typography/page content is not a numeric layout standard. Changing an
  individual visual detail would first call for
  a focused QML refinement; it does not by itself demonstrate QML cannot
  achieve the direction.

## Verification, review, and handoff

The intended native fixes, snapshot/export lifecycle, protocol validation,
error paths, measurement method, QML shell, tests, and documentation were
reviewed against the brief. Existing fixtures were preserved. Test additions
protect the original native frame-ownership defect, actual input-method
commits, immutable snapshots and late-worker failures, outline/focus/selection
preservation, and the visible export options/shortcut. Rendering checks use
the transformed viewport; they continue checking for missing/stray ink and
correct selection on both sides of page gaps/dual dialogue.

Verification completed on 2026-10-28:

The runtime implementation is committed as `1259a2a` (Writing desk), including
the native/export work in `9295f83`, following checkpoint `8d189dd`.

| Command / check | Outcome |
| --- | --- |
| `mise install` | Both project tool pins were already installed. The sandbox prevented global shim/cache writes; builds used the installed pins with writable `/tmp` caches. No upgrade or installation was needed. |
| `mise run check` | 6/6 CTest checks pass, including editor/PDF agreement and independent immutable snapshots. The full native runner repeats this release suite successfully on the final state. |
| `mise run check-debug` | 6/6 pass with ASan/UBSan and LeakSanitizer enabled outside the tracing sandbox; no diagnostics. After the native screenshot assertion correction, the affected editor/export tests were rebuilt and rerun: 2/2 pass. |
| `mise run check-engine` | 2/2 pass. `ldd build/engine-only/layout_tests` has no Qt dependencies. |
| `uv run tools/compare_academy.py --probe build/release/layout_probe` and separately with `build/debug/layout_probe` | Both pass every anchored reference page with only the four documented deviations. |
| Offscreen `uv run tests/editor_pdf_tests.py --editor build/release/apollo_editor --output build/final-verification/academy-editor-pdf --academy` | Pass, including all independent snapshots, both workloads, and 50 edits of the Academy transcription. |
| `tools/check-wayland.sh` | Exit 0. Release suite, 52/52 native interaction/input cases, 13/13 native export cases, all native snapshot fixtures/workload comparisons, both fatal-warning first-paint checks, and four 200-key timing runs pass. |
| Native sanitizer `editor_tests` / `export_job_tests` | 52/52 and 13/13 pass, including actual fcitx5 input and the final shell, with no sanitizer diagnostics. |
| `tools/inspect-writing-desk.sh --smoke` from `/tmp` | Exit 0; native open/closed/compact outline/focus checks and the sample's native PDF export pass. |
| `bash -n` on both runners, help/invalid-option checks, `git diff --check`, font hashes, source/private-include inspection | Pass. ShellCheck is not installed. |

One final native run initially failed a screenshot assertion for the selected
right-hand dual cue. The saved image showed the correct highlight and the
mapped coordinates were correct. Native subpixel glyph rendering at the new
presentation scale left only 12 exact-background pixels in the test's narrow
five-point strip. Sampling eight points above the baseline includes more of
the selection background while staying clear of the preceding line. The
original native failure then passed, including unchanged negative checks for
the right speech/gap. This was a test correction, not an editor workaround;
temporary diagnostics were removed. The full native runner was rerun cleanly
afterward. An earlier local release attempt overlapped relinking and failed
to execute the binary; subsequent sequential release checks passed. Neither
failed attempt supplies performance evidence.

Ordinary/sanitizer/Academy logs are in `build/final-verification/`; the final
native directory above holds `results.txt`, full logs, PDFs, and per-event
`export-{175,444}/events.tsv`. Native look captures are in
`build/release/editor-tests/writing-desk-{closed,open,compact}.png`; browser
comparison captures are `build/prototype-{closed,open}.png`. The handoff smoke
logs/sample/PDF are in `build/desk-review-e1JLn2/`. These are local regenerable
artifacts, not a requirement to commit copyrighted references or build output.

Review found no remaining concrete defect that blocks the bounded experiment
or establishes a switch trigger. The known gaps above remain real limits of
the recommendation. The original `prompt.md` is preserved and excluded from
the spike commits; other initial native/export changes belong to this reviewed
spike and are included explicitly.

The owner has now accepted the QML look and the keep-Qt recommendation, so the
previous visual/stack acceptance gate for `spike/editor-export-final` is clear.
The measured evidence and known limitations remain unchanged by that decision.
The [handoff](spike-handoff.md) remains available for further inspection and
gives the undo/redo shortcuts.
Source-schema, autosave, Git integration, and the rest of the product remain
outside the completed spike. The next proposed task is to plan the first usable
writing milestone, starting with the document format and save/recovery contract.
