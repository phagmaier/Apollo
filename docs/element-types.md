# Element-type authoring (task 3e)

The Writing desk's element selector shows the type at the caret and offers all
eight body text types. Labels include their direct shortcuts:

| Type | Shortcut |
| --- | --- |
| Scene heading | Ctrl+1 |
| Action | Ctrl+2 |
| Character | Ctrl+3 |
| Parenthetical | Ctrl+4 |
| Dialogue | Ctrl+5 |
| Transition | Ctrl+6 |
| Lyric | Ctrl+7 |
| Centered / act label | Ctrl+8 |

An act label is ordinary centered text, for example `ACT ONE`. It has no special
television pagination or nonprinting section metadata. It saves as `centered`,
exports to Fountain as `> ACT ONE <`, and prints at the profile's centered body
column. Lyrics save as `lyric`, export as `~text` and print in the existing lyric
column. These controls do not widen Fountain eligibility or imported PDF access.

A selector choice or Ctrl+1–8 changes the whole current element's type, even
when only part of its text is selected. It retains authored text, emphasis, identity, deliberate
breaks, caret and selection direction. It is one undoable edit; choosing the
current type does nothing. A selection crossing element boundaries is refused
with an explanation; choose one element at a time. No text is replaced.

Page-break markers and title fields cannot become body text through the selector
or Tab. Use their existing dedicated controls. A type change inside either
speech of a dual pair requires explicit unpairing first, following the owner's
[dual-dialogue policy](dual-dialogue.md). Ordinary incomplete speeches can still
be authored and saved; the existing diagnostics and export refusals apply.

With suggestions closed, Tab retains its caret-only six-type cycle: heading, action, character, parenthetical,
dialogue, transition. Shift+Tab reverses it. From lyric or centered text, either
Tab direction returns to action. An open writing suggestion menu gives Tab
acceptance priority; Shift+Tab dismisses and cycles backward. See
[task 4a](writing-build-plan.md#task-4a--context-restricted-writing-suggestions).
Enter retains the existing element rules:
splitting text within lyric/centered keeps its type, and Enter at its end makes
a following action. These are bounded authoring conventions within the
[build plan](writing-build-plan.md#editing-and-assistance-contract); full
selection/clipboard conventions are now in the [task 3f contract](selection-and-clipboard.md);
spelling, find/replace and broader assistance remain separate work.

## Verification and retained limits

The new editor checks were first run against the missing controls and failed.
They now exercise the real selector and keys, every direct shortcut, backward
selection and Unicode/break/style/ID retention, no-op undo behavior, Undo/Redo,
live labels, the six-type Tab cycle, protected structures, lyric/centered Enter,
and reachable controls beside the expanded outline. A captured synthetic-window
image was inspected. Project-flow checks author the independent fixture through
the real menu and shortcuts, save, reopen and export. The process/oracle checks
compare fresh-process exports and bounded reimport with the hand-written
Fountain and hand-derived PDF slots/columns; aw-parser independently reads the
styles and types (with its existing lyric-as-italic-action representation).

The existing stale-title-form test now simulates an external adapter changing
a field's actual type directly. It still verifies atomic rejection and Undo;
Tab no longer provides that unsupported way to turn title fields into body text.

Native checks on 2026-11-04 use Qt's installed Compose input plugin:
`QT_QPA_PLATFORM=wayland QT_IM_MODULE=compose QSG_RHI_BACKEND=opengl`.
The editor suite passes 108 checks with three optional input checks skipped;
the compositor scene shortcut passes separately. Project flows pass 44 checks.
The task's wtype smoke passes all eight shortcuts, ordinary typing, Tab,
Undo/Redo, exact saved identities, autosave/reopen and independent exports.
The standard fcitx preflight is unavailable because fcitx5 is absent here.
With the default Wayland text-input-v3 context, rapid test-window replacement
emitted `enableSurface` focus warnings and failed the suite's warning checks;
Compose is an explicit tested input-path deviation. These results do not
validate fcitx integration. Arch's wtype 0.4-2 executable was extracted under
`build/native-tools/` for the compositor checks; no system package changed.

Task 3f records [selection/clipboard rules](selection-and-clipboard.md) beyond
this single-element type command. A standalone shortcut reference, everyday
assistance, broader Fountain compatibility and imported-PDF validation remain
pending. Owner comfort/appearance acceptance and
sustained typing/export performance were not reevaluated. Existing memory and
crash-durability limits remain, including the known heavy-I/O crash-loss limit.

The complete isolated release and address/undefined/leak-sanitizer suites pass
**24/24** each; engine-only passes **2/2**. Final review also reproduced and fixed
a checkmark disappearing when choosing the active type: the menu restores its
live binding after unchanged/refused commands. All four affected editor,
project-flow, process and oracle checks pass again in both builds, and native
menu/no-op/protection checks pass again. No remaining in-scope review finding.

A concurrent sanitizer/editor workload produced a 1,146 ms maximum crash-loss
observation against the unchanged 1,000 ms target. The known heavy-I/O limitation
is retained; the complete isolated release suite passes without that competing
workload. Detailed results and log locations are recorded in the
[task record](writing-build-plan.md#task-3e--element-type-controls).
