# Apollo — first usable writing build

2026-10-28. Status: requirements planning complete; milestone scope and final
product choices agreed. Engineering designs below are proposals to validate
within the implementation slices. No owner question from this interview remains
pending. Implementation requires task-specific authorization; the progress
record below distinguishes completed work from remaining proposals.

**Current status (2026-11-05):** slice 1 and tasks **2.1–2.5** are done.
Project sessions are guarded; Save a Copy becomes active without resetting the
editor; Reload preserves current writing first; recovery/emergency packages and
explicit failure-close choices are implemented and verified as recorded below.
**2.5 — Bounded home/new/open is done.** Task 2.4 was finalized and committed
as `2d61ac4`; task 2.5 adds verified recents, staged blank creation with safe
collision handling/selectable locations, and lossless in-window switching.
Task 2.5 is finalized as `be35108`. **2.6a — lossless intake and read-only
review is done**, as a bounded first subtask; full editable import remains
incomplete. Initial **2.6b** interpretation/copy work is finalized in `d140a1b`.
The next bounded **2.6c** step admits an independently tested writer subset
(`e80e331`). **2.6d** adds bounded canonical Fountain export for editable
projects whose current snapshot meets the [export contract](fountain-export.md);
PDF of imported writing remains gated. See the
[preservation contract](editable-fountain-subset.md), evidence below and the
[compatibility table](fountain-compatibility.md).
The representative assessment added six preserved typographic punctuation
marks in `writer-subset-4` / `fountain-canonical-4`. The subsequent
**`writer-subset-5` full-length continuation** admits the owner's representative
script with explicit scene-spacing normalization while preserving unfinished
speeches; the original stays immutable. The independently exercised bound is
128 KiB / 3,072 elements, not general Fountain compatibility.
The owner settled the normalization, manual speech resolution and unchanged
250 MiB memory target in [planning round 25](planning.md#round-25--owner-reference-writing-readiness-settled-2026-10-28).
**Owner-reference readiness is done as a bounded step:** a private writable
copy leaves both speeches for the owner; clean single-project memory and the
required release/sanitizer/native checks pass. See
[readiness evidence](fountain-compatibility.md#owner-reference-writing-readiness-2026-10-29).
Crash loss during copying remains unverified. The bounded case/spacing and
emphasis steps are recorded below; remaining slice 2 flows and the rest of
slices 3–6 are still pending. Concurrent heavy I/O can exceed the measured
one-second crash-loss target; see task 2.3's evidence and limits.
**Task 3d — structured title-page form is done for this bounded step**, with
release, sanitizer, engine and full native evidence below. Other slice 3
controls and writing assistance remain separate work.
**Manual page-break authoring is done for its bounded slice 3 task**, with
whole-element/speech-safe controls, marker-only deletion, undo, autosave/reopen
and independently checked PDF/Fountain output. Final release, sanitizer,
engine and native evidence follows below; imported PDF remains blocked.

**Dual-dialogue authoring is done for its bounded slice 3 task.**
Format → Toggle Dual Dialogue / Ctrl+Shift+D pairs complete adjacent speeches
from the second speech. The owner chose explicit unpairing before structural
changes; incomplete/oversized groups retain writing with visible diagnostics.
See the [contract and evidence](dual-dialogue.md). Other milestone work and
imported PDF remain gated.

**Task 3e — element-type controls is done (2026-11-04), for this bounded step.**
The live selector and Ctrl+1–8 cover all eight body types, including lyrics and
centered act labels. Release and sanitizer suites pass 24/24, engine 2/2;
affected editor/project-flow/oracle checks pass again after the final menu fix.
Real controls, persistence, independent Fountain/PDF and native Compose/wtype
checks pass. fcitx is unverified here, and the existing heavy-I/O crash-loss
limit remains. See [the contract and evidence](element-types.md) and
[task result](#task-3e--element-type-controls). Task 3f below completes the
bounded selection/clipboard step; shortcut-reference, assistance/navigation
and interchange work stays pending.

**Task 3f — selection and clipboard behavior is done for this bounded step (2026-11-04).**
This bounded task defines ordinary replacement, deliberate breaks and paste
boundaries, preserves external text/type and internal emphasis, and verifies
one-step undo and existing title/page-break/dual protections. Release and
sanitizers pass 24/24 each, engine-only 2/2, plus native Compose/primary-selection
and compositor-key checks. See [the contract](selection-and-clipboard.md) and
[results and verification limits](#task-3f--selection-and-clipboard-behavior).
Task 3g below completes shortcut lookup; assistance and broader interchange
remain subsequent work.

**Task 3g — shortcut reference and action labels is done (2026-11-04).**
The rail's **Shortcuts** button or F1 opens the writing reference; **Edit**
exposes Undo/Redo and clipboard actions with key labels and live availability.
Release and sanitizer suites pass 24/24 each, engine-only 2/2, native project
flows 47/47, plus focused final UI and compositor-key checks. See
[the result and verification limits](#task-3g--shortcut-reference-and-action-labels).

**Task 4a — context-restricted writing suggestions is done for this bounded step
(2026-11-04).** It covers
current-script character names, cue extensions and individual heading parts,
automatic first-match selection, real-key Tab/arrow/Escape/Enter behavior,
free text, undo, persistence and excluded-context/input-method checks.
Release and sanitizer checks pass 24/24 each, with final editor checks repeated
after the composition-paste correction; engine-only passes 2/2. Native project flows pass 48/48;
the native editor and focused compositor/Compose checks pass as recorded below.

**Task 4b — US English spelling and personal dictionary is done for this bounded
step (2026-11-04).** F7 / Edit opens explicit replacements and Add; transient
underlines and computer-wide personal words leave source/export formatting intact.
Release and sanitizer suites pass 25/25 each, engine 2/2, with final affected
release and focused native checks passing. See [the spelling contract](spelling.md)
and [task evidence](#task-4b--us-english-spelling-and-personal-dictionary).
Compositor/fcitx/physical-key and sustained-performance checks remain unverified.

**Task 4c — find/replace is done for this bounded step (2026-11-04).**
Edit / Ctrl+F / Ctrl+H opens literal authored-text search with case/whole-word
options, opening-selection scope, navigation and explicit replacement; Replace
All is one Undo step. Release, final affected rechecks, sanitizer and engine
checks pass. Native Qt controls pass individually; compositor shortcut
reliability retains the documented Qt/wtype lookup limitation. See the
[contract](find-and-replace.md) and [task evidence](#task-4c--find-and-replace).
Tasks 4d and 4e below complete bounded diagnostics/navigation; broader Fountain
compatibility remains pending.

**Task 4d — speech-structure diagnostics is done for this bounded step (2026-11-04).**
The owner chose the speech-structure set and both corrections for dialogue
without a cue ([round 26](planning.md#round-26--speech-structure-diagnostics-settled-2026-11-04)).
One located issue list offers explicit, one-step-undo corrections that never
change words. Release and sanitizer suites pass 26/26 each, engine 2/2, plus
focused native Wayland checks. See [the contract](diagnostics.md) and
[task evidence](#task-4d--speech-structure-diagnostics). Task 4e below completes
the bounded navigator; broader Fountain compatibility remains pending.

**Task 4e — live scene navigator is done for this bounded step (2026-11-05).**
Live headings/current-scene tracking, click navigation and whole-scene drag
moves preserve authored content and selection, with one-step Undo/Redo.
Release passes 26/26, sanitizers pass across the full run and affected recheck,
engine 2/2, and focused native Wayland controls/persistence/export pass. See
[the contract](scene-navigator.md) and [task evidence](#task-4e--live-scene-navigator).
Broader Fountain compatibility, slice 2 follow-ups and the rest of slices 5–6 remain pending.

The [specification](spec.md) holds confirmed behavior. [Planning rounds 18–23](planning.md#round-18--first-usable-writing-milestone-boundary-2026-10-28)
record the owner's milestone answers. The [spike report](spike-report.md) holds
the accepted Qt/Writing desk foundation, measurements, and known limitations.

**Task 5a — bounded PDF export lifecycle is done for this bounded step (2026-11-05).**
PDF now offers Cancel Export, a 30-second Cancel/Keep Waiting watchdog, bounded
helper teardown and captured-snapshot status. Home/Close retain autosave safety;
cancellation/failure preserves existing outputs. Active-project destinations
and aliases are refused, including after Save a Copy. Required checks pass
across full runs and final affected rechecks; focused native PDF/leave/copy
checks pass. See [the contract](pdf-export.md) and
[task evidence](#task-5a--bounded-pdf-export-lifecycle). Task 5b — Fountain worker
lifecycle — is next. Broader compatibility, responsiveness and slice 2/6 work
remain pending.

## Intended result and boundary

The owner can create a screenplay or import an existing Fountain script, write
and revise it across multiple sessions, recover after an application crash,
navigate/reorder scenes, and produce clean Fountain and correctly paginated PDF
exports. Daily writing does not depend on remembering Save or using another app
for the agreed essential tools.

| Required in this milestone | Boundary |
| --- | --- |
| Project access | Simple home screen, create/open/import, default folder plus other locations, resume last project/cursor |
| Complete formatted writer | All element types and emphasis in the spec, dual dialogue, manual breaks and act labels, title-page form, keyboard/selector access, undo/redo |
| Everyday assistance | Character/heading-part/extension suggestions, US English spelling and personal dictionary, find/replace, applicable formatting diagnostics |
| Scene navigator | Live heading list, click to navigate, whole-scene drag reordering with one-step undo |
| Persistence | Continuous local saves, accurate status/errors, Retry/Save a Copy, crash recovery, damaged-save recovery |
| Interchange | Fountain import and export with an explicit compatibility contract; immutable imported original; preserved imported material and review summary |
| PDF | Selected Academy-derived profile, editor/PDF agreement, title inclusion option, independent correctness checks, writing during export |
| Imported auxiliary material | Read-only converted notes, omitted/stashed passages, and organizational/unsupported metadata; excluded from screenplay/PDF as agreed |
| Discoverability and running | Shortcut reference and labels on actions; repeatable launch on the owner's Omarchy/Wayland machine |

Full notes/stash authoring, capture/reinsertion, Git versions, alternative-draft
UI, GitHub sync, merge resolution, production tools, dedicated television
layouts, statistics, storyboards, and focus mode stay later work. Archiving and
local deletion are full-product requirements; proposed milestone boundary is
to defer their destructive/lifecycle UI while still supporting create/open and
reliable handling of moved/missing folders. Additional themes and public
distribution are not prerequisites. This sequencing does not remove v1 scope.

## Starting point and retained limits

Retain the engine, independent fixtures, public Qt integration, and accepted
QML Writing desk as the starting point. Qt still supplies general text editing,
selection, input methods, and undo. Review each spike component before reuse;
do not replace Qt's general editor mechanisms with a new text control.

The existing editor has no persisted source/autosave, a subset Fountain
importer, an opening-only outline, and provisional editing rules. The importer
joins dialogue source lines with spaces. A manual-break block can accept text
that PDF omits. Whole-document extraction and pagination are still linear per
edit. Unsupported glyphs currently reject PDF export. These are requirements
for follow-up work, not evidence that the usable milestone already exists.

The spike's primary-workload native results meet its measured targets. The
500-page stress case has less headroom; neither extended comfort nor cold-start
time has been established. Do not generalize those results to saving, spelling,
suggestions, or the finished build.

## Proposed source and project design

These are agent engineering recommendations, subject to the first slice's
validation. The owner chose a portable project folder and structured text, not
an exact schema or file layout.

| Proposed path | Responsibility |
| --- | --- |
| `project.json` | Format version, persistent project identity, and source-file references |
| `screenplay.json` | One active screenplay's structured title fields, ordered elements, emphasis, deliberate breaks, dual-speech relationships, and document revision |
| `imported-material.json` | Read-only converted notes/stash and imported organizational/unsupported data with provenance |
| `imports/original.fountain` | Untouched imported source; never the autosave target |
| `.recovery/` | Bounded recovery snapshots, outside future tracked writing/history |

Use JSON initially because Qt Core already supplies JSON support. Use explicit
type names, stable element identities, deterministic serialization, and schema
validation. Define text-offset units, span bounds, split/join identity rules,
and unknown-version handling before freezing the first schema. The temporary
PDF-worker protocol is not the persisted source format.

The live Qt document and source adapter must preserve deliberate line breaks,
emphasis, empty editing states, Unicode text, title fields, and stable identities
through edits, undo, serialization, and reopen. Generated MORE/CONT'D labels,
page numbers, layout caches, and automatic wrapping are derived output rather
than authored text. Prove line-break/cursor/selection mapping in Qt before
choosing its final in-memory representation.

Saving must accept unfinished or currently unexportable writing. Layout errors,
missing cues, and unsupported font characters must not prevent preserving the
source. Reject malformed source structure on load without overwriting it; do
not confuse a recoverable editing diagnostic with corrupt storage.

The first build has one working screenplay per project. Keep shared imported
material separate from draft content so later Git work has a viable seam, but
do not invent branch/checkpoint behavior now. Later shared notes/stash across
Git branches still require their own design and verification.

Create/import in a staged new folder and publish only a complete validated
project. Never overwrite a nonempty destination. Routine autosave changes one
authoritative screenplay file; title fields are in that same snapshot. Session
state and library recents live separately so moving a project does not depend
on the library index. Proposed default: an Apollo directory under the user's
Documents location, with a home-folder fallback and user-selectable locations.
Choose names without overwriting colliding project folders.

## Proposed saving, failure, and recovery design

The confirmed target is at most one second of lost recent writing after an
application crash with healthy storage. It is not a power-loss, drive-failure,
or damaged-latest-save guarantee.

1. Track content revisions independently of layout and undo grouping. While
   dirty, capture immutable writing snapshots on a bounded schedule; start
   with a maximum 250 ms capture interval, not a debounce that postpones saving
   indefinitely during continuous typing.
2. Serialize and write on one ordered worker. Coalesce pending work without
   letting an older revision replace a newer saved revision. Measure snapshot
   capture, queue delay, write/commit time, and the age of unsaved writing.
3. Replace the source with `QSaveFile`, keeping direct-write fallback disabled.
   Check all write/commit outcomes. Qt documents temporary-file replacement and
   error handling, and warns that direct-write fallback loses atomicity.
   [Qt 6.12 QSaveFile](https://doc.qt.io/qt-6/qsavefile.html)
4. Acknowledgment records the saved revision. Show Saved only if current writing
   is protected at the project destination; completing an older snapshot must
   not clear the dirty state for later edits. Flush on project switch/normal
   close without blocking the GUI on filesystem I/O.
5. Keep validated older snapshots for recovery. Initial retention proposal:
   the last two completed saves and up to ten minute-spaced snapshots. Do not
   prune the only verified copy during a failure. Retention must not make a
   current save wait long enough to miss the crash-loss target.

Treat this as a candidate protocol. If full snapshots cannot meet saving and
typing targets together at the representative workloads, profile and change
the representation/scheduling before claiming success. An append/replay design
is a contingency requiring its own truncation/corruption tests, not automatic
scope for a custom storage engine.

Saving failures stay visible and preserve in-memory text. Retry reattempts the
project destination. Successful explicit Save a Copy creates a complete new
project, becomes active, and clearly changes the displayed location; the old
project remains intact. Give the copy a new project identity and preserve its
writing/imported material. Future Git identity/history handling is deferred.

For the owner's emergency-location suggestion, propose a separate local
application-data recovery location, keyed by project identity and location.
An emergency package must include enough source and imported material to reopen
without access to the failed project drive; mirror immutable dependencies while
available. Show whether the emergency attempt succeeded, what writing it
protects, and where it is. It does not silently switch projects or turn the
original destination's status to Saved. Both locations can fail; keep the error
and in-memory text in that case. The attempt and accurate status at close are
confirmed below; location, retention, and ongoing scheduling remain engineering
proposals.

After an ordinary crash, resume the latest completed valid save and last
project/cursor. Reconcile newer valid emergency/recovery snapshots explicitly;
never replace newer project content with an older local recovery. If the latest
save is damaged, preserve it and offer verified alternatives with timestamps.
Do not silently roll back. If no valid copy exists, preserve all evidence and
explain the failure rather than opening an empty project as successful recovery.

Proposed concurrency policy: one editable instance of a project on this machine.
Reopening it focuses its existing window where possible; another process with
an active lock cannot acquire edit access. Use nonblocking `QLockFile` acquisition
with time-based stale expiration disabled for a long-lived document lock. Qt's
lock coordinates cooperating processes; it cannot prevent arbitrary external
edits. [Qt 6.12 QLockFile](https://doc.qt.io/qt-6/qlockfile.html)

Detect external source replacement/deletion and stop automatic replacement of
the changed file. Preserve current memory/recovery and offer Save a Copy or
Reload after preserving current work. Compare expected file contents around
saving as well as watching filesystem events; document races with noncooperating
writers. Live simultaneous editing of the structured source outside Apollo is
not a supported milestone workflow. Test moved folders, aliases, stale locks,
and missing destinations. Follow the confirmed save-failure close behavior below.

## Editing and assistance contract

The [spec's writing rules](spec.md#writing-and-formatting) and suggestion rules
are authoritative. Implement them together with the source adapter rather than
letting the spike's provisional behavior silently decide the product.

- Enter inside dialogue continues the same speech on the next line, with no
  added blank row/repeated cue/error. Preserve explicit breaks in every output.
  An orphan dialogue element is a warning with an undoable suggested correction.
- Keep Tab to the six common types; expose every required other type through
  the element selector and shortcuts. Provide shortcut lookup and action labels.
- Completion is only for character cues/extensions and scene-heading parts.
  No autocomplete in action, dialogue, or parentheticals. Additional contexts,
  including transitions, are optional. Spelling suggestions are separate.
- Proposed menu details: open only after relevant typing so an empty element
  still permits Tab cycling; rank prefix matches by script frequency with a
  stable alphabetical tie-break. Escape suppresses reopening until the current
  component changes or the owner explicitly reopens suggestions. Enter dismisses
  and executes its normal editing action. Allow custom cue extensions as text.
- Plain paste retains the current type; internal paste retains types/emphasis.
  Define paragraph versus deliberate-line-break handling, selection replacement,
  style boundaries, and paste into cue/heading/marker elements before shipping.
  Verify one-step undo of paste without losing unrelated text.
- Manual page breaks are non-text markers. Inserting/deleting them must retain
  surrounding text and speech associations; typing must land in an adjacent
  editable element. Proposed act labels use centered text, not nonprinting
  Fountain sections; no television template is added.
- Implement dual-dialogue authoring (confirmed interaction below), preserved
  cue extensions, emphasis, and title fields, with undo. Do not turn dual cues
  into ordinary cues silently when a complete pair exists. Oversized dual groups
  need a visible diagnostic and preserved text while the profile cannot lay them
  out; this limitation must not be hidden by export agreement on small examples.
- Proposed find/replace: literal text, optional case/whole-word matching, current
  selection or screenplay scope, with one-step undo for Replace All. Regex and
  notes-body search are outside this milestone proposal. Replacing text must
  preserve surrounding types, styling, and speech structure.
- Check spelling without rewriting. Compute assistance asynchronously or in
  bounded work; discard results for obsolete text revisions. Evaluate the local
  Hunspell/dictionary integration in its slice, adding a dependency only if
  necessary and recording its version/license. The personal dictionary applies
  across all scripts on this computer; its future sync scope remains deferred.
- The outline follows edits and cursor position. Reordering moves the agreed
  whole-scene range in one undo step, preserves identities/formatting, and keeps
  caret/selection associated with the moved content. Opening preamble, title
  fields, and project-wide imported material stay separate.

Before the editing slice is complete, write a compact behavior table for
selection-aware Enter/Backspace/Delete, cue/heading splits and joins, other
empty elements, Shift+Enter/manual line breaks, suggestion-menu Shift+Tab,
and composing text. Choose routine conventions explicitly and raise any new
choice that changes writing meaning. These details must not remain accidental
Qt defaults that bypass the agreed rules.

## Import and export compatibility gate

Use a written compatibility table backed by independent examples before
declaring the parser/serializer complete. It must cover every official sample
under `fixtures/fountain-syntax/`, malformed cases, and the required formats
below. The syntax evidence is the [official Fountain specification](https://fountain.io/syntax/).
Preserving the original source bytes is useful recovery evidence, not proof
that the editable content or exported text is correct.

| Construct | Milestone obligation |
| --- | --- |
| All required screenplay elements, emphasis, title fields, extensions, dual dialogue, manual breaks | Editable import, source persistence, Fountain serialization, and PDF/editor layout |
| Literal punctuation/markup, escaped/forced elements, Unicode | Preserve meaning and text; serialize unambiguously; never silently strip/substitute |
| Manual line breaks, whitespace-only dialogue lines, intentional action spacing/indentation | Distinguish from wrapping and element separators; preserve or explicitly surface unsupported layout before affected export |
| Inline notes and omitted/boneyard passages | Convert into read-only named notes/stash, preserve text and provenance, summarize, exclude from clean screenplay exports |
| Sections/synopses/unknown title metadata | Preserve read-only organization/metadata and original source; summarize treatment, exclude from screenplay/PDF; specify canonical Fountain treatment explicitly |
| Production scene numbers or other deferred production data | Preserve as imported data and disclose unsupported behavior; do not silently strip and call output equivalent |
| Unexpected characters/glyphs | Save original Unicode; identify affected content. Proposed policy: permit faithful Fountain output, block PDF when the selected font/profile cannot render faithfully, offer explicit undoable replacement |
| Malformed/ambiguous syntax | Preserve affected source and report locations/reason; allow review/correction; never turn unparsed writing into an apparently successful clean export |

For nonprinting organization/metadata, proposed clean Fountain output omits data
Apollo has retained only as imported information and discloses that omission.
Resolve this explicitly in the compatibility table alongside notes/stash rules;
do not confuse Fountain sections with printed act labels. Known forced mixed-case
cues and imported continuation labels also need explicit handling under the
agreed uppercase defaults without duplicating or losing authored information.

For supported screenplay content, import → edit/save/reopen → export → independent
parse must retain types, text, emphasis, extensions, dual relationships, title
fields, and deliberate breaks. Fountain whitespace/marker spelling need not be
byte-identical. Keep original small expected examples as the oracle rather than
testing only Apollo's importer against Apollo's serializer.

Both exports capture a fixed writing snapshot while editing continues. Report
which snapshot completed without implying later edits were included. Title-page
inclusion must not change logical body-page numbering. Existing output files
survive failure or cancellation. Add bounded worker-shutdown/cancellation and
timeout behavior; proposed 30-second watchdog offers Cancel/Keep Waiting rather
than leaving close indefinitely blocked. Test the actual helper lifecycle.

## Implementation slices and exit checks

This table defines the slices; the progress entries below record completed work.
Each slice must leave a runnable result and reviewable changes. The complete
milestone includes all slices; an earlier slice is not the usable writing build.

| Order | Deliverable | Required evidence before proceeding |
| --- | --- | --- |
| 1 | Source schema/adapter and saving foundation, wired to the existing editor | Version/identity/span validation; exact semantic save/reopen; line-break/Qt mapping probe; interrupted replacement preserves old/new complete source; initial continuous-typing crash-loss measurements |
| 2 | New/open/import and recovery flow with simple home screen | Complete project creation, collision handling, original untouched, staged import summary, imported-material reader, project switching/resume, Retry/Save a Copy, locking/external-change and corrupted/emergency recovery scenarios |
| 3 | Complete editing and Fountain compatibility | All required authoring controls including dual dialogue, title form and manual breaks; settled keyboard/clipboard behavior; independent import/export examples; undo/redo and persistence across realistic edits |
| 4 | Everyday writing assistance and live navigation | Context-restricted suggestions with real keys; spelling/dictionary; find/replace; diagnostics; click navigation and undoable drag scene moves with exact retained content |
| 5 | Production export flow and responsiveness | Snapshot Fountain/PDF while typing; errors/cancel/shutdown preserve outputs; full independent fixtures and editor/PDF agreement; native measurements with autosave/assistance enabled |
| 6 | Local launch and owner writing handoff | Reproducible launch with packaged fonts/helper and recorded dependencies; one runner with saved logs; realistic writing/recovery/export session and owner comfort assessment |

### Slice 2 task progress

- **2.1 — Project locking and external-change detection (done 2026-10-28).**
  Editable sessions lock the canonical folder before reading and retain the lock
  through writer shutdown. Competing instances/aliases are refused without
  waiting; active locks do not expire by age and dead-process locks recover.
  Exact source/manifest bytes and file/folder identities are checked around
  replacement, with idle watching plus polling. A change pauses saving, preserves
  memory and external files, and cannot be bypassed by Retry, queued saves, or
  destruction. Retry requires the original destination to match again.
- **2.2 — Save a Copy (done 2026-10-28).** Depends on 2.1. The Writing desk
  offers Save a Copy…/Ctrl+Shift+S. A staged complete project gets a new identity
  and retains current writing and cached imported files, even with a missing
  original. Successful validation/locking switches autosave and displays the
  canonical location without resetting cursor/selection/undo. Later edits save
  there. Failure retains the current project, its paused/error state where
  applicable, and all writing; a published copy that cannot activate is kept
  with its location reported. Evidence and limits follow below.
- **2.3 — Preserved-work Reload and recovery/emergency copies (done 2026-10-28).**
  Depends on 2.1–2.2. Complete timestamped packages retain current writing and
  cached imports, with verified readback and bounded retention that preserves
  open projects, damaged evidence and the newly acknowledged copy across clock
  rollback. Reload preserves memory before explicitly adopting valid disk content.
  Failure/paused captures and close attempt emergency protection independently of
  the original. Close shows current coverage and explicit Retry/Copy/Keep Open/
  protected-close or discard choices. Emergency copies never switch destinations
  or mark the original Saved. Evidence and limits follow below.
- **2.4 — Safe project access and resume (done 2026-10-28).** Depends on
  2.1–2.3. Healthy current writing opens without rollback; damaged/missing storage
  has explicit verified, timestamped recovery selection. Newer/same-revision
  conflicts require a choice; recovery forks preserve original/package evidence.
  App-local last-project/cursor state updates only after successful activation.
  Failure/app-flow, required release/sanitizer/engine and native checks pass.
- **2.5 — Bounded home/new/open (done 2026-10-28).** Depends on
  2.1–2.4. Recent projects use identity-checked safe access; complete staged blank
  projects use the default Documents/Apollo location or a selected parent, without
  overwriting collisions. Home/leave flushes writing asynchronously; save failure
  retains the writer with Keep Writing, Retry and Save a Copy. Real controls,
  failure paths, release/sanitizer/engine checks and review pass as recorded below.
- **2.6 — Fountain import and imported-material reader (partial).** The
  compatibility investigation scoped completed 2.6a to lossless intake and read-only review;
  initial 2.6b now adds located interpretations, named read-only notes/stash,
  retained metadata and explicit new-copy migration. Full grammar coverage,
  correction/editable activation and export compatibility remain pending.
  See [compatibility table](fountain-compatibility.md).
- Remaining slice 2 work is pending: stale temporary cleanup and focusing another
  process's existing window. Archive/delete and broader library lifecycle remain
  deferred milestone boundaries as described above.

Task 2.1 evidence and review:

| Check | Result |
| --- | --- |
| Independent competing instances | `project_lock_tests`: actual editor subprocesses, symlink/relative aliases, day-old active lock, SIGKILL recovery, normal release |
| Moved/reused folder and file aliases | `source_tests`: lock follows renamed folder; old-session cleanup preserves a new project's lock; shared source file links are refused |
| External changes and memory | `autosave_tests`: in-place/atomic/identical replacement, source/manifest deletion, lost lock, replaced folder, and changes before write/before commit/after commit; Retry, queued work and shutdown preserve external bytes and current text |
| Restored destination and slow Retry | Missing-folder restoration resumes only through Retry; writing during a slow retry is captured after it completes |
| Required suites | `mise run check` 10/10, `mise run check-debug` 10/10 (ASan/UBSan; leak detection disabled), `mise run check-engine` 2/2 |
| Independent layout checks | Hand-derived PDF fixtures and local Academy agreement pass with their existing documented limits/deviations |
| Native path | Wayland `autosave_tests` 30/30; software rendering, synthetic test keys, no new typing/performance or physical-input claim |
| Representative crash regression | All 36 kills leave complete valid source; max lost writing 259 ms at 200 pages and 289 ms at 500 pages |

Contract, race boundary, commands, and saved-log locations are in
[source-format.md](source-format.md#editable-sessions-and-destination-changes-slice-2-task-21).
Direct implementation review covered code and tests. The initial idle/during-save
checks failed on the old saver. Review exposed a missed-capture bug while Retry
was paused: a focused failing check reproduced it, and the fix now saves later
writing without another key. No remaining in-scope finding was identified.

The final compare/rename interval still races with noncooperating external
writers; live simultaneous external editing is unsupported. This task does not
claim durable recovery of memory-only writing after a destination change.
Save a Copy is now completed by 2.2 below. Reload, emergency copies and the full
failure-close choices were pending at that checkpoint; task 2.3 below replaces
the interim two-close discard.

Task 2.2 evidence and review:

| Check | Result |
| --- | --- |
| Copy and activation | Real Writing desk controls copy memory-only writing after an external change; a new UUID, exact retained elements/ids, changed location/PDF default, selection, cursor and undo/redo are checked |
| Missing original/imports | Moved original still copies cached imported-material/original Fountain bytes; source tests distinguish empty dependencies and reject unreadable/symlinked imports |
| Ordered saving/lifecycle | In-flight old save finishes; queued old writes are superseded; parent aliases resolve canonically; edits during a slow copy save after activation; close waits; destruction drains later writing |
| Failure | Existing/nonempty/empty destinations, symlinks, missing/read-only parents, relative paths, paused originals and copies changed/locked before activation keep the current project/writing; external files remain intact |
| Required suites | Release 10/10; ASan/UBSan 10/10 with leak detection disabled; engine-only 2/2. Final source/autosave sanitizer recheck passes 72/72 and 42/42 after review/test extensions |
| Native path | Final Wayland autosave suite 42/42, with fcitx and software rendering; final focused offscreen selection/cancellation/alias checks also pass |

Direct review covered requirements, implementation, concurrency and test
necessity; no remaining in-scope finding was identified. The first copy check
failed on the original saver. A focused failing regression then reproduced
loss of later writing on destruction during a copy; the fix and ordered-save
checks now pass. The collision form and active-location screenshots were
inspected. Logs, exact native setup, limitations and the copy contract are in
[source-format.md](source-format.md#save-a-copy-slice-2-task-22).

At the 2.2 checkpoint, crash-loss/latency during copying, emergency backup,
damaged-save recovery and full failure-close flow remained unverified or pending.
Task 2.3 below adds the recovery/close behavior; copying measurements remain
unverified. An ordinary autosave crash regression passed in the required suites;
the earlier representative crash-loss measurements were not repeated here.

Task 2.3 evidence and review:

| Check | Result |
| --- | --- |
| Recovery packages and retention | `recovery_tests` 7/7: complete cached imports with the original moved, hashes/corrupt evidence, real storage failure, open-package retention, clock rollback, and independently specified minute retention |
| Explicit Reload | Actual Writing desk confirmation controls; preserved memory copy; exact adopted elements/ids; undo reset; continued autosave; malformed source/preservation failure retains memory/undo and external bytes; later document changes cancel adoption and are protected |
| Failure/close | Accurate current versus older emergency coverage; original failure visible while emergency I/O is blocked; both destinations failing; Keep Open; Retry completes closing; explicit protected/unprotected close; repeated close never discards |
| Session baseline | `source_tests` 73/73; reading a Reload candidate never authorizes replacement; a stale candidate is refused before adoption |
| Required suites | Serial release 11/11, ASan/UBSan 11/11 (`detect_leaks=0`), engine-only 2/2; after final status-notification fixes, release/debug autosave suites each pass 48/48 |
| Independent layout | Hand-derived fixtures in required suites; local Academy comparison passes with its existing documented deviations |
| Native path | Full Wayland autosave suite 48/48, then final copy/Reload/close controls 7/7; fcitx, software rendering and synthetic input |
| Isolated representative crashes | 20 kills per workload, including snapshot phases and random timing; all 40 source files and published recovery packages validate independently; maximum loss 473 ms at 200 pages and 712 ms at 500 pages |
| Recovery reopen | A published package from each workload opens through the real editor's `--check`; writing/evidence bytes remain identical and edit locks release |

Direct review covered requirements, concurrency, lifecycle, implementation and
test necessity. The initial recovery tests failed before implementation. Review
fixed premature pruning of open/current packages, delayed original-error display
during slow emergency I/O, failure-close Retry, batched document replacement,
and stale status notifications. Final relevant checks pass; no remaining
in-scope correctness finding was identified. Reload and protected/unprotected
close screenshots were inspected. The contract, commands, timing details and
saved logs are in [source-format.md](source-format.md#preserved-work-reload-and-recoveryemergency-copies-slice-2-task-23).

A further pre-commit review reproduced two visible-status regressions before
fixing them: typing after emergency protection left the QML status stale, and a
Reload notice hid subsequent PDF export results. Both now have focused QML
regression checks. Retention also now takes the package edit lock, revalidates
its integrity and atomically unpublishes it before deletion, closing races with
opening or editing an older recovery project. The corrupt-evidence test now
also exercises later retention. The opener/pruner interleavings were reviewed
in code; the tests exercise open-package retention and preservation of changed
evidence without forcing every concurrent interleaving.
Final pre-commit checks pass: release and ASan/UBSan 11/11 each (leak detection
disabled), engine-only 2/2, release/debug autosave 50/50 each, and native Wayland
autosave 50/50 using the existing fcitx/software setup. Logs are
`build/recovery-review-{red,green,release,debug,engine,native}.log`. The earlier
representative workload measurements were retained, without a new timing claim.

The isolated runs meet the target for the exercised healthy-storage conditions.
A separate small-script crash run concurrent with sanitizer work lost 1.86 s of
recent writing while the primary commit stalled about 1.74 s; files remained
valid. Retain that limit rather than promising a universal bound under I/O
contention. Leak detection, crash loss during Save a Copy, power loss and loss
bounds with unavailable storage remain unverified. At the 2.3 checkpoint,
damaged-save selection/reconciliation were pending; task 2.4 below implements
them without automatic rollback.

### Task 2.4 evidence and review

Safe open/recovery decisions, the real chooser controls, app-local resume and
executable dispatch have focused failure and app-flow coverage. Final release
and ASan/UBSan checks each pass 15/15 (`detect_leaks=0`); engine-only passes 2/2;
the independent Academy comparison passes with its existing deviations.
Native Wayland preflight passes input/
compose, export lifecycle, agreement and OpenGL first paint. Final native
project-access controls pass 11/11. A real executable/wtype/SIGKILL restart
preserves writing and resumes the independently checked insertion position.

Independent source/integration reviews exposed and fixed stale damaged-original
decisions, hidden emergency alternatives at a non-directory original path, and
a dangling alias becoming a locked project while the chooser was open. Focused
checks failed before those fixes and pass after. Review also strengthened the
process restart oracle to require a fresh cursor capture rather than accepting
the preceding process's state. No remaining in-scope finding was identified.

Pre-commit review also reproduced a dangling original alias whose target
returned as a regular file, not a directory. Recovery now revalidates both link
and followed-target state. The focused source suite passes 46/46; both target
return cases retain the original and recovery evidence.

The complete contract, commands, evidence/log paths, native harness limits and
unmeasured cases are in
[source-format.md](source-format.md#safe-project-access-and-resume-slice-2-task-24).
At this checkpoint home/new/open and full switching were pending; task 2.5 below
adds them. Fountain import/reader, focus-existing-process IPC and stale-stage
cleanup remain separate slice 2 work.
No new representative timing, physical-input latency, power-loss or copying-loss
claim is added; task 2.3's I/O-contention limitation remains.

### Task 2.5 evidence and review

The bounded home/new/open workflow is implemented without Fountain import.
The working contract is in
[source-format.md](source-format.md#bounded-homenewopen-slice-2-task-25).
Recent entries are app-local navigation hints, not authoritative screenplay
storage. No-argument last-project/cursor resume is retained; Home is accessible
from the writer with Ctrl+Shift+H.

| Exercised check | Result |
| --- | --- |
| Blank staged creation | `project_creation_tests` 21/21: complete editable projects, unique identities, file/empty-folder/nonempty-folder/dangling-link collisions, parent aliases, invalid names/parents, actual permissions and stage cleanup |
| Recent list | `project_library_tests` 25/25: MRU order/cap, aliases/moves/replacements, cross-instance merge/lock contention, malformed/newer/nonregular evidence, source independence and real atomic-write failure |
| Real QML behavior | `project_access_flow_tests` 19/19 in release, ASan/UBSan and native Wayland: create/open/recent controls, identity refusal, lossless switching, retained undo, Retry/Copy/Keep Writing, both save destinations failing, corrupt recents and recovery |
| Departure concurrency | A held real exporter exposes destination loss after the initial save drain; failure retains the writer. A gated real recovery operation leaves the GUI event loop responsive, retains writing and waits for export before Home |
| Required checks | `mise run check` 17/17; `mise run check-debug` 17/17 with `ASAN_OPTIONS=detect_leaks=0`; `mise run check-engine` 2/2 |
| Independent layout | Academy comparison passes with the same four documented deviations; required suites retain independent PDF fixtures and editor/PDF agreement |
| Actual native executable | Isolated Home → real folder chooser → selected-location creation → writing → Home; missing default Documents/Apollo creation; restart/resume; explicit opening; Tab/Space recent selection and folder-dialog cancellation. Independently read saved JSON retains the exact typed writing |

Direct and independent reviews covered the new storage helpers, UI, lifecycle
and test necessity. The first Home-transition checks failed before implementation.
Review found an unchecked interval while waiting for PDF export, and project
lock deletion still occurring on the GUI thread. A focused held-exporter
regression failed before the correction. Final validation and session/lock
teardown now run on the saver worker after export and app-state work finish;
failure returns to the unchanged live writer. Review follow-up found no remaining
in-scope correctness issue. Native exploration also exposed missing recent-entry
Tab focus; explicit focus policy plus keyboard activation now have a regression
check. The existing editor suite caught a null-saver QML action binding; it is
guarded without changing the retained unsaved-file/instrumentation paths.

After the final ownership/merge cleanup, native and ASan/UBSan flow suites again
pass 19/19, and release/debug recent-list suites pass 25/25 each. This includes
the keyboard recent-selection regression and preserved malformed-list evidence.

Logs: `build/task-2.5-{release,debug,engine,academy}.log`,
`build/task-2.5-native-flow.log`, `build/task-2.5-debug-flow.log`,
`build/task-2.5-debug-creation.log`, `build/task-2.5-library.log`, and
`build/task-2.5-debug-library.log`. Native executable commands/environment,
screenshots and exact retained writing are recorded in
`build/task-2.5-native/results.txt`. QML screenshots are in
`build/release/project-access-tests/` (`home.png`, `new-project.png`,
`leave-failure.png` plus existing recovery surfaces).

Remaining limits: Fountain import/reader is task 2.6, not implemented here.
Focus-existing-window IPC, stale-stage cleanup and archive/delete/library
lifecycle remain outside this bounded task. Recent history is capped at 20;
only the last project's cursor is persisted. A publication-time collision is
fail-safe but is not deterministically injected in the new helper suite.
Failure checks use temporary filesystem changes/permissions and a stopped real
export worker, not a physical drive disconnect or a measured slow lock unlink.
Native keys are synthetic, software-rendered in the QML suite; executable
screenshots do not establish physical-input latency, extended comfort or normal
compositor-close behavior. Leak detection, power loss, unavailable-storage loss
bounds, crash loss during Save a Copy and new representative performance remain
unverified. Task 2.3's measured I/O-contention limitation is unchanged.

Finalization review re-inspected the complete uncommitted implementation and
tests; no remaining in-scope finding was identified. A fresh release check
passes 17/17 (`build/task-2.5-final-release-cache.log`). Its first invocation
could not write uv's default cache under the sandbox; `UV_CACHE_DIR=/tmp/apollo-uv-cache`
resolved that environmental failure. The prior sanitizer, engine and native
results above apply to the same implementation; no new timing claim is made.

### Task 2.6a evidence and review

Commit finalization re-reviewed the complete intake changes on 2026-10-28.
Fresh release and ASan/UBSan suites pass 19/19, engine-only 2/2, and the three
focused native Wayland import/reopen/cancel/recovery cases pass (5/5 including
suite setup/cleanup). Logs: `build/task-2.6a-commit-{release,debug,engine,native}.log`.
The first sandboxed attempts could not use uv's cache or LeakSanitizer process
inspection; rerunning with native process/cache access passed, including default
leak detection in this run. No new latency, physical-input or power-loss claim
is added. Direct review found no further in-scope defect.

The compatibility investigation found task 2.6 too large for one reviewable
change. [fountain-compatibility.md](fountain-compatibility.md) records every
checked-in official sample, independent original semantic expectations,
selected canonical export treatment and the remaining gates. The completed
first subtask is **lossless intake and review**, not editable import. It retains
all source as pending conversion; no converted note/stash or clean screenplay
export is claimed. The on-disk/UI contract is in
[source-format.md](source-format.md#lossless-fountain-intake-and-review-slice-2-task-26a).

| Check | Result |
| --- | --- |
| Intake/storage | `fountain_intake_tests` 32/32: byte-exact capture/staged reopen for all 16 official samples plus the independent example; empty/BOM/CRLF/Unicode/control/invalid-encoding bytes, located and bounded hints, nonregular/unreadable/oversize input, actual short-write rollback, typed metadata/hash/version validation, downgrade/hidden-writing refusal, copy/recovery and mode-changing Reload refusal |
| Actual QML import/reopen | `project_access_flow_tests` 24/24 in final release, sanitizer and native Wayland runs: review/publish controls, read-only plain text and note access, missing-parent Retry, collisions, external source changed after preview, retained-original reopen/resume, writer switching, cancellation, damaged-import recovery and competing reader lock |
| Publication/activation | A controlled real worker publication followed by replacement of both valid import dependencies (same UUID) is refused before activation; retained evidence/path and unchanged session/recents are checked |
| Executable workflow | `fountain_intake_process_tests`: real `--retain-fountain` → explicit reopen → SIGKILL → no-argument resume; Python independently checks SHA-256, length and all evidence bytes; editing/layout/PDF instrumentation and existing-destination publication refuse the intake |
| Required suites | Final release `mise run check` 19/19; `mise run check-debug` 19/19 (`ASAN_OPTIONS=detect_leaks=0`), followed by final rebuilt related sanitizer suites 10/10 after the last activation safeguard; engine-only 2/2 |
| Independent layout | Academy comparison passes with the same four documented deviations; required checks retain hand-derived PDF fixtures and editor/PDF agreement |
| Native path | `tools/check-wayland.sh --preflight` passes compositor/fcitx input (55 editor checks), export lifecycle (13), project flows, independent export checks and OpenGL first paint. Final rebuilt project-flow suite passes 24/24 using software rendering and synthetic Qt keys |

Direct implementation review covered staging, integrity, original-byte
preservation, version compatibility, worker ownership, safe access/recovery,
output refusal and test necessity. It removed a race between UI access to the
reader session and worker teardown during departure, refused mode-changing
Reload into a writer, and
added comparison against the reviewed bytes before activating a published
intake. Final checks pass; no remaining in-scope finding was identified. The
preview/reader screenshots were visually inspected. The first intake test run
had invalid test fixtures missing required source IDs; its QtTest exception
caused SIGABRT, confirmed in coredump metadata. Corrected fixtures pass; this
was not an application import crash. Repeated per-sample save-refusal checks
were consolidated into one focused contract check.

Logs: `build/task-2.6-final-release.log`, `build/task-2.6-debug.log`,
`build/task-2.6-debug-final-related.log`, `build/task-2.6-engine.log`,
`build/task-2.6-academy.log`, `build/task-2.6-native-preflight.log`, and
`build/task-2.6-native-flow-final.log`. Native preflight artifacts/results are
in `build/wayland-check-20261028-154102-SQSGlu/`; actual executable logs are in
`build/{release,debug}/intake-process-tests/`. UI captures are
`build/release/project-access-tests/import-{preview,reader}.png`.
The sandbox's uv cache needs `UV_CACHE_DIR=/tmp/apollo-uv-cache`; no tool/font
pins or dependencies were changed.

At the **2.6a checkpoint**, remaining **2.6** work was semantic conversion and precise unresolved ranges,
named read-only converted notes/stash and metadata associations, migration from
review-only intake, correction/editing access, output-specific diagnostics,
canonical Fountain serialization and independent edited semantic round-trip.
Slice 3's deliberate breaks, case/CONT'D, whitespace and complete authoring/layout
contracts remain necessary. The reader has no Save a Copy UI or persisted
selection/scroll. Its hints are not exhaustive syntax recognition, and invalid
encoding is retained as bytes rather than guessed. Native file/folder chooser
acceptance was not newly exercised for import; the native tests entered paths
directly through the real controls.
No deterministic simultaneous input-file mutation during its read, kill during
staging/publication, new representative import performance, physical-input
latency, leak detection or power-loss guarantee is established. Existing
copying/crash-loss and I/O-contention limits remain unchanged.

### Initial task 2.6b evidence and review

Implemented two bounded subtasks: **2.6b.1 located interpretation model** and
**2.6b.2 explicit interpretation-copy migration/reader**. This begins 2.6b;
it does not close the complete editable Fountain compatibility gate. The
contract and remaining work are in
[fountain-compatibility.md](fountain-compatibility.md#conversion-revision-1-and-migration)
and [source-format.md](source-format.md#located-interpretation-copies-initial-task-26b).

The model retains immutable original bytes and evidence, complete located
screenplay interpretations, deterministic named read-only note/stash entries,
section hierarchy, synopsis/scene-number associations and unknown title fields.
Precise unresolved ranges are separate from rendering/authoring limitations.
An explicit new staged project copy with a fresh identity is the only migration;
the previous review-only project is never rewritten. Publication failure retains
the original reader and lock. Activation verifies the published interpretation
against the reviewed snapshot. Metadata v1 remains readable, metadata v2 embeds
the frozen revision-1 interpretation, and older/newer/altered data cannot silently
gain editable access. Both clean exports and writer access remain blocked.

| Check | Result |
| --- | --- |
| Independent semantics | `fountain_conversion_tests` 33/33: complete type/text/dual expectations for all 16 official samples plus the independent original example; explicit unresolved expectations for the legacy heading spelling and lyric/speech relationship |
| Located preservation | Exact note/stash interior text and CRLF, named records/anchors, unknown metadata, section hierarchy including skipped levels, scene numbers, Unicode/non-BMP styles and independent byte/code-point expectations, deliberate/empty dialogue lines, optional/multiline title fields |
| Ambiguity and bounds | Paragraph-bounded unclosed notes, nested/orphan/escaped delimiters, unclosed stash, controls/invalid UTF-8, unequal/crossing/intraword emphasis, unpaired/space-disambiguated duals, empty cues, malformed production numbers and bare unknown title keys; stored data survives the independent UI cap and conversion limit |
| Integrity/migration | Original project/assets remain unchanged; new identity and complete verified reopen; read-only save refusal; recovery package retains interpretation; changed ranges, records, screenplay, output permissions, unknown fields and newer revisions are refused |
| Real app/executable | Project-flow suite 26/26; interpretation-copy controls exercise failed publication, retained original lock, successful activation/resume, plain note text, unresolved access and changed-original refusal. Actual executable creates/reopens/SIGKILL-resumes both metadata versions; Python independently checks bytes, hash, content, identity and blocked editing/PDF paths |
| Required suites | Full release 20/20, ASan/UBSan 20/20 with default leak detection, engine-only 2/2. Following final review corrections, rebuilt related release checks pass 9/9 and sanitizer checks 8/8; the latter cover source/recovery/creation/access, both Fountain suites, actual intake processes and real QML migration |
| Native/independent layout | Native Wayland project flows 26/26, with final focused migration/refusal cases 4/4 including setup/cleanup. Academy comparison passes with its four existing documented deviations; required full suites retain the independent layout/PDF fixtures |

Direct review covered the converter against the independent expectations, source
offsets and normalized text, stored schema/integrity, original preservation,
copy/failure/activation ownership and test necessity. It corrected skipped-level
section parenting and tightened unsupported emphasis/dual interpretations. Reader
state is cached from worker validation, avoiding metadata JSON parsing on GUI
property reads; moving session ownership on the GUI thread avoids a reader/
worker teardown race. A final focused regression reproduced an empty forced cue
incorrectly located at byte zero; it now points to its actual source row. The
regression fails before the correction and passes afterward. No remaining
concrete in-scope finding was identified. The interpretation reader screenshot
was visually inspected.

Logs: `build/task-2.6b-{release,debug,engine,native,academy}.log`,
`build/task-2.6b-final-{related,debug-related,native}.log`, and
`build/task-2.6b-range-red.log`. The intermediate final-release rerun was
interrupted when the agent rebuilt the executable during a crash-harness launch
(`PermissionError`); the completed-binary crash rerun and final related suites
pass. That interruption was a verification orchestration error, not a failed
save. Process artifacts are in `build/{release,debug}/intake-process-tests/`;
the reader capture is `build/release/project-access-tests/import-interpreted.png`.

Remaining: fuller grammar/emphasis and boundary-interaction coverage, explicit
correction/resolution UI, editable preservation of imported associations, slice
3 authoring/layout/font/case/CONT'D contracts, canonical Fountain serialization,
independent import → edit → save/reopen → export semantics, and output-specific
release gates. No imported writer access is claimed, even when unresolved count
is zero. Revision 1 interpretation is limited to 1 MiB / 100,000 source lines;
larger retained intakes get a whole-source unresolved range. No new realistic
import performance, physical-input latency, power-loss, staging-kill or
simultaneous-source-mutation guarantee is established. Native controls use
synthetic Qt input and software rendering, not a new comfort/latency acceptance.

### Slice 1 result (done 2026-10-28)

The implemented contract and measurements are in [source-format.md](source-format.md);
it supersedes the proposals above where they differ. A project folder opens
in `apollo_editor` with autosave (`apollo_editor DIR --create [FILE.fountain]`
created one); at that checkpoint a Fountain path opened unsaved. Task 2.6a
now routes ordinary Fountain opening to the review preview; explicit spike
instrumentation retains its limited parser.

| Required evidence | Result |
| --- | --- |
| Version/identity/span validation | `source_tests`: hand-written byte-exact fixture, 36 malformed or newer-version cases, project folder and staging checks |
| Exact semantic save/reopen | `autosave_tests`: fixtures, Unicode, line breaks, title fields, and real editor edits reopen as the same elements, ids, and layout |
| Line-break/Qt mapping probe | `line_separator_probe`: U+2028 inside the block works for cursor, selection, and undo, and Qt always breaks after it. Chosen: `\n` in source, U+2028 in the block |
| Interrupted replacement | Injected failures and SIGKILL at every save phase leave the complete old or new file |
| Initial crash-loss measurements | Max lost writing 258 ms (200 pages) and 337 ms (500 pages) over 18 kills each; offscreen typing p95 unchanged |

At slice 1 completion, moved to later slices rather than claimed: the engine,
PDF, Fountain, and keyboard handling of deliberate breaks, and the page-break-text
limitation
(slice 3); recovery snapshots, the emergency copy and full close dialog,
damaged-save recovery, Save a Copy, locking, external-change detection,
resuming the last project/cursor, and stale temporary-file cleanup (slice 2);
native typing and save measurements with autosave (slice 5). Locking and
external-change detection are now completed by task 2.1 above; Save a Copy is
completed by task 2.2.

Review corrections: clipboard copy/cut and Apollo paste now use the source's
line-break offsets and retain internal breaks/emphasis; Qt-created block ids
are repaired with the originating undo command instead of during autosave;
the writer rejects revisions beyond version 1's maximum before replacing the
last valid file. Regression checks cover Unicode selections, cut/paste,
IME split → type → save → undo/save → redo, and revision exhaustion/retry.

Retain tool/font pins from the spike brief and use existing mise/uv commands.
Record upgrades rather than changing dependencies as a planning side effect.
Do not add a second general document/undo engine or implement Git to satisfy
recovery. Review source changes for later history compatibility without claiming
that branch/shared-material semantics are solved.

## Verification and milestone acceptance

Extend the smallest existing tests where practical and add focused persistence,
compatibility, and app-flow checks for distinct risks. Use original expected
documents and failure outcomes rather than reproducing implementation logic.
Implementation evidence is recorded with each completed task; requirements
below remain the milestone acceptance gate.

- Run `mise run check`, `mise run check-debug`, and `mise run check-engine` for
  the implemented changes. Preserve the unchanged hand-derived fixture checks,
  measured geometry/font requirements, and local Academy comparison with only
  its documented deviations. Missing references must be reported.
- Repeated subprocess crashes during continuous timestamped edits on the
  primary and stress workloads must recover a complete valid source. Verify
  the one-second bound against an independent input log, including kills during
  capture, write, replacement, and after acknowledgment. Do not count a manual
  Save before termination as proof of autosave.
- Inject permission, short-write, full-disk, commit, missing-folder, corrupted
  source, and emergency-location failures. Do not fill the real filesystem;
  use controlled fault injection or a disposable bounded filesystem. Verify
  saved status, prior bytes, available recovery copies, and in-memory writing.
- Exercise create → write → reopen and import → review → edit → reopen → export
  through the actual app. Check original input bytes and retained imported
  content independently. Use small original samples for CI; the existing local
  `reference/own/Crossroads.fountain` and PDF are optional private compatibility
  inputs, never copied into committed fixtures or modified.
- Verify real key/mouse behaviors across page gaps and dual columns, selected
  replacements, deliberate breaks, type changes, scene movement, spelling
  replacement, Replace All, and undo/redo. Completion menus must never appear
  in excluded element types and must not accept on Enter.
- Re-run `tools/check-wayland.sh` and adapt its measurements to include saves,
  spelling/suggestions, scene navigation, and export while typing. Retain the
  primary targets: key-to-frame p95 ≤33 ms, no stall >100 ms, warm open ≤1 s,
  PDF export ≤5 s, and PSS ≤250 MiB on this machine. Record 500-page stress
  results separately; no claim that it meets the primary typing budget.
- Add save age/latency, cold-launch observations, memory during sustained
  sessions, and output correctness after scripted edits. Keep virtual input,
  compositor/physical-input latency, sampled/peak memory, and subjective
  comfort distinct in the report.
- Before the owner handoff, run the native path directly where possible and
  provide one executable runner with prerequisites, safe disposable project
  copies, checkpoints, failure handling, and saved output. The owner assesses
  appearance/comfort; automated success does not replace that acceptance.

No fallback-stack decision or large optimization is justified by a hypothetical
regression. Profile actual failures; keep the spike's explicit reassessment
criteria. A focused interaction prototype is appropriate only if an early
source/Qt or authoring risk cannot be resolved with a small real-app check.

## Final owner decisions and handoff

The owner accepted these choices in planning round 23:

1. **Dual-dialogue authoring:** provide a selector/menu action and shortcut on
   the second speech to pair it with the immediately preceding speech. Turning
   it off restores sequential dialogue without changing text. Reject ambiguous
   pairing across action/scenes rather than guessing.
2. **Personal dictionary:** words added to the personal dictionary apply across
   all the owner's scripts on this computer. Store it locally at app level;
   GitHub sync scope remains later design. Character-name suggestions continue
   to come only from the current screenplay.
3. **Close during a saving failure:** attempt the emergency copy, show whether
   current writing is protected, and offer Retry, Save a Copy, or Keep Open.
   If current writing remains unprotected and the owner chooses to close,
   require an explicit discard choice describing what is not protected; never
   trap the app in an unresponsive infinite wait. A current successful emergency
   copy must not be described as lost writing or as a successful original-project
   save.

The requirements interview is complete. Complete the detailed compatibility/
keyboard tables and schema contract in the relevant early slices, checking them
against these requirements. Technical validation and tests remain work to do;
completion of the plan is not completion of those checks. Raise any newly
discovered consequential product question rather than silently settling it in
code. Current implementation progress and the remaining scope are recorded above.

Finalization recheck: all 20 release checks pass across
`build/task-2.6b-commit-release.log` and `build/task-2.6b-commit-retry.log`.
The first run could not write uv’s default cache inside this sandbox; the six
affected Python checks passed with `UV_CACHE_DIR=/tmp/apollo-uv-cache`.
The previously recorded sanitizer, engine, native and Academy evidence remains
applicable to this unchanged checkpoint. Direct final review found no additional
concrete in-scope defect.

### Task 2.6c — bounded editable preservation

Implemented the explicitly requested next bounded step under
[editable-fountain-subset.md](editable-fountain-subset.md). The independent
fixture covers every saved screenplay element type, six title fields, emphasis,
Unicode é, literal punctuation, deliberate breaks and dual speeches alongside
named note/stash, organization, scene-number and title-metadata records. Admission
is conservative: at most 64 KiB, 256 elements and 256 retained records; no
unresolved syntax, unverified characters/case/spacing or layout failure.

The explicit new-copy flow uses a v3 manifest and separately validated immutable
working metadata. Existing reviews stay read-only. Current writing is source v1;
imported records retain historical IDs/provenance and show present, detached or
type-changed associations. The reader stays accessible from the writer, including
after Save a Copy. Immutable dependency changes pause saving and cannot be adopted
by Reload. At this step clean Fountain and PDF exports stayed blocked in writers,
copies, recovery forks and executable snapshot paths; task 2.6d below enables
contract-checked Fountain export, and PDF remains blocked.

The smallest editing/layout work makes LF a real engine break and makes Enter
inside dialogue preserve one speech element. Empty dialogue parenthetical Enter
also follows the agreed return-to-dialogue rule. Existing element identities
support the bounded historical association contract without adding text tracking.
Remaining work includes general grammar/corrections, mixed case/spacing/font
contracts, full authoring/title/dual controls, canonical Fountain output with an
independent exported semantic round-trip, and representative performance.

Evidence and direct review are recorded in the
[subset verification](editable-fountain-subset.md#verification-and-review).
Full release, ASan/UBSan with leak detection and engine-only checks pass, with
independent Academy/layout references retained. Final activation and association
regressions also pass in related sanitizer and native Wayland flows. This closes
the bounded preservation step, not full task 2.6 or slice 3 compatibility.

### Task 2.6d — bounded canonical Fountain export

Implemented as requested under [fountain-export.md](fountain-export.md), contract
`fountain-canonical-1`. Ordinary projects and 2.6c editable import copies
export clean Fountain when the snapshot captured by the request meets the
writer-subset content rules (shared code with admission), speech/dual structure,
unambiguous spelling and spellable emphasis. Eligibility is re-evaluated for
every snapshot, so edits after import can block and later permit export. Each
refusal names the element, type, text and reason; nothing is exported lossily.

The serializer's output is read back by the frozen revision-1 interpreter before
the worker replaces the destination through `QSaveFile`; refusals and failures
leave existing files intact, and the project folder and import original are
protected. The snapshot is fixed at request while typing, autosave and undo
continue; status names the capture time. Imported notes, stash, organization,
scene numbers, unknown title fields and whitespace-only source lines are omitted
and counted in the panel, status and CLI. Home/close wait for an accepted export.
The Writing desk export panel gains a PDF/Fountain choice, Ctrl+Shift+E, and an
eligibility/disclosure text; `apollo_editor PROJECT --export-fountain OUT
[--no-title-page]` exports saved writing through the same job. PDF export of
imported writing stays blocked.

| Area | Evidence |
| --- | --- |
| Serializer | `fountain_export_tests` 62/62: hand-written canonical text for the full subset model with/without title page, 23 spelling and 30 located-refusal cases, per-snapshot eligibility, omission counts, size bound, and a 40,000-snapshot explanation-coverage property |
| Real controls | `project_access_flow_tests` (32 total): import → edit copy → disclosed export equal to hand-written text; real-key edits that block and recover; export held on its worker while typing and autosave continue, later words excluded; PDF blocked; reopen + no-title export; failed write and refused destinations keep files; ordinary project export with Home waiting |
| Executables | `fountain_export_process_tests`: real typing with autosave makes a new process block (trailing space, existing file kept) then export exact expected bytes after one more key; disclosure, no-title body, project/original/review-only refusals |
| Independent parser | `fountain_export_oracle_tests`: aw-parser 0.1.1 reads 5 exports/expected files as hand-worked semantic models (types, text, emphasis, dual, title fields, breaks); lyric-as-italic-action is the explicit representation difference; mutations are detected |
| Required suites | Release 23/23, ASan/UBSan 23/23 with no sanitizer report, engine-only 2/2. Native Wayland preflight passes: editor 56/56, export lifecycle 13/13, project flows 32/32 (both new Fountain tests natively), first paint and PDF export while typing |

Exploratory fuzzing (2.4 million random snapshots) found four refusals that only
the read-back guard caught; each now has an explicit reason and a regression row.
Three hand-written unit expectations were corrected after the first run because
they had misapplied the written forcing rule (action starting with emphasis
markup is `!`-forced); the fixture files under `fixtures/export/` were unchanged. Measured GUI
check/worker times are recorded in the contract; representative full-length
scripts mostly fall outside the subset and were not measured.

Logs: `build/task-fountain-export-{release,debug,engine,native}.log`; native
artifacts are under `build/wayland-check-20261028-175021-KkLE8e/`. Panel captures
are `build/release/project-access-tests/fountain-export-{ready,blocked}.png`.
The Academy comparison was not rerun: the engine, profile and PDF path are
unchanged. Remaining gates are listed in the contract; this closes the bounded
export step, not slice 3 or slice 5 compatibility.

### Layout fix — spaces after a deliberate break

2026-10-28, reported during 2.6d. Pressing Enter before a space in dialogue
(`At the| same time.`) made the Writing desk footer show `QTextLayout did not
break block N where the engine did`.

**Cause.** The engine wraps each authored line's visible text, and a line never
starts with a space, so the line after the break starts at its first letter.
The editor ended each QTextLayout line where the engine's next line began. That
end included the break *and* the spaces, but QTextLayout always breaks right
after the line separator, so the two disagreed.

**Effect, as reproduced.** Text, caret and slots on screen were already right:
QTextLayout's own break put the space at the start of the next line, and the
existing lead offset drew the visible text at the dialogue column. The visible
effect was the red footer error. On such writing, `apollo_editor --check`,
`--agreement` and `--snapshot` failed, as did any check that requires no layout
error. Pagination and PDF were unaffected because both come from the engine,
working on the element snapshot (`pdf_export` does not link the editor layout).
The fix leaves them unchanged.

**Fix.** `ScreenplayLayout::relayout` ends a line just after the last authored
break before the next engine line, so the skipped spaces start the next
QTextLayout line before its first visible character. The spaces stay in the
writing and are saved. The caret can reach them just left of the column
([profile note](export-profile.md#editor-states-provisional-spike-rules)). Whether such spaces should be
shown, or should count toward width, was left to the case and spacing tasks;
task 3b below keeps them unshown and lets Fountain export write such lines.

| Check | Independent expectation |
| --- | --- |
| `editor_tests` `enter_before_space_keeps_engine_breaks` | Real Enter in fixture 01: second line starts after the break, "I always leave the lights on." at 216 pt on slot 10, caret on the space at 216 − 7.2 pt, later elements at fixture 01's slots, no stray ink; typing either side of the space keeps the mapping |
| `project_access_flow_tests` `deliberate_break_before_space_saves_and_reopens_without_layout_error` (+ an assertion in the 2.6d flow where it was reported) | Writing desk with real keys: no footer error, autosaved text keeps `\n `, reopened layout places the line at 216 pt on slot 3 |
| `editor_pdf_tests` `break_before_space` | Saved project with `\n  ` and a wrapped sentence: `--check`, editor/PDF agreement, and the PDF against hand-derived `fixtures/editing/break-before-space.expected` (speech gains one line; later elements move down one slot) |

Each new check failed without the fix, with the original message. Release 23/23,
ASan/UBSan 23/23 with no sanitizer report, and engine-only 2/2 pass. The native
Wayland preflight passes: editor 57/57 with OpenGL and fcitx5 input, export
lifecycle 13/13, project flows 33/33, native PDF snapshots and first paint.
Editor/PDF agreement on the local Academy transcription (0 and 50 edits) and
`compare_academy.py` pass; the layout fixtures are unchanged. Logs:
`build/task-break-space-{release,debug,engine,academy}.log`; native logs are in
`build/wayland-check-20261028-182633-uXLbC2/`.

### Task 3a — bounded case handling for headings and cues

2026-10-28. The [case contract](case-handling.md) makes the confirmed rule
explicit. The project keeps authored text, and scene headings, character cues and
the title are presented in capitals by converting ASCII letters only. One
definition, `presented_text()`, is used by the engine and PDF, the editor's
drawn capitals, the opening scene list and the line dump. Clean Fountain
(`fountain-canonical-2`) writes headings and cues as presented and discloses
lowercase authoring. New import copies (`writer-subset-2`) admit lowercase
headings and cue extensions with authored text and original bytes kept. Forced
mixed-case names, letters that cannot be shown in capitals (é) and lowercase
titles remain blocked with located reasons. `writer-subset-1` copies are still
validated by their own rules.

The owner accepted its three choices as the initial case policy (planning round
24): export presented capitals with disclosure while the project keeps authored
case; keep forced mixed-case names blocked until a faithful exception is
designed; present imported lowercase headings and cue extensions in capitals.
The evidence table and remaining limitations are in the contract.

Checks: release 23/23, ASan/UBSan 23/23 with no sanitizer report (the editor
suite was rerun under sanitizers after its last change), engine-only 2/2. The
native Wayland preflight passes: editor 58/58 with OpenGL and fcitx5 input,
export lifecycle 13/13, project flows 34/34, native PDF snapshots including the
lowercase case, and first paint. Editor/PDF agreement on the local Academy
transcription (0 and 50 edits) and `compare_academy.py` pass; layout fixtures
are unchanged. Logs: `build/task-case-{release,debug,debug-editor,engine,native,academy}.log`;
native artifacts are in `build/wayland-check-20261028-190734-M1Zis8/`.
A typed lowercase `(cont'd)` meeting a generated continuation was then tested
in the editor (`typed_continuation_is_shown_once`, lowercase row) and the PDF
(`authored-lowercase-contd` against fixture 08's hand-derived page). It exposed
no defect; removing the ASCII-apostrophe suffix match makes both fail with a
doubled `MARA (CONT'D) (CONT’D)`. Logs:
`build/task-case-commit-{release,debug,engine}.log`.

### Task 3b — bounded spacing and deliberate breaks

2026-10-28. The [spacing contract](spacing.md) defines authored versus presented
spacing. Spaces at the edges of an authored line are kept but not shown, counted
or written to Fountain (title fields excepted); repeated spaces inside a line are
shown as typed; an empty or spaces-only line inside dialogue is one blank slot,
written to Fountain as a two-space line. Profile rule B9 now treats a deliberate
break after a sentence end as a split point, and a split never puts an authored
blank line or blank speech part beside the page break. That fixes two defects
found while probing: a space before a break could start a continued page with a
printed blank line and raise the editor's QTextLayout error, and without the
space the same sentence end was no split point at all. Clean Fountain becomes
`fountain-canonical-3`, with a spacing disclosure; new import copies use
`writer-subset-3`, which admits dialogue edge spaces, blank lines inside
dialogue and repeated spaces in action and dialogue. Action indentation, blank
first/last dialogue lines, blank action lines, tabs and vertical spacing between
elements stay refused or review-only. No creation control or advisory for
unusual spacing was added; that remains an owner decision. Four engineering
choices await owner review; they and the evidence table are in the contract.

The case policy's three choices were also confirmed by the owner (planning round
24) and committed separately before this task.


Checks: release 23/23, ASan/UBSan 23/23 with no sanitizer report, engine-only
2/2. The native Wayland preflight passes: editor 62/62 with OpenGL and fcitx5
input, export lifecycle 13/13, project flows 35/35, and native PDF snapshots
including the new spacing expectations. Editor/PDF agreement on the local
Academy transcription (0 and 50 edits) and `compare_academy.py` pass with
unchanged results; the layout fixtures are unchanged. Each new layout check
fails on the previous engine. Logs: `build/task-spacing-{release,debug,engine,academy,native}.log`;
native artifacts are in `build/wayland-check-20261028-195348-YusiiS/`.

### Representative full-length workflow — bounded punctuation step

2026-10-28. Assessment used disposable private copies and committed independent
fixtures. The owner's 100,514-byte / 2,319-element reference has 9
character-blocked elements, 2 unresolved empty speeches and 49 extra-vertical-
spacing locations, and exceeds current editable size/element bounds. The second
148,439-byte / 2,759-element reference has 771 character-blocked elements plus
spacing, title, tabs and size gates. Counts, measurement boundaries and
remaining work are in the
[compatibility assessment](fountain-compatibility.md#representative-full-length-assessment-2026-10-28).
No private wording was used as test data.

The bounded improvement preserves **— ‘ ’ “ ” …** as authored Unicode in new
`writer-subset-4` copies and `fountain-canonical-4` output, removing 9 and 768
character-blocked elements respectively. Older contracts and the frozen
revision-1 interpretation remain unchanged. Independent original fixtures,
real import/edit/undo/autosave/reopen/export flows and aw-parser verify the
expanded repertoire. Size, unresolved syntax, unsupported spacing/Unicode,
title and imported PDF gates remain; neither reference becomes editable yet.
This does not complete task 2.6 or slices 3/5.

`tools/check-spacing.sh` is the short native owner check. It creates only an
isolated synthetic ordinary project, prints the four requested checkpoints and
keeps work/logs in ignored build output. Its `--smoke` mode checks the native
editor and existing real-controls disclosure/persistence/export behavior.
No spacing choice is promoted to an owner-approved policy.

Final verification: release 23/23, ASan/UBSan 23/23 without a sanitizer report,
engine-only 2/2; native preflight editor 62/62, export lifecycle 13/13, project
flows 36/36 plus native PDF snapshots/first paint; spacing runner smoke and
Academy independent comparison pass. Full native typing was not rerun.
Detailed evidence paths, measurement boundaries, review corrections and
remaining gates are in the compatibility assessment above.

### Full-length continuation — explicit spacing and empty-speech choices

The subsequent [workflow continuation](fountain-compatibility.md#full-length-workflow-continuation)
distinguishes 49 two-empty-line scene separators from two separate
cue/parenthetical-only speeches. `writer-subset-5` retains every interpreted
element and original byte, admits unfinished empty speeches with a visible
diagnostic and one-step undoable type-only correction, and requires an explicit
unchecked import choice before applying normal scene spacing. Every omitted
extra line remains located in imported material and disclosed in Fountain
output as non-equivalent spacing. Other spacing/syntax gates and imported PDF
blocking stay intact.

Working metadata v2 records the spacing choice; older copy contracts remain
readable under their own rules. Admission is bounded to 128 KiB / 3,072 elements
/ 256 retained records; the continuation records measurements and their limits.
The current milestone is still incomplete: this does not implement arbitrary
spacing, full Fountain grammar, the title form, general diagnostics, spelling,
suggestions, find/replace or independently correct imported PDF.

Use `tools/measure-fountain-workflow.sh --source FILE --output NEW_DIRECTORY
[--keys N]` for the isolated disposable workflow measurement, native Wayland by
default; `--offscreen` is runtime verification only. It deliberately applies
measurement-only empty-speech corrections to a copy, never records author
acceptance, verifies exact save/reopen/re-export and the unchanged input, and
keeps potentially private screenshots/projects/logs under a mode-0700 output.
Only aggregate JSON is printed. Measured outcomes and remaining gates are
recorded in the continuation above.

### Owner-reference writing readiness — bounded step

2026-10-29. **Done for this bounded readiness step**, not slice 3/4 authorization
or completion of task 2.6. [Planning round 25](planning.md#round-25--owner-reference-writing-readiness-settled-2026-10-28)
records the owner-confirmed acceptance of explicit non-equivalent normalization
of 49 scene gaps, exact import of both unfinished speeches for manual resolution,
and the unchanged 250 MiB primary-workload PSS target. The prior rehearsal's
type corrections are not acceptance of either speech's resolution.

The actual QML import admitted the reference at the existing bound only after
selecting the initially unchecked spacing choice. The private writable copy
retains the byte-exact original and both speeches unchanged. Native real-key
typing, accurate Saved acknowledgment, normal close, separate-process reopen,
both live issue selections and refused Fountain/PDF requests passed. All
rehearsal text was removed through the writer; final authored text, types,
styles, IDs and title fields match the initial imported writing.

Three fresh ordinary-application processes per workload, one project each,
120 `wtype` keys per run and external requested-50-ms PSS sampling, with no
in-application comparison snapshots or reopen, stayed below 250 MiB. Sampled
maxima: **120.0 MiB** for the standard nominal 200-page primary (203 displayed),
**169.4 MiB** for the 91-page import. Separate writer-ready/settled/post-autosave
numbers, method and limits are in the
[compatibility readiness record](fountain-compatibility.md#owner-reference-writing-readiness-2026-10-29).
No application or contract change, new fixture, speculative optimization or
stack reassessment was needed.

Final checks: release **23/23**, ASan/UBSan **23/23** without a sanitizer report,
engine-only **2/2**. Native preflight passes editor **71/71**, export lifecycle
**13/13**, project flows **37/37**, native PDF snapshots and first paint.
The independent synthetic spacing copy passed both speech corrections,
Undo/Redo, typing/autosave/reopen and byte-identical re-export; aw-parser
independently matched both actual exports to the complete saved writing.
Editor/PDF agreement includes both generated workloads and the local Academy
transcription at 0/50 scripted edits; the independent Academy comparison passes
with its four unchanged deviations. Full native typing/export timing was not
rerun because no editor or layout code changed.

Initial configure failures came from ignored CMake caches pointing at a deleted
temporary Ninja installation. Repairing only those cached paths to the installed
pinned Ninja allowed all final checks above to pass. Logs and bounded-method
limits are in the compatibility readiness record. Direct completion review
found no additional in-scope defect; no new permanent tests were needed for
this unchanged-behavior rehearsal.

Private command, both source-line/issue-list locations, logs and artifacts:
`build/owner-readiness-20261029/handoff.txt`. The entire readiness directory is
mode 0700 and ignored; only its named aggregate JSON is publishable. The owner
must still resolve both speeches before Fountain export; saving already works.
Arbitrary spacing, tabs/NBSP, the second reference's title gate and imported PDF
remain blocked. Extended writing/comfort, full compatibility and the remaining
writing-milestone features stay open.

### Task 3c — emphasis authoring

2026-10-29. **Status: done for bounded task 3c; required release, sanitizer,
engine and full native checks pass, including the final dialogue fixes.**
This does not complete slice 3, task 2.6 or the milestone, and is not owner
comfort/appearance acceptance.

#### Interaction contract

The owner explicitly chose **require a selection**. In the writer,
**Ctrl+B / Ctrl+I / Ctrl+U** and **Format → Bold / Italic / Underline** invoke
the same `EditorCommands` action. A fully covered selection loses only that
style; a mixed selection gains it throughout. One undo step covers all selected
elements, without changing words, types, identities or the selection. Authored
line breaks and element separators receive no style. Bare-caret shortcuts and
separators-only selections are no-ops; the menu disables actions without a
selection. There is no next-typing mode.

The [source contract](source-format.md#emphasis-authoring-slice-3-task-3c) defines
the preservation and boundary details. Existing emphasis remains editable in
headings/cues without changing their capital presentation. Typing within a
styled run, split/join, type changes, internal clipboard, undo/redo,
autosave/reopen and editable imported copies preserve it. Generated `(MORE)`,
repeated page-continuation cues and generated `(CONT’D)` stay plain.
The latter fixes actual pre-existing style leakage from repeated cues and
replaced typed suffixes; source emphasis is retained, not discarded.

`ScreenplayEditor` consumes the three shortcuts before Qt and is explicitly
plain-text. A Qt 6.12 offscreen baseline found **no default character-format
mutation** from these shortcuts on the previous plain-text surface; the new
route nevertheless prevents a Qt rich-text path from bypassing the command.

#### Independent evidence and review

The original public `emphasis-authoring` fixtures provide hand-written initial
and authored documents, exact canonical Fountain and an eight-row PDF layout.
Real QML controls apply emphasis in both ordinary projects and new editable
import copies; the independent aw-parser oracle reads the actual authored
export and a fresh-process re-export. Editor/PDF checks export the actual
command-authored ordinary project, check its independent geometry, exact
bold/italic text and rasterized underline, including plain generated CONT'D.

Before implementation, the three partial-toggle keyboard rows and multi-element
case fail. New engine regressions compiled against the original engine also
fail on a styled repeated cue and styled replaced suffixes. Focused corrected
checks pass. Further cases cover mixed/overlapping ranges, UTF-8/UTF-16 offsets,
separator-only no-ops, reversed selection, caret-menu dismissal, typing,
split/join, type changes and styled versus plain clipboard paste.

A final preservation review also reproduced two dialogue-boundary defects:
Enter inside emphasis styled the break itself, making the otherwise supported
speech unexportable, and toggling off a legacy span crossing a break left an
invisible styled separator. The fix inserts a plain break, derives continued-line
typing's style from the following text (also after reopen), and clears legacy
separator emphasis on toggle-off. Both regressions fail before the fix and
pass after it; hand-written Fountain bytes protect the Enter/typing result.
No import/export eligibility rule or version changed.

Completion review found and corrected two verification issues: Poppler's merged
mixed-face XML carries the first run's font ID, so exact style checks use its
nested style tags; the native global-key runner now selects the Hyprland instance
for the prepared socket and checks its own child's compositor focus before
every `wtype` batch. No private desktop screenshot or clipboard capture is used.

Final verification:

| Check | Observed result |
| --- | --- |
| `mise run check` | **23/23**, including actual authored Fountain export and independent aw-parser; final run is stage 1 of the full native runner |
| `mise run check-debug` | **23/23**, ASan/UBSan, no sanitizer report |
| `mise run check-engine` | **2/2**, without Qt/PDF |
| Full `tools/check-wayland.sh` | All six stages pass: native editor **80/80**, export lifecycle **13/13**, project flows **38/38**, compose/dead-key input, first paint, PDF snapshots, typing and export while typing |
| New real-key native smoke | `wtype` selects text and sends Ctrl+B/I/U; exact styled JSON, toggle-off, Undo/Redo, bare-caret plain typing, autosave and inverse-style checks after fresh-process reopen pass |
| Independent layout | Existing hand-derived fixtures unchanged; editor/PDF agreement on 203-/507-page workloads at 0/50 edits and local Academy transcription passes; all 11 Academy body pages pass their independent anchored comparison with the four existing deviations |
| Actual surface | Format menu and authored writer captures inspected; no owner visual acceptance inferred |

The full runner also refreshed the existing native Wayland/threaded-OpenGL
instrumentation, **200 synthetic keys per workload per mode**:

| Observed sample | Primary (203 displayed pages) | Stress (507 displayed pages) |
| --- | ---: | ---: |
| Typing key-to-frame p95 / max | 17.7 / 18.7 ms | 42.8 / 45.5 ms |
| Export-while-typing key-to-frame p95 / max | 22.9 / 25.6 ms | 52.4 / 59.1 ms |
| Warm open to first frame, typing run | 289.7 ms | 530.8 ms |
| Actual PDF requests / maximum completion time | 50 / 348.9 ms | 34 / 672.5 ms |
| Sampled editor-plus-helper PSS maximum, export mode | 117.5 MiB | 174.7 MiB |

Neither mode observed a >100 ms stall or missing frame. The primary samples
meet the exercised timing/memory targets; stress results are recorded separately,
not held to the primary's 33 ms p95. These are the existing unsaved workload
`--measure` / `--measure-export` paths, not performance measurements with
autosave or later writing assistance. Real-key autosave/reopen was verified
separately on the small isolated project. Memory is a requested-20-ms sampled
maximum, not an unsampled peak; no cold-start or extended-session claim follows.

Final logs: `build/task-3c/{debug-final,engine-final,native-final,pdf-focused,academy}.log`.
Native artifacts/results are in
`build/wayland-check-20261029-024031-xku0mi/`; its `native-emphasis/` output
is mode 0700, isolated from the owner's application state and retains the
per-step exact autosave evidence. `build/task-3c/release.log` is the earlier
failed PDF-oracle attempt, not final evidence. Independent reviews have no
remaining findings after the input-focus guard correction. Direct completion
review covered the later dialogue fixes and their two failing-before/passing-after
regressions; full release, sanitizer and native runs were repeated afterward.
Temporary original-engine probe source/binary were removed. No dependency or
font pins changed.

#### Limits and retained gates

`writer-subset-5`, `fountain-canonical-4`, source v1 and all admission/layout
limits remain unchanged. Arbitrary selections can create styles outside
Fountain's existing spellable subset; writing still saves and clean export
reports the located reason. Imported PDF, arbitrary spacing, tabs/NBSP,
forced mixed-case names and the other reference's title gate remain blocked.
The owner's private projects/reference material are untouched.

Dual authoring, title form, manual page-break/act controls, suggestions,
spelling, find/replace and scene navigation/reordering remain separate work.
Native virtual input is not physical-input latency or owner comfort/appearance
acceptance. No new crash-loss, copying-loss or power-loss guarantee follows.

### Task 3d — structured title-page form

2026-10-29. **Done for bounded task 3d**, including final verification. The form
is available in editable ordinary projects and admitted editable import copies.
This does not finish slice 3, full import/export compatibility, or the writing
milestone.

**Title page… / Ctrl+Shift+T** opens six labeled groups: title, writing credit,
authors, source/based-on credit, contact information and draft date. Fields
support keyboard navigation, visible focus, plain text, deliberate breaks and
separate repeated entries. **Apply / Ctrl+Enter** makes one document undo step;
**Cancel / Escape** discards the form draft. Autosave/recovery/reopen and both
export jobs use that same applied document; later form changes cannot enter an
accepted export snapshot. Unused absent fields remain absent; clearing an
existing entry preserves its empty text/identity until explicitly removed.
Unchanged emphasis and body history survive. The desk title follows Apply,
Undo/Redo and clearing rather than retaining its opening-only value.

The [source contract](source-format.md#structured-title-page-form-slice-3-task-3d)
records editing boundaries. [Title placement](export-profile.md#title-page-proposal-from-the-sample)
now supports deliberate/blank lines and repeated fields on the existing fixed
anchors, with independent original expectations. Off-page or overlapping title
text reports a layout/PDF failure instead of cropped success. A body-only PDF
removes title fields before layout, so a title-only failure does not block it.
Both formats retain the shared **Include title page** choice and logical body
numbering.

**No gate was widened:** source v1, `writer-subset-5` and
`fountain-canonical-4` are unchanged. Multiline, blank, over-48-character or
lowercase-title values remain saveable but outside the included-title Fountain
subset; omitting the title bypasses only its checks. Imported PDF stays blocked
even when the title is omitted. Immutable imported originals and unsupported
metadata/notes/stash remain accessible and unchanged. Owner-private material and
`prompt.md` are not part of this change.

Independent expectations cover all six fields, repeated authors/contact,
Unicode, styles, blank lines, title omission and two logical body pages.
The new engine checks fail when compiled against the pre-task engine.
Real form controls cover Apply/Cancel, one-step undo/redo, autosave/reopen,
held-worker snapshot isolation, Fountain refusal/file preservation, shared
omission choice, PDF layout failure/old-file preservation, and the separate
imported-PDF gate. The actual authored Fountain exports and fresh-process
re-export are also read by aw-parser; shared-engine agreement is not the oracle.

Native real-key checks are repeatable through `tools/check-wayland.sh`, which
now runs `tests/title_page_native_smoke.py` using only an isolated synthetic
ordinary project and its own focused child. The exact saved field values,
Tab focus/scroll, Apply/Cancel, whole-title Undo/Redo, autosave, fresh-process
editing, actual PDF content and exact Fountain are observed. No desktop-wide
screenshot or clipboard capture is used by that title smoke.

#### Verification and review

| Check | Observed result |
| --- | --- |
| Release (`mise run check`, full native runner stage 1) | **23/23**; repeat-run fixture setup corrected before the final pass |
| Sanitizers (`mise run check-debug`) | **23/23**, ASan/UBSan with no sanitizer report; final behavior-only PDF outcome assertions also pass in the focused sanitizer run |
| Engine-only (`mise run check-engine`) | **2/2**, without Qt/PDF |
| Full `tools/check-wayland.sh` | All six stages pass: native editor **84/84**, export lifecycle **13/13**, project flows **40/40**, compose/dead-key input, first paint, PDF snapshots, typing and export while typing |
| Real compositor input | Title smoke passes all six fields, keyboard traversal/scroll, Apply/Cancel, whole-title Undo/Redo, autosave, fresh-process edit/undo, PDF content and exact Fountain |
| Independent export checks | Hand-derived multiline-title geometry/styles and form-authored PDF content pass; aw-parser independently reads **24** exported/expected cases, including three actual title-authoring exports |
| Independent Academy reference | All 11 anchored body pages and full body text sequence pass with the four unchanged documented deviations |
| Actual surface | Open/populated native form captures inspected; no owner appearance/comfort acceptance inferred |

Fresh native Wayland/threaded-OpenGL instrumentation, 200 synthetic keys per
workload per mode:

| Observed sample | Primary (203 displayed pages) | Stress (507 displayed pages) |
| --- | ---: | ---: |
| Typing key-to-frame p95 / max | 18.0 / 19.3 ms | 43.1 / 44.6 ms |
| Export-while-typing key-to-frame p95 / max | 22.7 / 25.2 ms | 53.1 / 62.9 ms |
| Warm open to first frame, typing run | 300.7 ms | 528.9 ms |
| Actual PDF requests / maximum completion time | 50 / 341.6 ms | 34 / 691.7 ms |
| Sampled editor-plus-helper PSS maximum, export mode | 117.6 MiB | 178.1 MiB |

No >100 ms stall or missing frame was observed. The primary samples meet the
exercised spike targets. These are the existing unsaved `--measure` /
`--measure-export` workload paths, not timings with autosave or later assistance.
The title form's real-key autosave/reopen is a separate small-project check.
PSS is a requested-20-ms sampled maximum, not an unsampled peak.

Direct requirement/test review corrected the desk's stale opening-only title
and added title/undo/clear assertions. Repeating the suite exposed a new
PDF fixture setup bug: it tried to create its existing disposable project.
Setup now removes only that test's named project, and final repeated checks
pass. The original engine fails the new multiline/repeat/overlap checks; its
temporary probe source/binary were removed. Existing public fixtures, private
material, dependencies and font pins are unchanged.
The final test-quality pass uses actual PDF completion outcomes and preserved
file bytes rather than pinning error-message wording. Both title flows pass
again offscreen, on native Wayland and under sanitizers after that refinement.
Direct completion review has no remaining in-scope finding.

Native evidence: `build/wayland-check-20261029-031948-oVrSgx/`, including
`native-title-page/evidence.json` and `results.txt`;
`build/task-title-native-final.log` contains the final release/full native run.
Engine/Academy logs: `build/task-title-{engine,academy}.log`. Form captures:
`build/release/project-access-tests/title-form{,-open}.png`.
Sanitizer evidence: `build/task-title-debug-final.log` and
`build/task-title-debug-focused.log`.
The earlier `build/task-title-native.log` records the fixture-reuse failure,
not final evidence.

#### Remaining limits

Fixed title anchors are not an automatic composition or wrapping system:
overlong/overlapping fields need writer adjustment or title omission. Included
title Fountain eligibility, imported-PDF blocking, full Fountain grammar and
the remaining writing features retain their earlier gates. Extended comfort,
owner appearance acceptance and screen-reader interaction were not exercised.
Native virtual input is not physical-input latency. No new crash-loss,
copying-loss or power-loss guarantee follows from these title checks.

### Manual page-break authoring — bounded slice 3

2026-10-29. The Writing desk gains **Page break → Insert / Remove**,
**Ctrl+Enter / Ctrl+Shift+Enter**, and a nonprinting **Remove** control in each
manual-break page gap. This authorizes/completes only this bounded capability,
not act labels, dual-dialogue authoring, general paste, assistance or the rest
of slice 3.

The owner explicitly chose **before the whole element or speech** rather than
splitting words, and **refuse selected insertion while preserving selection**.
An adjacent dual pair is one group. A safe final end inserts after the group;
with no following writing it creates a plain empty action in the same undo
step. See the [boundary behavior table](source-format.md#manual-page-break-authoring-slice-3).
Existing words, types, emphasis, identities and dual flags stay unchanged.
Removal deletes only a marker, without joining its neighboring elements.
Selections crossing markers are refused for replacement, Enter, cut, paste
and deletion; a marker-only separator selection can remove that marker.
Potentially meaning-changing orphan-speech/dual attachment is explicitly refused.

The existing source model and autosave adapter are unchanged. Markers survive
undo/redo, autosave and reopen with their identities. Leading, consecutive and
trailing markers retain empty pages and body numbering; title pages do not
enter that count. Editor gap labels never enter PDF or Fountain. Legacy text
inside a marker remains visible/saveable and causes a PDF/Fountain refusal,
not hidden content loss. Imported PDF and every existing Fountain eligibility
gate remain in force.

#### Verification design and review

The initial real-key regression fails on the previous authoring path:
Ctrl+Enter splits action into another ordinary action instead of creating a
marker. Corrected focused tests cover whole-element text/styles/IDs, whole
speech/dual placement, safe boundary deletion, leading/consecutive/trailing
and marker-only bodies, empty elements, reversed selections, type/paste/IME
redirection, one-step undo and title-only removal destinations.

The original public `page-break-authoring` fixtures pair an unbroken model
with hand-derived title/body slots, columns, emphasis and numbering and
hand-written canonical Fountain. Real QML controls author the two markers,
save/reopen and export both formats. Poppler reads the actual PDF, and
aw-parser reads actual/fresh-process/reimported Fountain. Independent edge
cases retain leading/consecutive/trailing and marker-only Fountain.
The actual PDF worker also refuses legacy marker text while retaining the
existing PDF and exact source bytes.

Review caught a real instrumentation integration failure: `--snapshot` used
select-all replacement across protected markers. It now explicitly removes
safe markers through the same command as the gap control before replacing
text, with the entire live change grouped as one undo step. Its restoration
assertion now includes full elements, styles and identities, not only text.
The accepted PDF must still match the original independent fixture after
typing, undo/redo and live repagination.

Further review-driven corrections guard Ctrl+K's native delete-to-end-of-line
route and IME replacement offsets after selection removal, classify plain
paste from its actual redirected type, and create body writing rather than
append to a title when the last body marker is removed. A marker-only IME
composition check exposed lingering preedit ink; the command now clears Qt's
old transient preedit before moving/committing. The gap-control check edits
preceding text first, then clicks the updated control and checks undo.
Focused corrected checks pass; no scope-expanding compatibility change was
needed.

The final source-contract edge check also reproduces a wrong destination when
title fields are interleaved with body blocks. Marker insertion/removal now
skips those fields, reuses existing body writing, and creates a plain body
action only when needed. A failing-before/passing-after regression protects
these cases without changing ordinary title editing.

#### Final verification

| Check | Observed result |
| --- | --- |
| Release (`mise run check`, final native runner stage 1) | **24/24** |
| Sanitizers (`mise run check-debug`) | **24/24**, ASan/UBSan without a sanitizer report |
| Engine-only (`mise run check-engine`) | **2/2**, without Qt/PDF |
| Full `tools/check-wayland.sh`, final code state | All six stages pass: native editor **95/95**, export lifecycle **13/13**, project flows **41/41**, compose/dead-key input, first paint, PDF snapshots, typing and PDF export while typing |
| Focused page-break layout | **9/9**: independent slots, number geometry, source mapping, title/body and empty-page cases, legacy retention/refusal |
| Compositor-delivered authoring | Real `wtype` insertion/removal, safe Backspace, adjacent typing, exact IDs, Undo/Redo, autosave, fresh-process reopen, canonical Fountain and independently read PDF pages/numbers |
| Independent exports | Poppler checks actual authored PDF, frozen snapshots and title omission against hand-derived expectations; leading/consecutive/trailing empty PDF pages retain numbering; aw-parser 0.1.1 independently reads **30** exports/expected cases, including authored markers, bounded reimport and marker-only/edge cases |
| Academy reference | All **11** anchored body pages and the full **245-element** text sequence pass with the four unchanged documented deviations |
| Actual surface | Native page-gap label/Remove and numbered-page capture inspected; no owner appearance/comfort acceptance inferred |

Final native Wayland/threaded-OpenGL samples, 200 synthetic keys per workload
per mode, with no concurrent sanitizer run:

| Observed sample | Primary (203 displayed pages) | Stress (507 displayed pages) |
| --- | ---: | ---: |
| Typing key-to-frame p95 / max | 18.7 / 22.5 ms | 45.6 / 48.1 ms |
| Export-while-typing key-to-frame p95 / max | 24.0 / 27.3 ms | 54.3 / 60.9 ms |
| Warm open to first frame, typing run | 294.7 ms | 533.8 ms |
| Actual PDF requests / maximum completion time | 49 / 491.0 ms | 35 / 711.3 ms |
| Sampled editor-plus-helper PSS maximum, export mode | 116.6 MiB | 175.0 MiB |

No >100 ms stall or missing frame was observed. These are the existing unsaved
`--measure` / `--measure-export` workloads, not measurements with autosave or
later assistance. The real-key marker/autosave/reopen check is a separate small
ordinary project. PSS is a requested-20-ms sampled maximum, not an unsampled peak.
Final review has no remaining in-scope finding. No source-schema, import/export
contract revision, dependency or font pin changed.

Evidence: `build/task-page-break-debug-final.log`,
`build/task-page-break-engine.log`, `build/task-page-break-academy.log` and
`build/task-page-break-native-final.log`. Final native artifacts:
`build/wayland-check-20261029-044018-YHaBlu/`, including `results.txt` and
`native-page-break/evidence.json`. The native editor capture is
`build/release/editor-tests/manual-page-break.png`. Earlier
`build/task-page-break-{release,debug}.log` retain the snapshot-instrumentation
failure, not final verification.

#### Retained limits

Apollo-to-Apollo paste containing a marker is explicitly refused for this
bounded step; use the page-break command rather than silently detaching a
speech. Mixed marker/text selections must remove markers explicitly before
replacement or deletion. Legacy text-bearing markers are retained for review,
not automatically converted into writing. A new empty action remains valid
source but keeps the existing clean-Fountain empty-element refusal until
written or explicitly removed.

Act labels, dual authoring, full Fountain grammar and independently correct
imported PDF remain separate work. Automated native input is not physical-key
latency or owner appearance/comfort acceptance. No new crash-loss, copying-loss
or power-loss guarantee follows from these checks.


### Dual-dialogue authoring — bounded slice 3

2026-10-29. This task implements the confirmed second-speech interaction and
the owner's selected complete-speeches/explicit-unpairing policy. See
[dual-dialogue.md](dual-dialogue.md) for all boundary cases, verification and
remaining limits. The blank-row source-mapping regression is independently
reproduced against the pre-task engine. Existing tests that deleted a boundary
beside a pair now explicitly unpair first. Snapshot instrumentation likewise
unpairs through real commands before its destructive live edit and restores
all original words, types, styles, identities and flags in one Undo.

The pre-existing uncommitted multiline-title additions to `tests/layout_tests.cpp`
are preserved outside this task's commit. `prompt.md` and private references are
unchanged. No later milestone slice is authorized by this task.

Final verification: release **24/24**, address/undefined/leak sanitizer
**24/24**, engine **2/2**; native preflight passed **100 editor / 13 export /
43 project-flow** checks without skips, fcitx/compositor input, all four wtype
smoke checks and native workload editor/PDF agreement/rendering. The new
independent Fountain/PDF fixture passes, including fresh-process output and
retained imported-PDF refusal. See the [final evidence and limits](dual-dialogue.md#final-check-results--2026-10-29).
Sustained typing performance and owner appearance/comfort acceptance were not
re-evaluated. This completes only the bounded dual-authoring task.


### Task 3e — element-type controls

2026-11-04. **Done for this bounded step**, with the evidence and limits below.
The task follows the completed dual-dialogue step with the remaining body-type
controls from the [editing contract](#editing-and-assistance-contract). See
[element-types.md](element-types.md) for the interaction table and evidence limits.

The Writing desk now shows the caret's current element type and offers all eight
body text types through a selector and Ctrl+1–8. Lyrics and centered act labels
are directly authorable. Each choice changes one whole element in one undo step,
retains text/breaks, emphasis, identity and local selection, and refuses a
cross-element selection. Title fields and page-break markers use their dedicated
controls; paired speeches still require explicit unpairing. The six-type Tab
cycle remains, and the toolbar wraps beside the expanded outline. No source,
import-converter or Fountain-contract version changed.

Meaningful checks first failed against the missing controls, then passed through
the real menu/keys. Existing stale-title validation still tests atomic refusal
when another adapter changes a field's type; Tab now protects title fields.
Original synthetic fixtures provide hand-written canonical Fountain and
hand-derived PDF slots/columns, checked by Poppler and independent aw-parser,
along with exact saved IDs/styles and fresh-process reopen/reimport. Final review
reproduced a checkmark becoming unchecked when choosing the active type. The
menu now restores its live type binding after unchanged/refused commands;
focused checks fail before that fix and pass afterward. All four affected
editor/project-flow/process/oracle checks pass again in release and sanitizer
builds, and native menu/no-op/protection checks pass again.

| Check | Result |
| --- | --- |
| Isolated `mise run check` | 24/24 passed |
| `mise run check-debug` | 24/24 passed with address/undefined/leak sanitizers |
| `mise run check-engine` | 2/2 passed |
| Affected checks after final menu fix | 4/4 release and 4/4 sanitizer; native menu/protection passed |
| Independent Fountain process/oracle | 36 exports matched hand-worked semantic models |
| Native Wayland/OpenGL editor, Compose input plugin | 108 passed, three optional input checks skipped; scene shortcut passed separately |
| Native project flows | 44 passed, no skips |
| Native wtype element-type smoke | All eight shortcuts, lyric/act labels, typing, Tab, Undo/Redo, IDs, autosave/reopen and independent exports passed |
| Direct review, synthetic-window image, shell syntax and whitespace | Passed; no remaining in-scope findings |

Logs are in `build/task-3e-*`; final native smoke evidence is under
`build/task-3e-native-elements-verified/`. The system lacks fcitx5, so full fcitx
preflight was not run; the default Wayland input context produced focus warnings
on rapid test-window replacement, while the explicit Compose run passed. The
wtype test executable was extracted locally without changing system packages.
A concurrent sanitizer workload reproduced the known heavy-I/O crash-loss limit
(1,146 ms against the unchanged 1,000 ms target); the complete isolated release
suite passes without that competing workload. Owner comfort, sustained typing
performance, general selection/clipboard rules, standalone shortcut lookup,
broader Fountain compatibility,
imported PDF and later milestone slices remain separate work.

### Task 3f — selection and clipboard behavior

2026-11-04. **Done for this bounded step**, with the evidence and limits below.
The [behavior table](selection-and-clipboard.md)
records the delegated Enter/deletion, selection, composing and clipboard
conventions. This completes only the selected bounded task;
shortcut reference, assistance and broader interchange remain separate work.

External paste preserves the current element type, Unicode, spaces, blank
paragraphs and trailing breaks (tabs become one space). Dialogue paste keeps
all lines inside one speech. Other body types retain paragraphs and U+2028
breaks, with surviving destination halves kept separately. Apollo MIME retains
selected types/emphasis/dual flags without copying identities. Empty clipboard
paste preserves the selection and undo history. Middle-button paste uses the
same rules at its clicked location. Existing marker/dual refusals remain;
title-field boundary joins and replacements now refuse with an explanation.

The first real-key checks failed on type/whitespace preservation, empty-paste
selection retention and title-boundary replacement. New tests protect the
original failures, reversed replacement through six real edit routes, exact
unselected styles/identities and one-step Undo/Redo. The original synthetic
clipboard fixture exercises plain paste plus Apollo MIME, autosave/reopen,
fresh-process Fountain output, hand-derived PDF slots/columns/styles and
independent aw-parser. No source-schema, import/export version, font pin or
dependency changed, and imported PDF remains gated.

Preserved U+2028 exposed a parenthetical layout defect: the first separator
was counted twice when wrapping the hanging continuation, causing invalid
source mapping or an invalid Qt line access. Hand-worked engine examples fail
on the original implementation and pass with the correction, including hidden
leading spaces and blank/trailing lines. The editor also checks line validity
before accessing it and retains its existing visible fallback/error behavior.

Review reproduced an IME paragraph path around the title guard and corrected
it, while keeping marker-only deletion beside a title available. The existing
snapshot instrumentation's select-all mutation then hit the new title guard;
it now removes titles through the actual title command, within the same undo
step as unpairing/marker removal and typing. Exact elements/styles/IDs restore
on Undo, and the already accepted PDF snapshot still matches independent
expectations. The earlier concurrent-relink test failure and subsequent title
snapshot failure are retained in the earlier logs, not counted as final evidence.

| Check | Observed result |
| --- | --- |
| Isolated `mise run check` | **24/24** passed |
| `mise run check-debug` | **24/24** passed with address/undefined sanitizers |
| `mise run check-engine` | **2/2** passed |
| Final paired-dialogue paste assertion | Focused real-key check passes again in release and sanitizer builds after the last assertion addition |
| Native Wayland editor, Compose input plugin, OpenGL | **131 passed**, four optional compositor/fcitx checks skipped in this run; primary-selection paste passes |
| Native project flows | **45 passed**, no skips; software renderer, exact clipboard writing/styles/IDs, autosave/reopen and export checks included |
| Native compositor keys | `wtype` Ctrl+V replaces reversed selection with multiline dialogue; exact styles/IDs and one-step Undo/Redo pass separately |
| Independent exports | Poppler checks pasted text, blank-line slots, columns and italic suffixes; aw-parser 0.1.1 independently reads **39 exports** as hand-worked models |
| Direct completion review | Requirements, changed code, untracked fixtures, test necessity and whitespace checked; no remaining in-scope finding |
| Additional Academy regression comparison | **Unavailable:** `reference/academy/expected.tsv` and the local reference PDF are absent; both ordinary and browser-user-agent downloads of the public guide returned HTTP 403 |

Final logs: `build/task-3f-release-verified.log`,
`build/task-3f-debug.log`, `build/task-3f-engine.log`,
`build/task-3f-native-{editor,flows,keys}.log` and
`build/task-3f-paired-paste-{release,debug}.log`. The original regression failures
are retained in `build/task-3f-red.log` and `build/task-3f-engine-red.log`;
the latter reproduces the original invalid parenthetical source mapping.
`build/task-3f-academy.log` and `build/task-3f-reference-download*.log` record
the missing-reference/download limitation. Earlier release logs contain the
concurrent relink or title-snapshot failures, not final evidence.

The native input context is explicitly `QT_IM_MODULE=compose`; fcitx5 is not
installed, so the standard full fcitx runner was not run. The compositor check
uses the already extracted `build/native-tools/usr/bin/wtype`. Native virtual
keys are not physical-key latency. Owner comfort/appearance, sustained typing
performance, crash-loss during copying and power-loss behavior were not newly
assessed; the existing heavy-I/O crash-loss limit remains. Full Fountain grammar,
imported PDF and later milestone work retain their gates. The extra Academy
comparison remains unverified here; the independent synthetic fixtures passed.

At the 3f checkpoint, the next ready bounded work was shortcut reference and
action labels; task 3g below implements that step. Remaining Fountain
compatibility and slice 4 assistance/navigation are pending.

### Task 3g — shortcut reference and action labels

2026-11-04. **Done for this bounded task**, with the evidence and limits below.
This bounded task implements the [shortcut discoverability requirement](spec.md#writing-and-formatting)
and [editing plan](#editing-and-assistance-contract). Keybinding customization
remains unspecified; assistance, scene navigation and broader interchange are
separate work.

The rail's **Shortcuts** button and **F1** open a modal, scrollable reference in
the Writing desk. It covers Undo/Redo, clipboard, Enter/Tab, all eight types,
selected-text emphasis, dual dialogue, page breaks, title-page editing, project
actions and exports. The type rows use the selector's existing type model.
Tab/Shift+Tab move between the scrolling area and **Back to writing**; arrow
keys scroll and Space activates the button. Esc, the close button or clicking
outside dismisses help. Opening and closing retain the caret, selection
direction, writing and undo history.
The reference labels the current outline as a read-only preview and distinguishes
immediate PDF export from Fountain options. Export gates remain unchanged.

The new **Edit** menu exposes Undo/Redo, Cut/Copy/Paste and Select all with their
shortcut labels and current availability. Undo/Redo use Qt's existing history;
clipboard actions call the same protected screenplay commands as the keys.
Outline and Export tooltips/accessibility labels now include their shortcuts.
Writing/project accelerators are disabled while desk dialogs are visible, so
help or an editable dialog field cannot trigger an export/project departure
behind it. Title-form Ctrl+Enter remains local; F1 does not interrupt a dialog
or menu. No source schema, font pin, import/export version or dependency changed.

Focused tests first failed on absent F1/Edit controls. The Edit check then
reproduced a stale TextEdit `canUndo` binding after an external cursor edit:
the property getter and document reported available history while the binding
remained false. EditorCommands now forwards the installed document's
undo/redo-availability signals. The checks exercise initial/changed/exhausted
history, exact Undo/Redo, selected Apollo MIME copy/cut/paste, external paragraphs
and spaces, and title-boundary cut/paste refusal without changing writing,
selection or the clipboard. Help checks cover reversed selection, shortcut
isolation, narrow windows, keyboard scrolling/dismissal and title-dialog scope.
Visual review found the scrollbar could overlap key labels in a narrow window;
the content now reserves room for it.

| Check | Observed result |
| --- | --- |
| `mise run check` | **24/24** passed; both affected UI tests pass again after the scrollbar adjustment and final keyboard assertion |
| `mise run check-debug` | **24/24** passed with address/undefined sanitizers; both affected UI tests pass again after the final keyboard assertion |
| `mise run check-engine` | **2/2** passed |
| Native Wayland project flows, Compose input context, software renderer | **47 passed**, no failures/skips; final focused UI checks also pass after the Space assertion |
| Native Wayland/OpenGL Writing desk | Existing outline/focus check passes with the additional controls |
| Native compositor keys (`wtype`) | F1/Esc, background typing/export/project-action isolation, reversed selection, Undo/Redo, scroll/focus/Space dismissal and resumed writing pass with exact autosaved text and element identity |
| Direct completion review | Requirements, tracked and untracked code, tests and visual captures reviewed; whitespace check passes; no remaining in-scope finding |
| Additional Academy comparison | **Unavailable:** local `reference/academy/expected.tsv` and PDF are absent; independent synthetic fixture/PDF/oracle checks pass |

Logs: `build/task-3g-{release,debug,engine,academy}.log`,
`build/task-3g-{release,debug,native}-focused-final.log`,
`build/task-3g-native-{flows,editor}.log`, and
`build/task-3g-native-keys-final.log`. The initial red checks are retained in
`build/task-3g-{red,edit-red,edit-investigation}.log`. Visual captures are
`build/release/project-access-tests/shortcuts{,-small,-scrolled}.png`.
The repeatable compositor runner is `build/task-3g-native-smoke.sh`; its final
isolated project, app logs and exact source snapshots are under
`build/task-3g-native-keys-SzuUWs/`. The earlier ad hoc wtype run used Return on
the plain Qt button and did not activate it; the corrected runner and permanent
UI check use Space. No product behavior was changed to accommodate that
incorrect runner assumption.

Native input uses the installed Compose plugin because fcitx5 is unavailable;
the standard fcitx runner was not run. Compositor virtual keys do not measure
physical-key latency. Owner appearance/comfort and sustained typing performance
were not reevaluated, and the existing heavy-I/O crash-loss limit remains.
This step does not complete slice 3's broader Fountain compatibility, imported
PDF, assistance/navigation or the writing milestone.

Task 4a below completes the bounded character/cue-extension and
scene-heading-part suggestion step in slice 4. Spelling/dictionary, find/replace,
diagnostics and the live scene navigator remain separate work; the full
Fountain compatibility gate remains required.

### Task 4a — context-restricted writing suggestions

2026-11-04. **Done for this bounded task.** Implements the
[suggestion contract](spec.md#suggestions-and-character-identity) and
[editing plan](#editing-and-assistance-contract). Dependencies: existing
formatted editor, type controls and protected selection/clipboard behavior.
This step provides automatic applicable suggestions with keyboard acceptance
and dismissal, component-only headings, current-script character identity,
custom free text, one-step acceptance undo, persistence and native input checks.
Other slice 4 assistance and live scene navigation stay subsequent tasks.

#### Interaction and implementation conventions

These are bounded engineering choices within the owner's confirmed suggestion
rules, not new Fountain compatibility or character metadata:

| Context / key | Behavior |
| --- | --- |
| Typing a cue name | Prefix-match names from other cues in this script, ignoring trailing parenthesized qualifications for base-name lookup. Qualified JOHN cues contribute to one JOHN option. No global character dictionary. |
| Typing `(` after a name | Offer V.O., O.S., CONT'D and extensions already used in this script. Acceptance replaces only the extension, supplies a missing closing parenthesis and places the caret after it; an existing closing parenthesis is kept once. Custom extensions stay ordinary authored text. Additional/nested parentheses remain editable without completion. |
| Typing a heading | Offer INT., EXT. and INT./EXT.; after its space, offer previously used locations; after ` - `, offer time values. Recognize INT., EXT., INT./EXT., EXT./INT., INT/EXT. and I/E. for extracting heading parts. Use the final spaced dash as the time separator, preserving earlier dashes within a location. |
| Time values | DAY, NIGHT, MORNING, AFTERNOON, EVENING, LATER and CONTINUOUS, plus values already used in this script. |
| Ranking | Case-insensitive prefix lookup; uppercase options, ranked by occurrence count then alphabetical order. Built-in values contribute one initial occurrence. Exclude the active element; offer up to 12 distinct matches and omit textually identical no-ops. Narrowing the prefix finds later options. |
| Opening | Printable typing or a committed input-method string opens applicable matches with the first highlighted. Empty cue/heading starts permit Tab cycling. A typed `(`, heading prefix plus space or time separator may open that component's empty-prefix choices. Navigation, paste, Undo and loading do not automatically open menus. |
| Up / Down | Change selection, wrapping and scrolling within the menu; retain the caret. |
| Tab / pointer option | Explicitly accept only the active component in one undo step. Retain surrounding text, delimiters, types, identities, dual flags and matching prefix/suffix emphasis. |
| Shift+Tab | Dismiss and cycle to the previous common element type; never accept. |
| Escape | Dismiss without changing writing. Suppress this component while typing continues; moving to another component or Ctrl+Space permits reopening. |
| Ctrl+Space | Explicitly reopen at an unselected caret in a supported component. Other contexts remain unchanged. |
| Enter / Shift+Enter | Dismiss and perform the existing writing action, without accepting the highlighted match. |
| Selection, preedit, focus loss, revision/caret change | Hide/cancel results. Never apply completion to a selection or while composing. Structural completion keys reaching the editor during preedit cannot insert raw tabs/paragraphs. Other editing keys retain the existing protected commands, including clipboard and selected emphasis. |
| Other elements | No writing completions in action, dialogue, parentheticals, transitions, lyrics, centered text, title fields or markers. Deliberate multiline cues/headings remain editable without completion. |

The non-focusing menu sits by the scaled caret, stays within the writing
viewport, scrolls long lists and supports pointer acceptance. It retains the
stock TextEdit's selection and input-method ownership. F1's reference documents
acceptance, dismissal, reverse cycling and explicit reopening.

`WritingSuggestions` owns per-document transient queries. It reads at most 128
elements per event-loop batch, counts only prefix matches and checks revision
before publishing. Editing, moving or losing focus cancels pending work; applying
an option rechecks revision/caret/component/selection. Each request rebuilds
from current writing, so renaming/deleting cues and changing projects cannot
leave stale character options. No persistent cache, source schema, dependency,
font pin or import/export version changes.

#### Verification and review

The first real-key check failed on the absent menu before implementation.
Focused checks cover duplicate qualified cues, frequency/tie ranking, arrows,
Tab acceptance, reverse cycling, Escape suppression and component navigation,
Ctrl+Space, Enter without acceptance, custom names/extensions, excluded types,
selection/preedit/focus, stale revisions and queued requests after later keys.
They verify exact surrounding text, identities and emphasis through acceptance
Undo/Redo. A narrow-window check wraps/scrolls a 12-option list, checks its
bounds and accepts a pointer option while retaining editor focus. Synthetic
window captures were inspected.

Project-flow checks author completions through the actual editor, compare exact
autosaved source, reopen, create another project and verify that names do not
carry across projects. They check fixed canonical Fountain and use Poppler to
independently extract both qualified cues and their dialogue from the PDF.

Completion review found a caret stopping too early when spaces precede an
existing extension `)`. The focused reproducer reports expected position 30,
actual 27 before the correction and 30 afterward; the permanent check retains
the spaces and verifies the final caret. Deferred typing requests now carry
their intended caret, revision and key generation, preventing reopening after
Undo, navigation or Escape. Input-method requests use their expected committed
caret while Qt finishes assigning the revision. The native Compose observer
caught duplicate delivery in an intermediate implementation; commits now pass
through Qt once, with deferred suggestions observing the result.

A final composition-paste reproduction caught the early preedit guard letting
Ctrl+V use stock Qt paste: an external paragraph split one title field into two
(three total elements instead of two). The guard now reserves only structural
completion keys; other editing keys retain their protected screenplay routes.
The reproducer and permanent regression check now preserve one title field,
its identity and deliberate break, with one-step Undo. Both complete editor
suites and focused native checks pass again after this final correction.

Native testing also exposed readiness gaps in the existing UI helpers. Project
tests now wait for the initial screen to finish loading, window activation and
a rendered frame before sending keys; the direct editor helper also waits for
its first rendered frame. The intentional queued-worker-result teardown test
explicitly bypasses this wait and retains its original evidence/lock assertions.
Exact writing/shortcut assertions remain in place.
Earlier non-isolated native runs included intermittent page-break/title/import
and completion failures; their logs are retained below. No production fix is
claimed for the earlier page-break transient.

An isolated native Wayland/OpenGL probe used synthetic ordinary projects with
autosave enabled and 40 one-letter cue requests each:

| Workload | Key-handler p95 / max | Menu-ready p95 / max | Final sampled PSS |
| --- | --- | --- | --- |
| 203 pages (175 generated scenes) | 29.35 / 42.85 ms | 37.15 / 48.83 ms | 107.05 MiB |
| 507 pages (444 generated scenes) | 46.80 / 47.16 ms | 68.05 / 73.75 ms | 175.03 MiB |

These are synthetic key-to-handler and key-to-published-menu observations,
not compositor/physical-key-to-frame timings, peak memory, sustained writing,
cold opening or export-while-typing acceptance. The final probe waits for the
query signal in an event loop. An earlier polling probe added a 10 ms sleep per
batch and is not latency evidence; earlier harness attempts also omitted native
activation or selected a paragraph separator when clearing a cue. The corrected
probe selects only cue text. No production behavior was changed for those
harness mistakes.

| Check | Observed result |
| --- | --- |
| `mise run check` | **24/24 passed** on the final isolated release run, including crash checks and the independent Fountain oracle |
| `mise run check-debug` | **24/24 passed** with address/undefined sanitizers |
| Final editor rechecks after the composition-paste correction | **145 passed**, six platform/optional checks skipped, no failures in each of release and sanitizer builds |
| `mise run check-engine` | **2/2 passed** |
| Native Wayland / software project flows, Compose input context | **48 passed**, no failures/skips; includes the queued-result teardown exception |
| Native Wayland / OpenGL editor, Compose input context | **145 passed**, five optional checks skipped; full final run includes temporary key-state tracing, removed afterward |
| Final compositor (`wtype`) and focused native checks | **6 passed**, no failures/skips after removing tracing and fixing composition paste: completion/Undo/Redo/heading parts/Escape/Enter/Compose plus composition/stale-query/protected-paste and interleaved-title/page-break checks |
| Completion review | Requirements, tracked/untracked implementation and test necessity reviewed; caret, stale-trigger, input-event-delivery, composition-paste and test-readiness findings corrected; final whitespace check passes |

Final logs: `build/task-4a-{release,debug}-verified.log`,
`build/task-4a-engine.log`, `build/task-4a-native-flows-ready-final.log`,
`build/task-4a-native-editor-trace-final.log` and
`build/task-4a-native-ime-final.log`. Final editor rechecks are
`build/task-4a-{release,debug}-editor-final.log`.
`build/task-4a-red.log` retains the initial missing-menu failure;
`build/task-4a-extension-{red,green}.log` retain the caret reproduction.
`build/task-4a-composition-clipboard-{red,green}.log` retain the paste reproduction.
Synthetic menu captures are
`build/release/editor-tests/suggestions-{location,small-scrolled}.png`.
The disposable performance source is `build/task-4a-measure.cpp`, with final
results in `build/task-4a-measure-{200,500}-event-loop.log` and matching isolated
project folders. Earlier native/check/probe logs remain under `build/task-4a-*`.

An intermediate release run failed when the crash harness's `committed:3`
child exited normally before its planned kill; the final isolated release
suite passes all phases. This is not a fix to that harness timing or the
existing heavy-I/O crash-loss limit. Likewise, earlier direct-editor native
transients were not assigned a proven production cause; final full and focused
runs pass without weakening writing assertions.

Native input uses the installed Compose plugin because fcitx5 is unavailable.
The standard fcitx runner was not run. Physical-key latency, owner appearance/
comfort and sustained typing/export performance were not reevaluated. The
additional Academy reference is unavailable locally; independent synthetic
fixtures/PDF and Fountain-oracle checks pass. Imported PDF, broader Fountain
compatibility and the rest of the writing milestone retain their existing gates.

Task 4b below implements the next bounded spelling step. Find/replace,
diagnostics and the live scene navigator remain separate tasks.

### Task 4b — US English spelling and personal dictionary

2026-11-04. **Done for this bounded task.**
Depends on task 4a and the existing protected
editing commands. Underline potential misspellings without changing source,
offer replacements on demand with one-step Undo, and persist personal words
across this computer's scripts. Obsolete-result rejection, retained
formatting/structure, persistence and storage failure, real controls, native
Qt input, and required release/sanitizer/engine suites are verified below.

Engineering scope: bundled pinned US dictionary and Hunspell; background
checking of authored text, transient viewport marks, caret-word spelling menu
(F7 / Edit), explicit add-word action. No automatic replacement, language
selection, sync or changes to character completion identity.


The [bounded spelling contract](spelling.md) records tokenization, long-token
limits, the explicit F7/Edit panel, replacement/Undo, viewport-only marks and
personal-word storage/error behavior. Hunspell 1.7.2 is built from a hash-pinned
source archive; the committed SCOWL US English files retain their full notices
and configure-time hashes. See [dependencies and licenses](../assets/spelling/README.md).
No source schema, export profile, character identity or runtime tool pin changed.

Verification covers known US words/misspellings, UTF-16 locations after emoji,
deliberate breaks, exact retained identities/dual flags/emphasis, one-step
replacement Undo and source serialization/reopening. Personal words survive a
new editor, ignore case, merge independent-instance additions under a lock and
refresh an already open editor. Busy/corrupt dictionary cases preserve evidence
and writing; the US dictionary remains functional. Tests isolate personal state
and leave no user dictionary file. Real F7/Edit controls, pointer replacement,
Add, Escape, modal shortcut blocking and selection/preedit guards pass.
Underlines and menu captures were inspected.

Review covered tracked/untracked implementation, dependency/license provenance
and test necessity. The UI reproducer exposed a replacement destroying its
button delegate before the click handler finished; completion now runs in the
surviving editor context. Sanitizer project flows exposed a layout notification
reading obsolete mark positions before contentsChanged; published-revision,
current-word/block and layout guards now reject those positions. The same flows
pass with the guards, with a focused whole-document-change regression retained.
Review also corrected Edit-menu focus handoff and stale dictionary-error recovery.

| Check | Observed result |
| --- | --- |
| `mise run check` | **25/25 passed** |
| Final release recheck after review adjustments | **3/3 passed**: project flows (48 cases), spelling (5 cases including lifecycle), editor (146 passed, six platform/optional skips) |
| `mise run check-debug` | **25/25 passed** with address/undefined sanitizers |
| `mise run check-engine` | **2/2 passed** |
| Native Wayland/OpenGL, Compose context, sanitizer binary | **3 passed**, no failures/skips: focused spelling lifecycle including controls, persistence, Undo/Redo and modal/preedit/selection guards |
| `tools/check-wayland.sh --preflight` | Unavailable: required `wtype` command missing |
| Direct completion review / whitespace | No remaining in-scope findings; tracked whitespace check passes |

Logs: `build/task-4b-release-final.log`, `build/task-4b-release-recheck.log`,
`build/task-4b-debug-final.log`, `build/task-4b-engine.log`,
`build/task-4b-native-final.log` and `build/task-4b-wayland-preflight.log`.
Captures: `build/release/editor-tests/spelling-{underlines,menu}.png`.
Intermediate logs retain the earlier UI and sanitizer failures. An earlier
release attempt also overlapped relinking with process tests and reported
executable permission errors; final full/recheck runs do not overlap builds of
their own executables. No production fix is claimed for those harness errors.

Native input here uses Qt-synthesized keys and the Compose context. The attempted
compositor F7 check could not launch `wtype`; compositor/fcitx/physical-key input,
sustained spelling/typing/export performance, memory targets and owner comfort
remain unverified. Existing Academy-reference, imported-PDF, broader Fountain
compatibility and heavy-I/O crash-loss limits remain. This bounded task does not
complete slice 4 or the writing milestone. Task 4c below implements find/replace.

### Task 4c — find and replace

2026-11-04. **Done for this bounded task**, with the native-input limits below.
Dependencies: protected selection/clipboard editing (3f), writing suggestions
(4a) and spelling (4b). Implements the
[writing requirement](spec.md#writing-and-formatting) and the proposed literal
search/selection/Replace All Undo step in the
[editing plan](#editing-and-assistance-contract).

The [bounded contract](find-and-replace.md) records authored-text matching,
case/whole-word options, opening-selection scope, wrapping navigation, explicit
single replacement and one-step Replace All Undo. Edit and Ctrl+F / Ctrl+H open
the modal panel; Enter/Shift+Enter in Find and F3/Shift+F3 navigate. Title fields
are searchable separately; markers and generated labels are skipped. Every
replacement stays inside one authored line and one element, retaining IDs,
types, flags, breaks and surrounding emphasis. Replacement text takes its first
matched character's emphasis. Source, import/export contracts, dependencies and
tool pins are unchanged.

`FindReplace` owns bounded, cancellable event-loop scans and validates current
revision before publishing or replacing. Long queries, excessive match sets,
control/break insertion and excessive replacement expansion are refused before
editing, with a visible reason. Search state is transient per document.

The first real-key test failed on the absent panel. Focused checks cover literal
punctuation, authored case/spaces, Unicode and whole-word edges, reversed scope,
navigation, stale/closed/reconfigured scans, long-paragraph chunk boundaries,
caps, no-op history, exact emphasis/IDs/dual/title/marker preservation and source
reopening. A scope regression caught Qt's tracked selection anchor moving past
new text at its left edge; separate tracked endpoint cursors keep scope through
replacement and Undo. Completion review also reproduced a fully deleted scope
disabling its checked scope control; the opening-selection capability now stays
available even when its tracked range is empty, permitting return to whole-script
search. `build/task-4c-empty-scope-{red,green}.log` records the same QObject-property
probe before/after correction; the permanent scope test retains the case.
The first sanitizer run exposed repeated full-block copies for dense matches
(the 100,001-match refusal took 9.45 seconds). Scanning now reads each block
once per event-loop batch and searches string views inside it; the existing
bounded-input regression verifies the same refusal without a relaxed timeout.
Real QML controls cover both entry points, selected-query
seeding, scope, keyboard navigation, single/all replacement, Undo/Redo, modal
blocking, composition guarding and a smaller desk. Project-flow checks exercise
autosave/reopen and compare actual Fountain with hand-written text; Poppler
independently extracts the expected two-page PDF text.

Native shortcut probing is limited by an unresolved Qt/wtype lookup discrepancy:
tracing recorded an actual `Key_H` with Control but possible key sequence
`Ctrl+Esc`. One isolated compositor run authored replacements and undid them,
but later combined and isolated runs missed the opening shortcut; a delayed
wtype probe did not resolve it. A combined synthesized native run also missed
F3, while its individual cases pass. These observations do not establish
reliable compositor shortcut interaction, and no production key handling was
changed to accommodate them. Review removed the unreliable optional compositor
case from the permanent suite and native runner; its complete source remains in
`build/task-4c-compositor-test.cpp.fragment`. The successful run is
`build/task-4c-native-compositor-isolated.log`; failures/traces remain in
`build/task-4c-native-{editor,editor-repeat,editor-final,shortcut-trace-all,delay-trace}.log`.

An isolated native Wayland/OpenGL probe opened ordinary generated projects with
autosave/spelling enabled, measured 20 alternating whole-word queries, then
replaced `radio` with `signal`, undid it and waited for Saved:

| Workload | Search-ready p95 / max | Replace All count / synchronous duration | Final sampled PSS |
| --- | --- | --- | --- |
| 203 pages | 11.12 / 37.04 ms | 524 / 47.91 ms | 115.96 MiB |
| 507 pages | 14.89 / 33.23 ms | 1,331 / 129.97 ms | 174.63 MiB |

These are API-to-search-result and synchronous document-edit observations,
not physical-key-to-frame timings, sustained typing/export, cold-open or peak
memory acceptance. Replace All runs in a modal panel and remains synchronous.
The disposable probe is `build/task-4c-measure.cpp`; final results/projects are
`build/task-4c-measure-{200,500}-verified{.log,/}`. These final measurements use
the corrected scan and scope implementation; earlier observations remain under
`build/task-4c-measure-*-final*`. Its initial relative output path
was correctly refused by the session-state contract; final runs use absolute
paths. No production behavior was changed for that invocation error.

| Check | Observed result |
| --- | --- |
| `mise run check` | **26/26 passed** before the final scope/scan review corrections |
| Final release build and affected rechecks | **3/3 suites passed** after those corrections: search **9 passed** including lifecycle; editor **148 passed**, six platform/optional skips; project flows **49 passed** |
| `mise run check-debug` | **26/26 passed** on the corrected implementation with address/undefined sanitizers, including the independent Fountain oracle and crash checks |
| `mise run check-engine` | **2/2 passed** |
| Native Wayland / OpenGL, Compose input context | Both focused Qt-synthesized editor cases pass in separate processes, including controls, selection, navigation, replacement, Undo/Redo, composition guard and smaller-window access |
| Native Wayland / software project flow, Compose context | The focused autosave/reopen/Fountain/PDF case passes |
| Completion review / whitespace | Requirements, tracked/untracked implementation and test necessity reviewed; scope and scan findings fixed and rechecked; no remaining code finding; final whitespace check passes |

Final logs: `build/task-4c-release.log`,
`build/task-4c-{unit,editor,flows}-verified.log`,
`build/task-4c-release-build-verified.log`, `build/task-4c-debug-verified.log`,
`build/task-4c-debug-unit-focused.log`, `build/task-4c-engine.log`,
`build/task-4c-native-find_{replace_real_controls_and_undo,selection_navigation_menu_and_composition}-verified.log`
and `build/task-4c-native-flow-verified.log`. Captures inspected:
`build/release/editor-tests/find-replace{,-small}.png`.
The earlier sanitizer timeout remains in `build/task-4c-debug-final.log`;
that pre-correction run was stopped after its failure, and the full corrected
run above passed. The earlier initial debug build was interrupted; it supplies
no passing-suite claim.

The installed Compose plugin supplies native Qt input here. fcitx/physical-key
input and reliable compositor shortcut interaction remain unverified; the full
fcitx runner was not run. No new owner comfort, sustained typing/export, peak
memory, Academy-reference, broader Fountain compatibility or imported-PDF
acceptance follows from this task. Existing heavy-I/O crash-loss limits remain.
Diagnostics and the live scene navigator are the next separate slice 4 tasks;
this does not complete slice 4 or the writing milestone.

### Task 4d — speech-structure diagnostics

2026-11-04. **Done for this bounded task.** Dependencies: empty-speech review (2.6c),
dual dialogue, element-type controls (3e), selection/clipboard (3f) and
suggestions (4a). Implements the
[writing requirement](spec.md#writing-and-formatting) for flagged structural
problems with undoable corrections. The owner chose the bounded
speech-structure set and both corrections for dialogue without a cue
([round 26](planning.md#round-26--speech-structure-diagnostics-settled-2026-11-04)).
The [contract](diagnostics.md) records issue rules, corrections and refusals.

One located list replaces the empty-speech bar: dialogue without a cue (**Add
character cue** / **Treat as action**), empty speeches, dialogue elements that
run together (**Join as one dialogue**) and incomplete Dual Dialogue (**Turn
Dual Dialogue off**, which also moves the former warning line into the list).
Every correction is revalidated against the live document, changes no words
and is one Undo step. Corrections that could create, reassign or structurally
edit a pairing are disabled with their reason. That includes joining inside a
pair, because the owner's dual contract requires unpairing first. Source,
Fountain/PDF contracts, import admission, dependencies and tool pins are
unchanged. `emptySpeeches` remains the empty-speech subset; the measurement
tool and existing flows now use the shared controls.

Evidence and review:

| Check | Observed result |
| --- | --- |
| Focused editor cases | Hand-counted issue positions/ranges and order; real-key Tab/Undo resolving and restoring issues; deliberate/blank dialogue lines never flagged; each correction against a hand-written expected model (inserted cue, types, joined text with `\n` breaks and shifted emphasis byte offsets, identity retention); a single Undo restores the exact original and Redo reapplies; stale and disabled requests leave writing and undo history unchanged |
| Mutation checks | Joining with a space instead of a line break, and offering Add character cue before a following dual cue, each fail the new cases; restored code passes |
| Real Writing desk controls | Issue list selection, both cue-less corrections, cue typing with ordinary keys, bar hiding and two-step Undo; captures inspected at 1100 and 640 px (`build/release/editor-tests/speech-issues{,-small}.png`) |
| Project flow | Real controls add a cue, type `THEO`, join dialogue; autosave, reopen and clean Fountain equal hand-written bytes; Fountain is refused before correction |
| `mise run check` | **26/26 passed** on the final code (editor 156 passed, six platform/optional skips; project flows 50 passed) |
| `mise run check-debug` | **26/26 passed** with address/undefined sanitizers, including the independent Fountain oracle and crash checks |
| `mise run check-engine` | **2/2 passed** |
| Native Wayland, Compose input context | The three focused desk/editor cases pass on OpenGL; the project-flow case passes with software rendering |
| Measurement tool | `--offscreen` on a disposable unfinished-speech file corrects one empty speech through the renamed controls; exact save/reopen/re-export |
| Completion review / whitespace | Requirements, implementation and test necessity reviewed; a join inside a pair was found allowed against the dual contract, fixed and covered; `git diff --check` passes |

Logs: `build/task-4d-release-final.log`, `build/task-4d-debug.log`,
`build/task-4d-engine.log`, `build/task-4d-native-editor.log` and
`build/task-4d-native-flow.log`. The earlier `build/task-4d-release.log` ran
before the pair-join correction.

Limits: compositor-delivered keys, fcitx and physical input were not exercised
for these controls. Owner comfort with the issue bar was not assessed, and no
performance was measured beyond the existing linear refresh per document change.
Other diagnostics, the mixed-case exception and blank-spacing advisories remain
unspecified. The live scene navigator is the next separate slice 4 task; this
does not complete slice 4 or the writing milestone.

### Task 4e — live scene navigator

2026-11-05. **Done for this bounded task.**
Depends on the source adapter, scene-boundary decisions, element/selection
controls and speech protections. Implements slice 4's click navigation and
undoable whole-scene dragging. The [contract](scene-navigator.md) records the
interaction conventions; confirmed scene boundaries remain unchanged.

The outline follows heading edits/types and the caret, uses stable identities
for duplicate/empty headings, and reveals the selected heading on click. Drag
handles show insertion lines and scroll at the list edges. Moves preserve exact
text/types/styles/identities, deliberate/blank lines, markers and dual flags;
title/preamble/imported material remain separate. Selection endpoints follow
their elements. Esc, edits and stale requests cancel without changing writing.
Alt+Shift+Up uses the same general move command; Undo/Redo retain Qt's history.

The initial live-outline test failed against the opening-only implementation. A new
move-to-end test also detected an appended empty element caused by comparing
two different invalid Qt text-block sentinels; bounded valid-block iteration
fixes it. Real row/handle tests cover navigation, reversed selections, Escape,
edit cancellation, composition, both move directions and autoscroll. A project
flow checks autosave/reopen, hand-written canonical Fountain bytes and
independently extracted PDF scene order.

| Check | Observed result |
| --- | --- |
| `mise run check` | **26/26 passed** on final code and tests: editor 160 passed with six existing platform/optional skips; project flows 51 passed; independent fixtures, aw-parser oracle and crash checks included |
| `mise run check-debug` and final `ctest --preset debug --rerun-failed` | All **26 checks pass across the full run and affected recheck** with address/undefined sanitizers. The full run passed 25/26; the new project-flow case failed only on an incidental capitalized status-string expectation. That assertion was removed in favor of independently reading the actual PDF, and the entire affected suite then passed |
| `mise run check-engine` | **2/2 passed** |
| Native smoke, Wayland/OpenGL/Compose | `QT_IM_MODULE=compose tools/inspect-writing-desk.sh --smoke` passes the six desk/editor cases (eight including setup/cleanup), creates the disposable writer and checks its sample editor/PDF export |
| Native project flow, Wayland/software/Compose | Drag, Undo/Redo, exact source/autosave/reopen, hand-written Fountain bytes and Poppler PDF scene order pass on the final test. A transient compositor resize had made an earlier pointer run too small; the fixture now requests/waits for its minimum geometry |
| Completion review | Requirements, actual changes and test necessity reviewed; removed the superseded scene-up-only API; fixed the end sentinel bug and the runner's separate create/open commands. `git diff --check` and `bash -n tools/inspect-writing-desk.sh` pass |

Logs: `build/task-4e-release-final.log`, `build/task-4e-debug.log`,
`build/task-4e-debug-recheck.log`, `build/task-4e-engine.log`,
`build/task-4e-native-smoke-final.log` and `build/task-4e-native-flow-final.log`.
The earlier release/flow logs contain the fixture expectation corrections,
not final passing results.

The Writing desk smoke runner now exercises the navigator and sample export;
its manual path opens a disposable autosaving writer. Screenshots at 1100 and
640 px were inspected. Full native-compositor pointer input, fcitx, physical
input, owner comfort and fresh sustained typing/crash-loss measurements remain
unverified. Existing import/PDF and broader Fountain gates remain in force.

### Task 5a — bounded PDF export lifecycle

2026-11-05. **Done for this bounded task.** This bounded slice 5 step
follows task 4e. Depends on the existing immutable PDF helper, autosave/close
guards and Writing desk export panel. The [contract](pdf-export.md) describes
Cancel Export, the 30-second Cancel/Keep Waiting watchdog, asynchronous
Home/Close behavior, bounded teardown and captured-snapshot status. Imported
PDF restrictions and independent layout gates remain unchanged.

The first regression check failed against the old destructor: a stalled helper
kept teardown waiting for its three-second lifetime. Teardown now cancels
unfinished work and reaps the helper without publishing. In normal operation,
cancellation remains asynchronous and busy until the helper exits, including
when requested during startup or after painting a complete staged PDF. Prior
PDF bytes survive, temporary files are removed, and retry succeeds. Window
close waits through an explicit choice and re-enters the autosave guard when
export finishes, so later writing is still saved. Home uses the existing drain
flow; cancelling a stuck export lets that flow finish.

Focused tests exercise actual helper processes and QML controls. The existing
snapshot test now independently extracts its PDF with Poppler and compares a
hand-written expected line after the live writing was replaced and destroyed.
The close test also covers normal successful completion without cancelling.
Review includes the new tests' purpose, cleanup and interactions with autosave.
It also found that the old PDF destination guard compared only the originally
opened path, permitting a destination inside an active project. A new test
failed against that behavior. PDF now refuses active-project contents and
file/parent aliases, with a Save a Copy regression checking that the protected
root follows the active destination without replacing its saved source.

| Check | Result |
| --- | --- |
| Regression baseline | Old code fails bounded teardown and protected-project destination checks; `build/task-5a-red.log` and `build/task-5a-dest-red.log` |
| Release | `mise run check`: 26/26 pass; both affected suites pass after the review fixes (`build/task-5a-review-release.log`) |
| Sanitizers | All 26 checks pass across the full run (24/26) and final affected recheck (4/4), with address/undefined sanitizers. The two full-run launch failures are explained below |
| Engine | `mise run check-engine`: 2/2 pass |
| Native Wayland/OpenGL/Compose | Final affected run: PDF lifecycle/protection 17/17; Home/Close and Save a Copy 5/5. The full smoke runner passed before the destination fix; subsequent unchanged navigator checks were intermittent as described below |
| Review | Requirements, actual code/untracked contract and test necessity reviewed. Both reproduced bugs fixed; affected release/sanitizer/native checks pass. `git diff --check` and `bash -n tools/inspect-writing-desk.sh` pass |

Full runs: `build/task-5a-release.log`, `build/task-5a-debug.log` and
`build/task-5a-engine.log`. Final affected rechecks:
`build/task-5a-review-release.log`, `build/task-5a-review-debug.log` and
`build/task-5a-native-affected.log`. The sanitizer run's last two subprocess
checks failed to launch because the agent overlapped a rebuild with them
(`PermissionError` during relinking). After the rebuild finished, both and the
two affected suites passed in one serial recheck. This was a verification
scheduling error; no assertion was weakened. A final combined-alias case
(parent alias into the project, then a file link pointing outside) also passes
in the release/sanitizer PDF suites (`build/task-5a-alias-release.log` and
`build/task-5a-alias-debug.log`) and final native PDF suite
(`build/task-5a-native-alias-final.log`, 17/17).

Native full smoke passed in `build/task-5a-native.log` and
`build/task-5a-native-recheck.log` before the destination fix. An intervening
repeat failed the existing outline shortcut check, and the post-fix full smoke
stopped on the existing scene-autoscroll timeout (5 s; Qt reported 6.45 s would
have sufficed under that run's load). Their implementation was unchanged;
final native PDF/leave/copy cases were run directly and passed. These
intermittent navigator checks remain a separate verification limitation.

The local `reference/academy/` directory is empty, so no new Academy comparison
was available. Required independent hand-derived fixtures, editor/PDF checks
and aw-parser passed; no layout/profile code changed here. Fresh Academy and
responsiveness evidence remain part of the separate production export gates.

The owner runner is
`QT_IM_MODULE=compose tools/inspect-writing-desk.sh --smoke`; it now includes
native PDF lifecycle/departure checks and still saves each stage's output.
These tests send Qt key/mouse events. Full compositor/physical input, fcitx,
owner comfort, new sustained latency/crash-loss measurements and filesystem
stall cancellation are not established by this task.

### Upcoming slice 5 tasks

- **5b — bounded Fountain export lifecycle (ready).** Apply the agreed fixed
  snapshot, cancellation/watchdog and bounded shutdown requirements to Fountain's
  current in-process worker. Verify actual worker lifecycle, safe existing
  outputs, retry, typing, Home/Close and truthful snapshot status. Preserve
  [canonical eligibility](fountain-export.md) and independent aw-parser evidence.
- **5c — production export responsiveness and remaining evidence (pending).**
  Depends on 5a/5b. Measure native primary/stress writing with autosave and all
  current assistance enabled, including Fountain/PDF exports while typing,
  saved output correctness and independent layout fixtures. Record cold/warm
  launch, save age, stalls and memory with honest measurement limits. This does
  not close the separate broader import/compatibility or slice 2/6 work.
