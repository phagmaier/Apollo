# Speech-structure diagnostics (task 4d)

The [writing requirement](spec.md#writing-and-formatting) asks Apollo to flag
structural or ambiguous formatting problems with corrections the owner can
apply and undo, without silently rewriting dialogue or action. Before this
step, it confirmed only one rule: dialogue without a character cue. On
2026-11-04 the owner chose this bounded **speech-structure** set and, for
dialogue without a cue, both corrections ([planning round 26](planning.md#round-26--speech-structure-diagnostics-settled-2026-11-04)).
Everything else in this document is an engineering convention for this step.

| Issue | Flagged when | Corrections (each one Undo step; words never change) |
| --- | --- | --- |
| Dialogue without a cue | A run of dialogue/parentheticals that no character cue leads (after action, a heading, a marker, a title field or at the start) | **Add character cue** inserts an empty cue with a new identity just before the run and puts the caret in it, so typing the name uses the normal cue suggestions. **Treat as action** changes only the run's types to action |
| Empty speech | A cue with no dialogue element (existing, task 2.6c) | **Treat speech as action** changes the cue and its parentheticals to action |
| Dialogue that runs together | Two or more dialogue elements in one speech with no parenthetical between them. They look like one speech, but stay separate elements and would merge in Fountain | **Join as one dialogue** joins the run into its first element. Each former boundary becomes an authored line break without emphasis; emphasis, text and the first element's identity remain, and the other elements' identities are retired |
| Incomplete Dual Dialogue | A second-speech dual flag without two complete adjacent speeches (the existing editor warning, now listed per speech) | **Turn Dual Dialogue off** clears only that cue's flag, through the existing toggle |

Not flagged: deliberate line breaks and blank lines inside dialogue,
intentional spaces, a dialogue element that is empty while being written
(the existing empty-speech and dual rules still apply), oversized dual groups
(their existing notice above the page), and every other export-only refusal.
Fountain and PDF eligibility stay governed by their own contracts; resolving
these issues does not by itself make a snapshot exportable. Saving is never
restricted.

## Refusals

Corrections are offered disabled, with the reason in the explanation, when
applying them could change a dual-dialogue relationship:

- Add character cue, when the run is directly followed by a second-speech dual
  cue (the new speech would silently become its partner). Treat as action
  remains available.
- Any correction of a run whose element carries a dual flag.
- Join as one dialogue inside either speech of a Dual Dialogue pair, valid or
  incomplete. Under the owner's dual contract, a boundary join touching a pair
  needs explicit unpairing first; turn it off, join, then pair again.
- Treat speech as action for a dual-related empty speech (existing).
- Turn Dual Dialogue off for a speech in an invalid chain that a following dual
  cue depends on (the existing toggle rule: clear the last one first).

Every request is revalidated against the live document. A resolved, changed
or mismatched issue, or a disabled correction, returns `-1` and edits nothing,
with no undo entry. The desk says the issue changed and asks for a current one.

## Writing desk

When any issue exists, a bar above the page shows **N speech issues…**. It opens
a modal list in document order: kind and the first line of the element. Choosing
one selects its elements in the page (the whole run, or the cue) and shows its
explanation and corrections in the bar. Nothing changes until the owner edits
or presses a correction. After a correction the caret is at the corrected
location. The bar hides when no issue remains. The list is transient UI state
and is recomputed in one linear pass after each document change, as the
empty-speech list was.

`EditorCommands::structureIssues` publishes `{key, kind, id, position, end,
text, explanation, corrections}`; `key` is the kind plus the first element's
persistent id, so selection survives typing elsewhere, Undo and Redo.
`locateStructureIssue` and `correctStructureIssue` take that key. The existing
`emptySpeeches` list remains the empty-speech subset for the export workflow
tools.

## Limits

No new source field, Fountain spelling, import admission or PDF behavior. The
issue list does not include ambiguous Fountain spelling, emphasis, unsupported
characters, case or spacing diagnostics; those remain export-time refusals.
The mixed-case cue exception and blank-spacing advisories remain undesigned.
Verification evidence belongs to the
[task record](writing-build-plan.md#task-4d--speech-structure-diagnostics).
