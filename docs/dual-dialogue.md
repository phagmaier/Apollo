# Dual-dialogue authoring

2026-10-29, bounded slice 3 task. The Writing desk offers **Format → Toggle
Dual Dialogue** and **Ctrl+Shift+D**. Place a single caret in the second cue,
parenthetical or dialogue. The command changes only that cue's source-v1
`dual` flag in one undo step. Turning it off restores sequential speeches.
Words, cue extensions, emphasis, deliberate/blank lines and identities remain
unchanged. Autosave and reopen use the existing source adapter.

The owner chose **complete speeches only; explicitly unpair before structural
changes** during this task. Complete means a nonblank cue and at least one
nonblank dialogue element in its consecutive dialogue/parenthetical run. This
is an authoring precondition, not a new source schema or import restriction.

| Context | Behavior |
| --- | --- |
| Two complete adjacent unpaired speeches | Pair using the second speech's cue flag |
| Caret in a right speech already marked dual | Turn its flag off, even if unfinished or missing its left partner |
| First speech, action, title, marker, orphan dialogue, incomplete speech | Refuse visibly; retain writing and undo history |
| Action, heading, page break or other element between speeches | Refuse; do not search across the barrier |
| Either speech already belongs to another pair | Refuse; explicitly turn the existing pair off first |
| Invalid chain of three or more dual cues | Refuse to clear a flag that a following dual cue depends on; clear the last marked speech first, avoiding implicit partner reassignment |
| Any selection when toggling | Refuse and retain selection; use a single caret |
| Ordinary typing, within-element replacement/deletion, compatible inline paste, emphasis, compose/IME | Keep the pair; blank writing remains unfinished and gets a warning |
| Enter inside dialogue | Keep an authored deliberate break, including blank internal lines |
| Enter at a cue/parenthetical end, or on an empty parenthetical | May add dialogue without detaching the pair |
| Enter at the final nonempty dialogue end of the whole pair | Add following action, keeping the pair |
| Type cycling, cue splitting, Enter that would introduce action inside the pair, boundary joins or cross-block replacement/cut/delete touching a pair | Refuse visibly until explicitly unpaired |
| Structured/multielement paste into a pair, or primary-selection paste there | Refuse; use compatible inline Ctrl+V paste or unpair first |
| Page-break insertion in either speech | Before the whole pair, or after its safe final end, using the existing marker command |
| Page break adjacent to a pair | Marker-only removal remains available; ambiguous attachment across a barrier stays refused |

Conservative cross-block protection includes selections that end at a paired
cue's start and removal of a neighboring separator. Copy is unrestricted.
Apollo does not guess whether such replacement should remove a cue, change an
association, or retain a pair. Unpair, make the structural edit, then explicitly
pair complete adjacent speeches again. Ordinary external multiline paste still
uses the spike paragraph rules outside pairs; this is not a general paste redesign.

Existing incomplete/invalid saved pairs retain every flag and word. A persistent
warning says the layout is a preview and Fountain may be refused. Missing speech
parts retain the existing profile's ordinary-cue preview; it is not represented
as a completed dual pair. Clearing words from a complete pair keeps its elements
and flag, so an empty dialogue part does not silently become action.

## Layout and exports

The profile still uses two columns (dialogue at 108 and 331.2 pt, cues at 180
and 403.2 pt), with indivisible groups of at most 56 slots. Oversized groups
keep their text and flag in a reading preview, with a persistent, wrapped notice
above the page. The PDF worker refuses them without replacing an existing PDF.
Shortening the group or explicitly unpairing permits reassessment; a long
sequential speech can itself remain outside the profile's splitting rules.
Small-pair editor/PDF agreement does not validate oversized dual pagination.

Blank dual lines now consume the correct authored separator before mapping the
next row. Previously an empty row could reuse the preceding row's source end,
causing a QTextLayout boundary error. Hand-counted offsets for repeated text and
blank lines in both columns protect the fix.

Fountain stays under `fountain-canonical-4` and its per-snapshot checks. The
second cue gets ` ^`; no flag or marker is typed into screenplay text. Empty,
ambiguous or otherwise ineligible snapshots keep the existing located refusal.
The writer-subset and frozen interpretation revisions are unchanged. Editable
imports can use these controls, retain their original/material assets, and
continue to refuse PDF. Review-only intakes remain review-only.

## Verification

- `editor_tests`: real keys pair/unpair from all speech parts, exact content,
  extensions/styles/IDs, Undo/Redo, invalid contexts and chains, selected and
  boundary edits, paste and IME refusal, deliberate breaks, unfinished writing,
  and visible oversized/incomplete warnings.
- `geometry_tests`: hand-derived slots, column/source offsets, emphasis after a
  blank row, and the exact 56-slot limit. The new offset assertions fail against
  the original engine at both blank rows and pass with the fix.
- `project_access_flow_tests`: real menu/keys, paired-word edits, adjacent page
  breaks, autosave/reopen, both exports, oversized preservation/save and PDF
  refusal without replacement, plus editable-import controls and retained gates.
- `fountain_export_process_tests` and its aw-parser variant: exact hand-written
  canonical bytes, fresh-process export/reimport, imported PDF refusal, Poppler
  page/slot/column checks and bold rendering. The source fixture is unpaired;
  only the right cue gains a flag. The `.expected` file is hand-derived, not an
  engine dump. Underline spans are checked in source and independent Fountain
  semantics; this fixture does not add a separate raster underline measurement.
- `tests/dual_dialogue_native_smoke.py`: compositor-delivered wtype pair/unpair,
  word editing, protected structure, Undo/Redo, exact saved IDs, fresh-process
  reopen, canonical Fountain and hand-derived PDF columns. Integrated in
  `tools/check-wayland.sh` for a single native runner.

Owner appearance and extended writing comfort remain unaccepted. This task does
not complete slice 3, general Fountain compatibility, imported PDF validation,
or the writing milestone. Existing performance and crash-durability limits
remain; no fresh sustained-typing performance claim follows from these checks.

## Final check results — 2026-10-29

| Check | Final result |
| --- | --- |
| `mise run check` (inside the native runner) | 24/24 passed, including both Fountain process/oracle checks; aw-parser independently read 33 exports across the suite |
| `mise run check-debug` | 24/24 passed with address, undefined-behavior and leak sanitizers enabled |
| `mise run check-engine` | 2/2 passed, including the new source-offset and exact-height expectations |
| `tools/check-wayland.sh --preflight` | Passed: 100 editor, 13 export and 43 project-flow checks, all without skips; fcitx/compositor input; all four wtype smoke checks; native editor/PDF agreement and rendering preflight |

Native logs and synthetic saved-source evidence are under
`build/wayland-check-20261029-141659-qZ47g3/`; the CTest logs remain under each
build's `Testing/Temporary/`. The native agreement workloads cover 203/507 pages
before edits and 204/508 after 50 scripted edits each. These are automated native
checks, not owner acceptance or sustained-typing measurements. The runner's
`--preflight` deliberately omits the full typing performance run.

The checks used the installed pins, a writable temporary uv cache and an empty
Qt platform-theme setting for automation. Sanitizers ran outside the restricted
process environment so LeakSanitizer could inspect threads; leak detection was
not disabled. No dependency, schema, converter or export-contract revision changed.

Final direct review checked requirements, pair semantics, edit/clipboard/IME and
marker boundaries, autosave, snapshot isolation, refusal paths and test quality.
It fixed the blank-row mapping, legacy-chain partner reassignment, safe
marker-selection deletion and snapshot-instrumentation assumptions. There are
no remaining in-scope implementation findings. The limits above remain explicit.
