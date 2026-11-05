# Find and replace (task 4c)

The [writing requirement](spec.md#writing-and-formatting) and
[editing plan](writing-build-plan.md#editing-and-assistance-contract) require
explicit find/replace that preserves surrounding writing and provides one-step
Undo for Replace All. These are this bounded step's engineering conventions.

| Control | Behavior |
| --- | --- |
| Ctrl+F / Edit → Find | Open the modal search panel, focusing Find. A single-line opening selection seeds the query; otherwise retain this editor's previous query. |
| Ctrl+H / Edit → Find and Replace | Open the same panel, focusing Replace with. |
| Find | Literal, non-overlapping matches of authored text, ignoring case by default. No regex, escapes, normalization or accent folding. A match stays on one authored line in one element; automatic wrapping and page gaps do not divide source text. |
| Match case | Compare authored spelling, including the original case of headings, cues and title text. Default search also finds these when presentation capitalizes them. |
| Whole words | Require both adjacent Unicode characters to be outside letters, numbers, combining marks and underscore. Apostrophes and hyphens are boundaries: `cat` matches `cat's`, but not `scatter`, `cat2` or `cat_`. |
| Whole script (default) | Search all authored body text and structured title fields in document order. Skip page-break markers and generated MORE/CONT'D text; imported-material reader, notes and stash are separate. |
| Opening selection only | Search inside the range captured when opening the panel, with either selection direction. Complete matches must fit within it; whole-word boundaries still use the original surrounding text. Navigating results does not replace the captured scope. Replacements expand/shrink its tracked endpoints. Disabled without an opening selection. |
| Next / Previous | Select and reveal a match without taking focus from the panel. Wrap at either end. A query starts at/after the current search position (initially the opening caret), wrapping to the first when needed. |
| Enter / Shift+Enter in Find; F3 / Shift+F3 in the panel | Next / previous. Never accept a writing suggestion or edit screenplay text. |
| Replace | Explicitly replace the selected current result, then refresh and select at/after that replacement's end. Empty replacement deletes only the matched text. |
| Replace All | Replace the current query's complete matches inside its scope. One Undo restores every changed text run, emphasis, identity and flag; Redo reapplies the operation. Replacement text is not searched again during that operation. |
| Replacement formatting | Retain each element's type, identity, dual flag and all surrounding text/emphasis. Replacement text inherits the first matched character's emphasis. It can empty an element but never remove it, join elements, insert a marker, or change speech associations. Existing export eligibility/diagnostics apply to the resulting writing. |
| Escape / Close | Return focus to the editor, keeping the last selected result. Query/options remain transient in that editor; changing projects creates a new search session. |

Fields accept a single line of at most 4096 UTF-16 units, without control
characters, tabs, paragraph/line separators or Qt's reserved frame characters.
They do not insert new elements or deliberate breaks. Existing authored breaks,
blank dialogue lines and line-edge spaces remain untouched; searching for spaces
uses authored spaces, including those presentation hides. Literal `$`, `\` and
other punctuation have no special replacement meaning. Identical replacements
do not create an undo entry. Invalid operations preserve writing and undo state
and display their reason.

The modal panel dismisses writing suggestions and blocks desk shortcuts such as
export, title editing, Home and F1 while open. It refuses opening during active
input-method composition. Qt retains ownership of composition, ordinary field
editing and the screenplay's persistent selection. Title fields remain separate
and page breaks/dual structure cannot be crossed by a replacement.

`FindReplace` owns transient state per document. Scans yield after at most 128
elements, 512 candidate matches or approximately 4 ms of work, using 8192-unit
chunks in long paragraphs. A document change clears published results and
restarts; a new query or close cancels pending work. Navigation and replacement
recheck the document revision. No source styling is used for search marks:
the current result uses the editor's existing selection.

More than 100,000 matches requires a narrower query/selection; no partial result
set can be replaced. A replacement operation adding more than 1,000,000 UTF-16
units is refused before editing. Replace All itself is synchronous within one
Qt edit block; bounded scanning does not establish a typing-performance result
or make that edit block asynchronous. Sustained writing/export performance and
owner comfort remain separate milestone checks.

Verification evidence and retained limits belong to the
[task record](writing-build-plan.md#task-4c--find-and-replace).
