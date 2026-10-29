# Bounded editable Fountain preservation contract

2026-10-28. This is task **2.6c**, a bounded continuation of 2.6b, not complete
Fountain compatibility or completion of writing slice 3. Contract `writer-subset-1`
(now `writer-subset-3`, below) is an engineering choice within the requested preservation work.

Admission requires a verified revision-1 interpretation with no unresolved ranges,
a nonempty screenplay, at most 64 KiB original, at most 256 elements and 256
retained records, and a successful engine layout. Supported content is ordinary/forced headings and cues in ASCII
uppercase (including extensions and authored CONT'D), action, dialogue,
parentheticals, transitions, centered text, standalone lyrics, manual breaks,
and all six title fields. The title is uppercase; title fields are single lines of at most 48 characters.
Other text may contain printable ASCII, LF in action/dialogue, and U+00E9 (é).
Simple bold/italic/underline and their supported combinations/escapes are included.
Dual dialogue is admitted only with the converter's unambiguous pairing.
Action/dialogue lines must be nonempty and have no leading/trailing or repeated
spaces. Tabs, extra vertical spacing, other screenplay Unicode, mixed-case headings/cues/
titles, and any other reported layout limitation remain review-only. These bounds
are conservative evidence limits, not a claim that other writing is invalid.
Notes/stash, section hierarchy, synopses, unknown title metadata and scene numbers
remain separate read-only records with exact original text, names, IDs and ranges.

**`writer-subset-2` (task 3a)** changes only case, under the
[case contract](case-handling.md). New copies admit headings in any ASCII case
and cues whose lowercase letters are only in extensions (`EVE (cont'd)`). Their
authored text and the original bytes are kept, and they are presented in
capitals. A forced mixed-case name (`@McKay`), a heading or cue letter that cannot be
presented in capitals (é), and a title not in capitals remain review-only, each
with a located reason. Copies created under `writer-subset-1` are still validated
by its rules (headings and cues in ASCII capitals); older Apollo builds refuse
`writer-subset-2` copies.

**`writer-subset-3` (task 3b)** changes only spacing, under the
[spacing contract](spacing.md). New copies admit spaces at the edges of dialogue
lines, lines of spaces inside dialogue (intentional blank lines), and repeated
spaces inside action and dialogue lines, keeping their authored text and the
original bytes. Action indentation or trailing spaces, a blank first or last
dialogue line, repeated spaces in other elements, tabs and extra vertical
spacing remain review-only. Copies created under `writer-subset-1` or `-2` are
still validated by their stricter spacing rules; builds before task 3b refuse
`writer-subset-3` copies.

**Edit in a New Copy** creates a staged project with a fresh identity. Neither a
review project nor the external Fountain is upgraded or rewritten. Publication
and activation verify the reviewed snapshot. The working metadata retains the
frozen interpretation and untouched source; autosave writes screenplay source.
Old review copies remain read-only, including when they happen to meet admission.

The preservation unit is an element: ordered type, authored text, emphasis,
identity and dual flag, with title fields stored separately in their original
order. Editing, undo/redo, autosave, restart, verified recovery, and Save a Copy
must preserve the independently specified initial content and intentional edits.
Deliberate LF breaks remain internal to action/dialogue; Enter inside dialogue
adds a break to the same speech, without a blank row or repeated cue.

Imported associations are **historical element associations**, not tracked text
ranges. The original element ID and source location never change. In-place edits
and scene moves retain the ID. A split keeps the ID on its first nonempty part;
new parts and pasted copies get fresh IDs. A removed/merged-away anchor is visibly
“Detached”; a changed element type is visibly “Type changed”. Undo restores the
original connection. No record is deleted or silently retargeted to adjacent
writing. Section/synopsis hierarchy and project-wide records remain available.
This does not implement anchored notes or automatic tracking of edited passages.

The imported-material reader remains accessible from the writer, with the full
original source, named records and current association state. PDF export stays
blocked for every imported working project, including copies and recovery forks.
At 2.6c clean Fountain was blocked too; task **2.6d** now exports it when the
*current* snapshot meets the same content rules plus the
[canonical export contract](fountain-export.md), with an independent exported
round-trip on these fixtures. Complete case/spacing/font contracts, full
authoring controls and import performance remain blockers. Saving unfinished or
unrenderable later edits remains allowed; admission is not a restriction on
preserving new writing.

Independent examples live in `fixtures/import/writer-subset*`: original authored
Fountain, hand-written source semantics and retained-record expectations. Actual
QML controls and keyboard flows must be exercised, not only the converter and
source adapter. Final measured evidence is recorded below after verification.

## Verification and review

The hand-written screenplay and record fixtures match the complete conversion,
including code-point styles after é, dual flags, every title field, record IDs,
text and byte/line provenance. Gate checks cover mixed case, unverified Unicode,
tabs/spacing, unresolved delimiters/emphasis, lyric-in-speech, deliberate empty
speech lines, source size and retained-record count, plus altered/newer metadata.

The actual QML writer tests cover failed/successful new-copy publication with the
old reader lock retained on failure, exact initialization, independent profile
slots/columns for authored breaks, split/join/deletion/type changes, scene moves,
undo/redo, autosave and resume, the live read-only original/record reader,
persisted detached anchors, Save a Copy adoption and continued saving, damaged
source recovery through explicit controls and continued editing in the fork.
Both LF and BOM/CRLF variants retain their own exact original bytes and record
strings. Existing PDF output survives a refused request. The executable harness
adds typing and SIGKILL at writing, written, committed and acknowledged phases,
independent JSON comparisons, stale-lock reopen/resume and refused snapshots.

Direct review covered admission against the declared subset, frozen converter
compatibility, Unicode/source offsets, identity and association rules, immutable
assets, publication/activation, autosave/copy/recovery and the actual export
boundary. It corrected activation that had compared only the imported assets:
a real worker publication followed by a valid replacement screenplay could open
unreviewed writing. The new check compares the exact initial converted screenplay
as well. Its regression fails before the fix and passes afterward, with the
replacement evidence and previous resume/recents preserved. Review also corrected
the active-location reader label after Save a Copy, documented whole-element
versus partial-join identity behavior, and made the initial PDF status disclose
the block. Writer and detached-note reader captures were visually inspected.
No outstanding concrete in-scope finding remains.

Required verification: release 20/20; ASan/UBSan 20/20 with default leak detection;
engine-only 2/2; Academy independent comparison passes with its four existing
documented deviations. After the activation fix and extra edge checks, the full
release suite passes again and the related sanitizer suite passes 9/9. Native
Wayland preflight passes: 56 interaction cases (including compositor/fcitx input),
13 export lifecycle cases and 29 project-flow cases, plus workload first-paint
and export checks. The final native project-flow run passes 30/30, and focused
Enter/identity checks pass 5/5 including setup/cleanup. Native project flows use
synthetic Qt events and software rendering; the preflight also exercises the
native threaded OpenGL editor path. Full typing measurements were not rerun.

Logs: `build/task-2.6c-verified-release.log`, `task-2.6c-debug-final.log`,
`task-2.6c-reviewed-debug-related.log`, `task-2.6c-engine.log`,
`task-2.6c-academy.log`, `task-2.6c-native.log`,
`task-2.6c-reviewed-native-{flow,editor}.log`; all are under `build/`.
The successful preflight artifacts are under
`build/wayland-check-20261028-170147-ZEIQ7F/`.
Red regressions are `build/task-2.6c-{layout,enter,activation}-red.log`.
The final label-only correction has additional focused checks recorded below.
The initial sandbox sanitizer run could not use LeakSanitizer; the successful
full and related runs were outside that restriction with leak detection enabled.

Still unverified: complete Fountain grammar, mixed authored case, meaningful
spacing/empty-dialogue layout beyond this subset, full font coverage, correction
UI, title and dual authoring controls, exported independent semantic round-trip
(added for the subset by 2.6d), realistic import/typing performance with the complete milestone, staging-kill or
simultaneous-source-mutation guarantees, power-loss durability and subjective
writing comfort. Historical associations do not follow edited text passages;
that limitation is explicit rather than silently retargeting records. No clean
Fountain or PDF compatibility promise was made for these imported projects at
2.6c; [fountain-export.md](fountain-export.md) records the later bounded Fountain step.

Final focused checks after the initial export-status correction pass: release
3/3 and ASan/UBSan with leak detection 3/3 (actual intake processes, full project
flows and export jobs), plus native LF/CRLF writer and activation cases 5/5
including setup/cleanup. Logs are `build/task-2.6c-final-related.log`,
`build/task-2.6c-final-debug-related.log` and
`build/task-2.6c-final-native.log`. Final diff inspection found no additional
in-scope issue; no dependency/runtime pins or independent layout references changed.
