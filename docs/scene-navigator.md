# Live scene navigator (task 4e)

The [writing requirement](spec.md#writing-and-formatting) and
[round 22](planning.md#round-22--suggestions-scene-boundaries-and-import-review-settled)
define a scene as its heading and every element before the next heading.
Preamble stays before the first scene. Title fields and project-wide imported
notes, stash and metadata stay separate. The conventions below are engineering
choices for this bounded task.

- **Scenes / Ctrl+\\** shows or hides the live list. Headings use the same
  presented capitals as the page; an empty heading reads **Untitled scene**.
  Duplicate headings remain separate, using their element identities. Adding,
  editing, removing or changing a heading's type, Undo and Redo update the list.
- The caret's scene is highlighted and scrolled into view. Preamble and title
  fields have no current scene. Clicking a row places a single caret at its
  heading, reveals it on the page and returns focus to writing. This navigation
  changes no writing and creates no Undo step.
- Drag the right-hand handle to an insertion line before a scene, or after the
  final scene. Near the list's top/bottom, the list scrolls while the handle is
  held. Row clicks and touch/scroll gestures on the list remain separate from
  handle drags. The order changes only on release; **Esc**, a canceled grab,
  dropping outside the list, hiding the list, a modal panel, leaving the writer
  or any intervening document edit cancels the move.
- A move preserves all elements and their identities, authored text, types,
  emphasis, deliberate/blank lines, page-break markers and dual flags. Speech
  groups stay inside their heading's scene; unfinished writing is retained.
  Existing diagnostics and export eligibility still apply. Preamble, title and
  imported material are never part of a scene move.
- Caret and selection endpoints follow their original elements and UTF-16
  offsets, including reversed selections and endpoints outside the moved scene.
  A selection spanning several scenes still moves only the dragged scene;
  its endpoints remain associated with their original elements, so its resulting
  range follows the new document order. **Alt+Shift+Up** moves the caret's scene
  above its predecessor using the same command and selection behavior.
- Reordering is one Qt Undo step; Redo reapplies the exact writing and identities.
  Ordinary Qt Undo/Redo caret behavior applies. Missing/stale identities, an
  obsolete document revision, or an unchanged order leave writing and Undo
  history untouched. Scene navigation/moves are unavailable during composition
  and while the writer is disabled or covered by a modal panel.

The source schema, import admission and Fountain/PDF contracts are unchanged.
Editable imports retain their immutable originals/read-only material and their
existing PDF gate. The list refresh follows the existing linear document scans;
this task makes no new sustained-performance or crash-loss claim.

Verification and remaining limits are recorded in the
[task result](writing-build-plan.md#task-4e--live-scene-navigator).
