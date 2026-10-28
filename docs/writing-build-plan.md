# Apollo — first usable writing build

2026-10-28. Status: requirements planning complete; milestone scope and final
product choices agreed. Engineering designs below are proposals to validate
within the implementation slices. No owner question from this interview remains
pending. Implementation requires task-specific authorization; the progress
record below distinguishes completed work from remaining proposals.

**Current status (2026-10-28):** slice 1 and tasks **2.1–2.3** are done.
Project sessions are guarded; Save a Copy becomes active without resetting the
editor; Reload preserves current writing first; recovery/emergency packages and
explicit failure-close choices are implemented and verified as recorded below.
Next: project access, including damaged-save selection/reconciliation and resume.
Leak detection and crash loss during copying remain unverified. The other slice 2
flows and slices 3–6 remain pending. Concurrent heavy I/O can exceed the measured
one-second crash-loss target; see task 2.3's evidence and limits.

The [specification](spec.md) holds confirmed behavior. [Planning rounds 18–23](planning.md#round-18--first-usable-writing-milestone-boundary-2026-10-28)
record the owner's milestone answers. The [spike report](spike-report.md) holds
the accepted Qt/Writing desk foundation, measurements, and known limitations.

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
- Remaining slice 2 work is pending: home/create/open/import, imported-material
  reader, switching/resume, damaged-save selection and recovery reconciliation
  on open, and stale temporary cleanup. Focusing an existing process's window
  belongs to the later open flow.

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
bounds with unavailable storage remain unverified. The damaged-save picker and
reconciliation on open are pending project-access work; packages are already
openable directly without silent rollback.

### Slice 1 result (done 2026-10-28)

The implemented contract and measurements are in [source-format.md](source-format.md);
it supersedes the proposals above where they differ. A project folder opens
in `apollo_editor` with autosave (`apollo_editor DIR --create [FILE.fountain]`
creates one); a Fountain path still opens unsaved.

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
