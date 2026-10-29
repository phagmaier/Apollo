# Fountain compatibility and intake

Investigated 2026-10-28 against the checked-in official samples and
[official Fountain 1.1 syntax](https://fountain.io/syntax/). Sample provenance and
MIT terms remain in [fixtures/README.md](../fixtures/README.md) and
[LICENSE](../fixtures/fountain-syntax/LICENSE); upstream is
[nyousefi/Fountain](https://github.com/nyousefi/Fountain).
The independent original example and hand-worked semantic expectations are in
[fixtures/import](../fixtures/import/README.md). Private reference files were
not changed or incorporated into fixtures.

## Result and scope

**Current readiness (2026-10-29):** the owner accepted explicit non-equivalent
normalization of the reference's 49 scene gaps and will resolve its two
unfinished speeches by hand ([planning round 25](planning.md#round-25--owner-reference-writing-readiness-settled-2026-10-28)).
The private writable copy preserves both speeches and the byte-exact original.
Fresh single-project native writer sessions meet the unchanged 250 MiB PSS
target in the measured samples; see [readiness evidence](#owner-reference-writing-readiness-2026-10-29).
`writer-subset-5` admission remains 128 KiB / 3,072 elements; imported PDF
remains gated. This does not complete task 2.6 or slices 3/4.


Task 2.6 is larger than one reviewable import change. The spike importer is not
a safe production converter: it joins source lines with spaces, trims action
indentation and transition-significant trailing spaces, accepts ellipses as
forced headings, lacks forced mixed-case cues and several heading forms, rejects
notes/organization, and does not implement Fountain's emphasis fallback rules.
Existing engine/editor gaps for deliberate breaks and case preservation also
prevent claiming faithful editing or PDF. At that investigation there was no
Fountain serializer; task 2.6d adds the bounded one described below.

**2.6a implements lossless intake and read-only review only.** The whole original
is pending conversion, including ordinary screenplay text. The preview labels
findings as candidates; it is not a parser and never grants a clean compatibility
result. Publishing retains the exact previewed bytes, even if the external file
later changes or disappears. The complete byte copy, SHA-256 and provenance are
validated before publication and on reopen. Invalid encodings use a hex view.
Review-only projects cannot enter the editor, autosave writing, or export a
screenplay through Apollo's project/instrumentation paths.

This is a useful preservation/reopen checkpoint, **not completion of task 2.6**.
No notes/stash are declared converted and no editable round-trip promise is made.
The existing unsaved Fountain spike tools remain limited research/instrumentation
paths; they do not become the new project import workflow.

**2.6b now has two implemented reviewable subtasks: located interpretations and
explicit interpretation copies.** `source/fountain_conversion.*` derives a
versioned screenplay interpretation, named read-only note/stash records,
organization/metadata associations, unresolved ranges and separate layout
limitations. `fixtures/import/interpretations.expected.json` contains hand-worked
complete element/text/dual expectations for every official sample and the
independent original example. These are semantic expectations, not layout
oracles. The interpretation reader replaces candidate hints with types, named
records and exact diagnostics when viewing the new version.

These copies remain **interpretation reviews**. Their stored permissions,
placeholder screenplay and manifest are never upgraded in place.

**2.6c adds a separate bounded editable copy**, admitted by the
[writer preservation contract](editable-fountain-subset.md). It supports a
representative subset through actual editing, undo/redo, autosave, reopen,
recovery and Save a Copy, with independent expected content and associations.
Unsupported cases remain in review. PDF export stays blocked for every imported
working project; complete Fountain compatibility is pending.
**2.6d adds bounded canonical Fountain export** for editable projects, ordinary
and imported, under [`fountain-canonical-4`](fountain-export.md). Each request
checks the current snapshot, including edits after import, against the writer
subset's content rules, speech structure, unambiguous spelling and emphasis
rules; refusals name the element and reason. Output is verified by reading it
back with the frozen revision-1 interpreter, written atomically from a fixed
snapshot while writing continues, and accompanied by a disclosure of omitted
retained material. Hand-written expected text and the independent aw-parser
check import → real edits → autosave/reopen → export. Unsupported content stays
blocked; nothing is exported lossily.

The frozen revision-1 interpretation remains the immutable historical baseline,
including its original conservative limitation messages. The working wrapper
validates its separate subset contract without rewriting that baseline.

### Conversion revision 1 and migration

- Element envelopes and contributing text slices use zero-based half-open
  original byte ranges, including any BOM/CRLF; one-based first/last occupied
  lines accompany them. Normalized text uses LF for authored breaks. Emphasis
  spans use the existing byte-in-memory/code-point-on-disk contract. Source
  slices describe contributing runs before emphasis decoding; they are not a
  per-character editor cursor map. Element envelopes locate normalized breaks.
- Notes and stash retain the exact UTF-8 interior text, including CRLF and
  spaces, with deterministic names (`Imported note 1 (line N)`), independent
  record IDs and source anchors. Anchors prefer a containing/preceding body
  element, or the following body element for leading material; byte provenance
  remains authoritative. Notes/stash are outside screenplay elements.
- Sections retain depth and the nearest preceding shallower section as parent,
  including skipped levels. Synopses anchor to the latest section or scene.
  Scene numbers retain exact values and heading IDs. Unknown title fields
  retain the whole field/continuations with their key and source range. None
  of these are silently turned into printed acts or screenplay text.
- Complete notes stop at a paragraph boundary unless a blank line contains at
  least two literal spaces. Unclosed notes quarantine only that paragraph;
  unclosed boneyards quarantine to EOF. Nested delimiters, orphan/escaped
  delimiters, invalid encoding/control characters, crossing/unsupported emphasis,
  unpaired duals and ambiguous opening title keys have located unresolved
  records. Known unmatched emphasis stays literal. Literal fallback text and
  incomplete relationships may appear in the interpretation, with output blocked.
  Crossing, unequal delimiter runs and intraword emphasis are conservatively
  reported with literal text; this is a tested subset of Markdown emphasis.
- The older sample's `I./E.` spelling is retained as action with a diagnostic;
  it is not silently accepted as a published heading prefix. Lyrics within
  speech retain lyric elements but flag the missing source-v1 speech relationship.
  Forced headings require a single dot directly followed by an alphanumeric.
- Action tabs/spacing, extra blank lines, authored case and deliberate breaks
  remain preserved in source and/or interpreted text, with located layout
  limitations. This does not establish faithful PDF layout or glyph coverage.
- Revision 1 is bounded to **1 MiB / 100,000 source lines**. Larger intakes keep
  their full original (up to the unchanged 16 MiB intake limit) and get one
  unresolved whole-source `conversion-limit` range. Interpretation metadata is
  bounded to 64 MiB at publication/reopen, without truncating stored records.
  The UI caps its display list at 200 and labels clipped excerpts; stored data
  and complete original are independent of that display cap.

Migration is explicit **Interpret in a New Copy**, available from a preview or
an existing review-only reader. It writes a new staged project with a fresh
identity; it never upgrades an existing project in place. Existing readers
first revalidate their locked project and retained evidence. Failure keeps the
old reader/lock/session state; success opens the new verified review copy. An
activation failure retains the published copy/path for inspection. The external
file is not reread during this operation. The CLI equivalent for a new capture
is `apollo_editor NEW_PROJECT --interpret-fountain FILE`; an existing destination
is refused. `--retain-fountain` continues to create the older intake version.

Project manifest v2 `import-review` remains the mode. Intake metadata v1 remains
unchanged/readable. New copies use metadata v2 `interpreted-review`, embed the
complete conversion revision 1, and carry the original bytes, origin, length and
SHA-256 unchanged. Reopen validates the evidence and compares the complete stored
interpretation against the frozen revision-1 converter. Unknown/newer fields or
revisions are refused; an older 2.6a build refuses metadata v2 instead of dropping
it. Copy/recovery retains both assets. Future converter revisions must preserve
the old reader/algorithm or explicitly require a new build, and migrate through
another copy. Manually changing a version, permission flag or stored result
never grants editable access. The frozen revision includes stored names,
diagnostic wording and output policy; changing those also requires revision
dispatch rather than invalidating existing interpretation copies.

## Compatibility table

The target column records the agreed milestone obligation plus selected canonical
export treatment. Everything in this table remains read-only source in 2.6a;
intake does not assert editable support. PDF refers to a future converted
screenplay, not a PDF of the source reader.

| Construct / evidence | Required editable conversion and Fountain re-export | PDF treatment / current gap |
| --- | --- | --- |
| Scene headings: SceneHeaders | Recognize boundary/context, all official prefixes and case, forced single dot; preserve ellipsis as action; force headings on canonical export when needed | Task 3a: authored case kept, presented in capitals in editor/PDF/export ([case contract](case-handling.md)); letters outside ASCII not capitalized |
| Action: MultilineAction, Indenting | Preserve authored breaks, action indentation, tab intent and extra vertical spacing; explicit `!` where ambiguous | Task 3b: repeated spaces inside lines admitted and shown; indentation, tabs and vertical spacing stay review-only ([spacing contract](spacing.md)) |
| Cues/extensions: Dialogue, ForcedElements | Context-sensitive cues; mixed-case `@`; letters outside extensions; preserve authored extensions and continuation labels; force mixed case on output | Task 3a: lowercase extensions admitted and presented in capitals; forced mixed-case names stay review-only pending an owner exception rule; generated CONT'D unchanged |
| Dialogue/parenthetical: Dialogue | Preserve single speech with deliberate breaks and whitespace-only dialogue lines, including style boundaries; distinguish separator from empty speech line | Task 3b: line-edge spaces not shown or written; a line of spaces inside dialogue is one blank slot, written as two spaces; breaks are page-split points; blank first/last lines stay refused ([spacing contract](spacing.md)) |
| Dual: DualDialogue, Dialogue | Preserve paired speeches and second-cue caret, including valid zero/multiple-space markers; reject/surface unpaired or ambiguous relationships | Existing fixture is narrower than complete authoring/import contract |
| Lyrics: ForcedElements | Explicit tilde per lyric line; preserve lyric-in-speech intent for review | Current parser treats a tilde inside dialogue as text |
| Transitions: Transitions, Indenting, Simple | Honor blank boundaries and trailing-space disambiguation; use forced `>` for canonical transitions | Existing trim-based parser loses distinctions |
| Centered/act text: CenteredText | `>text<`; preserve intended printed label; never infer act label from a section | Existing centered rendering has limited independent fixtures |
| Emphasis/literals: independent example, layout 01 | Bold/italic/underline combinations, escapes, literal punctuation, unmatched delimiter fallback; byte/code-point offsets must agree after decoding | Existing emphasis parser removes unmatched markers then throws |
| Title page: Simple, independent example, layout 07 | All supported fields independently optional, Authors alias, multiline indentation, field emphasis; canonical known-field spelling | Task 3d adds the structured form for editable projects and independently checked multiline/repeated-field layout. Fountain still requires nonblank single-line values up to 48 characters, with capital titles; import admission and imported-PDF gates are unchanged |
| Unknown title fields: independent example | Retain separately with provenance; omit from canonical clean Fountain and disclose omission | Exclude from screenplay/PDF |
| Manual breaks: PageBreaks, Simple | Three or more equals only; canonical `===`; preserve surrounding content and empty pages | Existing importer recognizes exactly three |
| Inline/multiline notes: Notes, Simple | Convert to named read-only notes with text and source location; omit from clean Fountain and disclose conversion | Excluded from screenplay/PDF; 2.6a only locates candidates |
| Boneyard: Boneyard, Simple | Convert omitted passages to named read-only stash; retain exact text and provenance; omit from clean Fountain and disclose conversion | Excluded from screenplay/PDF; delimiter ambiguity needs explicit handling |
| Sections/synopses: SectionHeaders, Sections-Complex, Synopses, Simple | Preserve hierarchy/text outside screenplay; omit from canonical clean Fountain and disclose omission | Excluded; not printed acts |
| Production numbers: SceneNumbers | Retain value and source association separately; clean Fountain omits unsupported production data with explicit disclosure; no equivalent production-export claim | Excluded from submission PDF; production tools deferred |
| Unicode/encoding: independent and byte-level tests | Preserve valid Unicode and original encoding bytes; require explicit conversion for non-UTF-8; no replacement/deletion on decode | Font coverage needs separate diagnostics; no transliteration |
| Malformed/ambiguous: independent and byte-level tests | Retain affected source, exact location and reason; resolve before claiming affected clean output | Block affected clean export; 2.6a blocks all screenplay export |

Canonical treatment of retained-only organization/metadata is an engineering
choice under the delegated compatibility contract: omit with disclosure, retain
in the project and immutable original. It does not imply equivalent round-trip
of those constructs. Supported screenplay round-trip must still prove types,
text, emphasis, extensions, deliberate breaks, dual relationships and title
fields through an independent parse after edits/save/reopen/export.

## Official sample inventory

Every `.fountain` sample in the repository is exercised by the intake suite.
These are intake assertions, not layout or complete parser acceptance.

| File | Evidence to protect in conversion work |
| --- | --- |
| Boneyard | Inline, whole-line and multiline omitted text, including interior spaces |
| CenteredText | Trim padding inside centered delimiters; distinguish forced transition |
| Dialogue | Lowercase extensions, repeated CONT'D, parentheticals, numeric non-cues, two-space empty line and indentation |
| DualDialogue | Two speeches and the second cue's caret |
| ForcedElements | Forced multiline action, mixed-case cue and lyric lines within speech |
| Indenting | Indented transition/cue/parenthetical/dialogue, preserved action spacing |
| MultilineAction | Deliberate action lines, internal uppercase line that remains action |
| Notes | Whole-line, multiline and inline notes |
| PageBreaks | Both three and nine equals signs |
| SceneHeaders | Prefix variants, case, context and ellipsis; sample `I./E.` is outside the current published prefix list and needs explicit interpretation |
| SceneNumbers | Numeric, mixed-case, dashed and dotted numbers |
| SectionHeaders | Spaced/unspaced headers and nested depth |
| Sections-Complex | Hierarchy, synopses and scene boundaries together |
| Simple | Mixed document: title, lower-case heading, four-equals break, notes/stash, forced heading, ellipsis and trailing-space nontransition |
| Synopses | Spaced/unspaced synopsis with sections |
| Transitions | Standard/forced transitions; nontransition punctuation and trailing spaces |

## Remaining subtasks and acceptance

- **2.6b.1 / 2.6b.2 implemented:** located conversion model, named note/stash,
  metadata associations, hand-checked corpus interpretations, precise unresolved
  ranges and explicit new-copy migration/reader. Original bytes stay immutable.
  This begins 2.6b; it does not declare the full Fountain grammar complete.
- **2.6b remaining:** extend semantic coverage for syntax outside the exercised
  corpus and resolve diagnostics through an explicit correction workflow.
  Reassess the conservative conversion limits using measured realistic inputs.
  General editable activation still requires fuller authoring/layout evidence.
  The separate 2.6c subset admits only cases meeting its tested preservation
  contract; it does not relax review-only permissions or enable clean exports.
- **2.6c bounded step implemented; remaining slice 3:** complete authoring/layout contracts, title form, case,
  spacing and deliberate breaks; expose unresolved imported material in context.
- **2.6d bounded step implemented:** canonical serializer, independent parsing
  of import → edit → save/reopen → export, semantic expected fixtures and
  Fountain-specific blocking/disclosure, for the writer subset only.
- **Task 3a bounded case step implemented:** headings and cues keep authored
  case and are presented, printed and exported in capitals; `writer-subset-2`
  admits lowercase headings and cue extensions ([case-handling.md](case-handling.md)).
- **Task 3b bounded spacing step implemented:** line-edge spaces are kept but
  not shown or written, blank dialogue lines and repeated spaces in action and
  dialogue are laid out, exported and (`writer-subset-3`) imported, and speeches
  split at deliberate breaks ([spacing.md](spacing.md)).
- **Representative-workflow bounded step:** `writer-subset-4` /
  `fountain-canonical-4` preserve six typographic punctuation marks exactly.
  Size, syntax, spacing and imported PDF gates are not relaxed. Measurements
  and the remaining full-length blockers are recorded below.
- **Slice 3/5 compatibility gate, remaining:** widen export with each authoring
  contract (forced mixed-case names, title case, Unicode, action indentation and other spacing in spacing.md's limits, multiline title/lyric/centered,
  lyric within speech, complex emphasis) and independent examples for each;
  correct PDF still needs independent layout references, not just agreement
  with Apollo's engine.

The published syntax has examples that exceed the older parser samples; neither
corpus alone is exhaustive. Intake tests cannot establish font fidelity,
editing, note conversion, export compatibility, power-loss durability, or import
performance on representative full-length scripts.

## Representative full-length assessment (2026-10-28)

Two existing local references were assessed on disposable copies in a
mode-0700 ignored build directory. Neither original was edited; SHA-256 checks
confirmed both unchanged. No private screenplay text or fixture derived from it
is committed. The owner's accompanying PDF has 88 pages; that is a workload
size observation, **not** validation of Apollo's imported layout.

### Blockers and distance to a writing workflow

| Measured category | Owner reference | Second local reference |
| --- | ---: | ---: |
| Original bytes | 100,514 | 148,439 |
| Interpreted elements, including title fields | 2,319 | 2,759 |
| Character-blocked elements, before → after punctuation support | 9 → 0 | 771 → 3 |
| Unresolved syntax ranges | 2 empty speeches | 0 |
| Located unsupported layout | 49 extra-vertical-spacing locations | 113 action-whitespace locations |
| Elements refused for significant spacing | 0 | 304 |
| Title-case refusal | 0 | 1 |
| Tabs present | No | Yes |
| Editable size/element gate | Exceeds 64 KiB / 256 elements | Exceeds 64 KiB / 256 elements |
| Export-check diagnostics on interpreted model, before → after | 11 → 2 | 962 → 194 |

Counts are not additive: one element can have several reasons, a layout code
is deduplicated in the admission message, and unresolved syntax/size are
whole-import gates. The actual admission message counts fall from 12 to 3 and
1,079 to 311. The second reference's remaining characters are one U+00A0
(nonbreaking space) and two U+00B0 (degree signs); its remaining export-model
diagnostics are those three characters, 190 repeated-space elements and one
title-case refusal. The owner's two empty speeches still prevent clean output.
The 49 vertical-spacing locations must not simply be collapsed to get a pass.

Neither full reference is editable yet. Actual `--edit-fountain` requests
refuse without publishing a project, and actual exports of their review copies
refuse without replacing an existing output. The export-model counts above
come from an in-process diagnostic probe of the interpretation, **not** an
enabled export path or proof of round-trip fidelity for these scripts. The
official committed sample corpus remains 6/16 admitted; it is not a full-length
workload or an exhaustive Fountain grammar.

Sanitized **synthetic**, not quoted, examples: `He can’t wait…` and
`INT. ROOM — NIGHT` now preserve their punctuation; a cue without dialogue,
extra blank separator lines, action indentation, tabs and nonbreaking spaces
remain visible and gated. No script-specific wording or located excerpts are
needed to explain these categories.

### Responsiveness: measured, not inferred

Release builds on this machine; warm repeated runs, not a controlled cold-start
benchmark. The in-process baseline used five runs per script; actual CLI and
native review paths used three. Native review uses the real `ProjectAccessFlow`
and confirms Wayland/OpenGL. Its clock starts after `QGuiApplication`
construction and includes font registration, QML setup, intake and the
admission result; “ready” means reader state installed, not compositor/display
latency. Other verification ran in the background, so these are indicative
samples, not isolated performance budgets or before/after speedup claims.

| Operation (median milliseconds) | Owner reference | Second local reference |
| --- | ---: | ---: |
| Baseline lossless intake and candidate view, in process | 4.5 | 4.5 |
| Baseline located interpretation, in process | 14.2 | 19.2 |
| Baseline editable admission/refusal, in process | 15.3 | 21.6 |
| Baseline refused export-model check, in process | 10.8 | 9.9 |
| Actual CLI interpretation plus staged publication, process wall time | 333.8 | 413.1 |
| Current native read-only review ready, including admission | 191.8 | 301.3 |

CLI publication ranges were 333.8–345.2 and 409.5–415.7 ms; native readiness
ranges were 183.7–197.3 and 271.6–310.1 ms. A **successful** CLI Fountain
export of the 8-element / 199-byte independent punctuation fixture took
66.5 ms median, 58.3–75.4 ms across five warm process runs, including project
validation and atomic output. It is deliberately labeled a small fixture:
successful full-length import/edit/export latency and full-length typing
responsiveness remain unmeasured because neither representative input passes
the fidelity gates. No timeout/cancel, cold-start or owner-comfort claim follows.

### Selected bounded improvement

Typographic punctuation was the largest counted content blocker, so
`writer-subset-4` / `fountain-canonical-4` add exactly **— ‘ ’ “ ” …**, not
general Unicode acceptance or ASCII substitution. The frozen revision-1
converter and older copy contracts are unchanged. Size limits are retained:
lifting them alone would admit neither reference and would not establish
authored-spacing or syntax fidelity.

The original synthetic `punctuation-subset` fixtures provide independently
written authored semantics and canonical output, including a bold span after a
curly apostrophe, an authored break, a parenthetical, CONT’D and dual dialogue.
Conversion/admission and exact export regressions fail on the old repertoire.
The real QML flow imports, types an ellipsis, undoes/redoes, autosaves, reopens
and exports the hand-written content. Every new character has a nonzero,
fixed-pitch glyph in all four pinned faces; a writer capture was visually
inspected. The executable/oracle flow additionally checks original bytes,
code-point styles and independent aw-parser readings of the original,
hand-written export and a typed/reopened export. Imported PDF remains blocked,
including preservation of an existing PDF on refusal.

This is progress toward writing existing scripts, not a usable full-length
import claim. Next fidelity work is vertical/action spacing and ambiguous
empty speeches, then independently exercised full-length admission bounds;
the second reference also needs its title and residual-character contracts.
No correction UI, general Unicode policy, title form or expanded PDF support
was added.

### Verification and review

- Release: `mise run check`, **23/23**, completed inside the successful native
  preflight. Includes both executable Fountain suites and the independent
  aw-parser check of 13 original/expected/edited inputs.
- ASan/UBSan: `mise run check-debug`, **23/23**, no sanitizer report.
- Engine-only: `mise run check-engine`, **2/2**.
- `tools/check-wayland.sh --preflight`: editor **62/62**, export lifecycle
  **13/13**, project flows **36/36**, native PDF snapshots and 203-/507-page
  first-paint/export checks pass. Editor/input uses native threaded OpenGL and
  fcitx5; project flows use synthetic Qt events and software scenegraph.
  Full native typing measurements were not rerun.
- `tools/check-spacing.sh --smoke`: native focused spacing **6/6**, existing
  spacing export/persistence flow **3/3**, synthetic native project agreement
  and actual Fountain export pass. `bash -n` passes. This does not record
  owner acceptance of the provisional spacing feel.
- `uv run tools/compare_academy.py --probe build/release/layout_probe`: full
  text and all 11 anchored body pages pass with the four documented deviations.

Completion review covered the shared repertoire, strict validation of older
copies, Unicode/code-point emphasis, authored-source preservation, actual
publication/reopen/export, the unchanged PDF block, and the isolated manual
runner. It caught and removed a runner assumption that spike `--create` keeps
trailing dialogue spaces; the checkpoint now creates its leading space by a
real Enter. An initial new test asserted active-reader rather than preview
state; that test setup was corrected. No outstanding concrete in-scope finding
remains. An overlapping release configure and an initial sanitizer command
deadline were rerun successfully without relaxing checks.

Evidence: release/debug `build/{release,debug}/Testing/Temporary/LastTest.log`,
native `build/wayland-check-20261028-202516-i6rXY1/`, spacing runner
`build/spacing-check-ssx74u/`, aggregate JSON and red regressions under
`build/workflow-assessment-g03orzwl/`. **The assessment directory also contains
disposable private copies: do not publish that directory.** Temporary probe
sources/binaries were removed. No dependency pins, engine layout rules or
independent layout references changed.

## Full-length workflow continuation

### Actual meaning of the blockers

The owner's disposable 100,514-byte reference contains **49 distinct runs of
exactly two empty lines before scene headings**: 37 after action and 12 after
dialogue. No run is leading, trailing, longer, whitespace-only or an internal
dialogue blank. The two separate unresolved speeches each contain a cue and
parenthetical without dialogue; neither overlaps a spacing run. Counts therefore
do not describe 51 occurrences of one formatting problem.

[Official Fountain syntax](https://fountain.io/syntax/) promises vertical
whitespace preservation. Independent synthetic examples confirm aw-parser 0.1.1
emits one additional separator token for the second blank line. Apollo cannot
silently discard these lines or call normalized output spacing-equivalent.

The smallest implemented workflow is an **unchecked-by-default, explicit
normal-scene-spacing choice**, only for that exact gap pattern. Original bytes
and frozen located limitations remain immutable. Every affected line is surfaced
in the working imported-material reader, with an export-panel/completion/CLI
disclosure of the count and non-equivalence. Other vertical spacing, longer runs,
whitespace-only lines, or removed material inside a gap remain gated. Cancel or
clear the option before publication; afterward keep/revisit the original review
and make another copy to reverse the normalization decision.

Empty speeches remain exact unfinished elements instead of forcing an invented
line or deletion before editing. A live list navigates every affected cue.
**Treat speech as action** is an explicit type-only change to the cue and its
parentheticals, preserving every word, style and identity in one undo step;
manual dialogue authoring is the alternative. Stale/now-valid and dual-related
corrections are refused. Unresolved empty speeches block Fountain export, not
saving. Other unresolved source syntax still blocks editable admission.

New copies use `writer-subset-5`, working metadata v2 and the required
`normalizeSceneSpacing` boolean. The frozen converter, source schema, old copy
contracts, canonical Fountain revision 4 and imported PDF gate are unchanged.
Admission is bounded to **128 KiB / 3,072 elements / 256 retained records**,
not the converter's 1 MiB limit; later unfinished writing remains saveable.

The full original's interpreted types, presented text, emphasis spans, dual
flags and title fields matched aw-parser on all **2,319 interpreted elements** before
correction. The oracle adapter needed its independently documented `shot` token
mapped to action for two isolated uppercase actions; unknown token categories
now fail rather than silently disappearing from comparisons. Synthetic `HOLD.`
checks that representation separately, without using private wording.

The script's private original is never edited or committed. Tests use independently
written `scene-spacing` fixtures. Measurement artifacts contain disposable
private copies, screenshots and output; only aggregate numbers may be published.


### Responsiveness method and initial finding

`tools/measure-fountain-workflow.sh` drives the actual QML import, explicit
spacing choice, issue picker/correction with real Undo/Redo, keyboard edits,
autosave acknowledgments, Fountain panel, Home, reopen and identical re-export.
The measurement-only corrections are not author acceptance. Each run uses
isolated application state, a fresh disposable project, native Wayland/OpenGL
and synchronized `frameSwapped` observations; these are synthetic QWindow
events, not device/compositor input latency. Preview timing begins at its
control click; publication timing includes staged creation, verified activation
and the first writer frame. Reopen timing begins at Open Project. Build/startup
before Home is excluded. Autosave acknowledgment includes correction/undo/redo
and typing revisions, including revisions superseded by a newer save.

The first enabled-flow measurements exposed a saving-target miss despite
responsive typing. A throwaway aggregate-only profiler isolated repeated frozen
interpretation validation during creation/open/recovery. Working operations now
derive admission from already verified bytes and render the original through
the shared source-view formatter instead of constructing and discarding another
full interpretation reader. Validation retains **one exact complete
metadata-plus-original value per thread**: byte-identical evidence may reuse a
successful deterministic result; any changed byte requires full verification.
Qt copy-on-write keeps a stored key immutable when callers edit their copies.
This is not a path/mtime/hash-only trust cache. Filesystem, manifest, lock and
dependency checks are unchanged. Altered original bytes, nested interpretation,
contract/version and unknown metadata remain refused after a valid open.

Review also corrected the runner's build environment: application XDG isolation
must start in the executable, **after** using the existing mise toolchain.
Changing tool-manager XDG paths before building had unnecessarily reinstalled
tools into disposable output. Those preliminary runs are not the final
performance evidence.


### Final admitted-workload measurements

Three isolated warm runs per workload, **120 keys per run**, with autosave and
the live diagnostic list enabled. The boundary is an independently generated
ASCII action workload at both admission limits, not derived from private text.
Times below are medians across runs unless labeled maximum.

| Measured operation / observation | Owner disposable copy | Admitted synthetic boundary |
| --- | ---: | ---: |
| Original bytes / elements | 100,514 / 2,319 | 131,072 / 3,072 |
| Apollo editor pages (not imported PDF validation) | 91 | 110 |
| Import preview → first reader frame | 198.6 ms | 246.0 ms |
| Explicit editable publication → first writer frame | 1,332.6 ms | 1,446.5 ms |
| Saved-project reopen → first writer frame | 665.0 ms | 731.7 ms |
| Reopen maximum across runs | 716.2 ms | 982.9 ms |
| Key handling p95 | 7.9 ms | 7.6 ms |
| Key → synchronized frame p95 | 17.1 ms | 16.9 ms |
| Key → synchronized frame maximum | 27.8 ms | 29.6 ms |
| Frames above 100 ms / unobserved frames | 0 / 0 | 0 / 0 |
| Mutation → verified autosave acknowledgment maximum | 857.8 ms | 931.6 ms |
| GUI autosave capture maximum | 2.6 ms | 4.0 ms |
| Fountain request → successful completion | 113.6 ms | 82.5 ms |
| Export mouse handler/capture/dispatch upper bound, maximum | 17.0 ms | 21.4 ms |
| Reopened Fountain re-export → completion | 79.1 ms | 93.7 ms |
| PSS after typing, maximum sampled | 218.4 MiB | 240.7 MiB |
| PSS maximum of writer-ready / after-typing / after-reopen-workflow samples | 250.8 MiB | 270.3 MiB |

The admitted workloads met the observed typing and one-second save/reopen
targets in these samples, not a durability guarantee or worst-case proof.
These historical full-workflow samples do **not** establish a writer-only
250 MiB failure or waive the target: the harness retains comparison snapshots
and exercises reopen in one process. The clean single-project measurements
below supersede that unresolved writer-memory verdict, not these observations.
The complete milestone's later-feature and 500-page measurements remain open;
a sampled maximum is not an unsampled peak bound.

The initially considered **4,096-element** bound was **not enabled**. Even after
the validation improvement its exact-size stress workload reached 1,089.8 ms
autosave acknowledgment and 283.9 MiB after typing. The final element bound is
3,072; source bytes remain 128 KiB and retained records 256. The tests separately
admit the exact boundaries and refuse one additional byte/element on otherwise
supported writing, so another fidelity refusal cannot mask a missing limit.

Every final run exercised the actual import review/spacing choice, both
empty-speech corrections and Undo/Redo when present, editing, verified autosave,
Home/reopen, Fountain-panel export and byte-identical re-export. Model comparison
kept every type, word, emphasis span, identity, dual flag and imported asset except
the deliberately changed types and typed measurement text. aw-parser separately
matched the saved/exported meaning of **all six final exports**. Synthetic
hand-written fixture expectations protect the same choices without private text.
The PDF request was refused in every run and left an existing destination intact.
There were no redacted Qt messages in the six final runs.

The owner subsequently accepted the explicit non-equivalent scene-spacing
normalization and reserved each unfinished cue's resolution for manual writing
([planning round 25](planning.md#round-25--owner-reference-writing-readiness-settled-2026-10-28)).
The earlier automated corrections were **disposable rehearsal choices**, not
acceptance of either speech's correction. Spacing-equivalent reconstruction
is not required for this milestone. Declining the unchecked option still keeps
that import gated; arbitrary spacing, the second reference's unsupported content
and imported PDF remain blocked.

Aggregate evidence: ignored
`build/full-length-workflow-hgj6ljwr/{native-admitted,measured-summary,final-independent}.json`;
the six `native-*-admitted-*` folders retain actual application artifacts.
**Those directories contain private projects, screenshots and exports: do not
publish them.** Earlier `native-*-warm-*`, `native-*-final-*` and
`native-*-verified-*` folders are explicitly pre-final candidate measurements.
No private text or derived fixture is committed. Temporary profiling source,
binaries and unnecessary isolated tool installations were removed.

### Completion review and recheck

Direct review covered the exact spacing opt-in, legacy contracts, complete-byte
validation cache, live speech diagnostics, type-only correction/Undo, QML
publication/export, independent expectations and the measurement runner. No
additional functional change was needed. The element-count label above was
clarified to include title fields; the recorded full-length measurements were
preserved. Untracked Python caches were removed and the tracked generated cache
restored. The private original and existing rehearsal directory were not opened
or changed during this review.

A fresh native Wayland/OpenGL runner smoke used only the public
`scene-spacing` fixture: 12 keys, both speech corrections with Undo/Redo,
explicit spacing selection, exact autosave/reopen and byte-identical Fountain
re-export all passed. The original fixture stayed unchanged, PDF remained
blocked, and no Qt messages were redacted. The writer and diagnostic-bar
captures were visually inspected. These small synthetic checks do not replace
the full-length evidence or owner comfort/spacing/speech decisions above.
Artifacts are ignored under `build/full-length-review-native/`.

Required rechecks pass: `mise run check` **23/23**, `mise run check-debug`
**23/23** with no sanitizer report, and `mise run check-engine` **2/2**.
Both Qt suites include the independent aw-parser export oracle. Logs are
`build/full-length-review-{release,debug,engine}.log`. The initial sandboxed
release attempt could not write uv's cache; the successful release and sanitizer
runs used normal cache access outside that restriction, with default leak
detection. `bash -n tools/measure-fountain-workflow.sh` and `git diff --check`
pass. A separate CR-only synthetic-fixture CLI import also retained byte-exact
original evidence and disclosed both normalized gaps. Earlier Academy and
full-length native measurements remain the evidence recorded above; they were
not repeated during that completion review. Round 25 and the readiness evidence
below supersede its pending owner-decision and writer-memory status.

## Owner-reference writing readiness (2026-10-29)

This is the bounded readiness step authorized after round 24, not slice 3 or 4.
No application, layout, admission, source-format or export-contract code changed.
The decisions are owner-confirmed in [round 25](planning.md#round-25--owner-reference-writing-readiness-settled-2026-10-28);
the measurements and rehearsal actions here are engineering evidence only.

### Private writable copy and live writer

The real QML import review started with **Use Apollo's normal scene spacing**
unchecked and editable admission refused. Selecting it explicitly admitted the
100,514-byte / 2,319-element reference at the unchanged 128 KiB / 3,072-element
bound and published a separate writable project. All 49 normalized gaps remain
located in imported material and counted in the export disclosure.
SHA-256 before/after and of `imports/original.fountain` match; the external
original was never the writing/save target.

Both unfinished speeches entered unchanged. The actual live issue-list controls
selected the exact cue ranges, before and after fresh-process reopen. The real
Fountain panel disabled export; a direct request also refused without changing
an existing destination. Imported PDF refused without creating an output.
No speech was corrected or invented on the owner's copy.

An ordinary native `apollo_editor` session received 120 compositor-delivered
keys through `wtype`, saved the exact changed snapshot, displayed **Saved**, and
closed normally. A separate process reopened that saved writing with both
issues still navigable and export still blocked. The bounded rehearsal text was
then removed through the writer and autosaved; final text, types, styles, IDs
and title fields equal the original imported writing. Only the save revision
and derived recovery/session history advanced. Memory sessions likewise removed
their rehearsal text, leaving both speeches for the owner.

Private handoff: `build/owner-readiness-20261029/handoff.txt`. It contains the
open command, the two original source-line locations and issue-list positions,
checks and artifact paths. No private wording, source-derived fixture, screenshot
or export is incorporated into this document or committed.

### Clean writer memory

Three fresh processes per workload, sequentially: each runs the **ordinary
application**, one already-published project, isolated writable XDG state, native
Wayland/fcitx and threaded OpenGL. No comparison model is retained inside the
application, no export worker runs, and no process reopens or switches projects.
The external observer owns the comparison JSON; it is not part of application
PSS. The primary is the standard generated 175-scene, seed-1 **nominal 200-page**
workload; the real writer displays **203 pages**, as does its existing agreement
check. The imported writer displays **91 pages**.

Externally read `/proc/PID/smaps_rollup` every requested 50 ms from process launch
through post-autosave: 437 primary and 436 imported observations across the three
runs. Writer-ready is sampled after three seconds; 120 real Wayland keys type
`readiness ` repeatedly into the first nonempty action at requested 20 ms spacing
(about 2.97 s including dispatch). Settled-after-typing is a separate sample
200 ms after dispatch ends. Post-autosave is sampled one second after the exact
typed snapshot is observed on disk; **Saved** is also checked on the actual
surface. Autosave may already have completed at the settled sample: these are
separate observations, not claims of three mutually exclusive saving states.

Each phase below is the **highest observation across three runs**, in MiB:

| PSS observation | Nominal 200-page primary (203 displayed) | Imported copy (91 displayed) |
| --- | ---: | ---: |
| Writer-ready | 112.3 | 148.5 |
| Settled after typing | 119.2 | 159.4 |
| Post-autosave | 119.2 | 149.4 |
| Maximum sampled from launch through post-autosave | 120.0 | 169.4 |

**Measured verdict: both pass 250 MiB**, including every sampled observation.
No over-target cause triggered profiling; no speculative optimization was made.
This is six short bounded sessions, not a continuous peak bound, extended
writing soak, cold-start timing, 500-page memory pass, export-worker memory
measurement, or validation with the unfinished milestone's later assistance.
An initial exploratory heading-typing session and a failed primary title-field
cursor-targeting rehearsal are excluded from this clean-method dataset.
The observed sampling interval was approximately 51.8 ms median, 53.3 ms
maximum; reading `smaps_rollup` adds overhead to the requested 50 ms delay.
Intake/publication peaks were not part of these saved-project writing sessions.

### Resolved synthetic export and final checks

An independent public `scene-spacing` fixture copy, not the owner's writing,
used the native real-QML runner with explicit spacing selection, both one-step
speech corrections, Undo/Redo, 12 synthetic QWindow keys, verified autosave,
Home/reopen and successful byte-identical Fountain re-export. aw-parser 0.1.1
then independently read **both actual exported files** as the complete saved
writing, including types, presented text, styles, dual flags and title fields.
Those correction choices apply only to synthetic material. The owner's copy
remains unresolved and correctly export-blocked.

Required final checks pass:

- `mise run check`: **23/23**, including the independent Fountain oracle.
- `mise run check-debug`: **23/23**, default ASan/UBSan checks with no sanitizer
  report.
- `mise run check-engine`: **2/2**.
- `tools/check-wayland.sh --preflight`: editor **71/71**, export lifecycle
  **13/13**, project flows **37/37**, native PDF snapshots and first paint.
- `tests/editor_pdf_tests.py --academy`: fixtures and both generated workloads
  still agree at 0/50 scripted edits; the local Academy transcription agrees
  too. `compare_academy.py` passes all 11 body pages with the four existing
  documented deviations. No profile or independent fixture changed.

No final functional check failed. Initial release/debug/native/measurement
configure attempts failed because ignored CMake caches referenced a removed
`/tmp/apollo-mise/.../ninja` binary. Both cached `CMAKE_MAKE_PROGRAM` values were
reset to the installed pinned Ninja, without changing project configuration or
application code; the final runs above then passed. The failed attempt logs
remain distinct from the `*-final.log` evidence. No full native typing/export
timing run was required or performed: editor/layout code did not change.

Final logs: `build/owner-readiness-20261029/{release,debug,engine,native-preflight,synthetic-export,academy,academy-editor-pdf}-final.log`.
Native preflight's additional synthetic-only artifacts are in
`build/wayland-check-20261029-011755-UOh03A/`. The resolved synthetic workflow is
under `synthetic-resolved-final/` within the private readiness directory.
`synthetic-independent-aggregate.json` records its independent parse;
`verification-aggregate.json` records the required checks.
Direct completion review checked these outcomes against the prompt and the
unchanged contracts. No new application defect or permanent test addition was
needed; disposable probe source/binaries were removed.

### Remaining gates

The owner still resolves both speeches by adding dialogue or explicitly choosing
**Treat speech as action**; saving is already allowed, Fountain export is not.
The accepted 49-gap normalization is disclosed non-equivalence, not arbitrary
spacing support. Tabs/NBSP, the other reference's title/content gates and imported
PDF remain blocked. General Fountain compatibility, extended comfort/soak and
the later writing features remain outside this completed readiness step.


All detailed material is under mode-0700 ignored
`build/owner-readiness-20261029/`: private project, screenshots, source hashes,
located issues, raw samples and runtime logs. Only aggregate JSON is publishable:
`clean-memory-aggregate.json`, `memory-summary-aggregate.json` and the explicitly
named `*-aggregate.json` results. The earlier full-length workflow directories
remain private and retain their historical observations.
