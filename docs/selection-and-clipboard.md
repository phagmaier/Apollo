# Selection and clipboard behavior (task 3f)

This bounded slice 3 contract follows the [writing requirements](spec.md#writing-and-formatting)
and [editing plan](writing-build-plan.md#editing-and-assistance-contract).
These are implementation-selected conventions within that work. It does not
complete suggestions, shortcut lookup, diagnostics or Fountain compatibility.

## Editing behavior

Selection ranges use Qt's UTF-16 positions, with either selection direction.
Authored source and emphasis offsets continue to use the existing source format.
Automatic wraps and page gaps are presentation, never selected source text.

| Operation | Result |
| --- | --- |
| Typing, Enter, cut, Backspace/Delete or paste over ordinary selected writing | Replace/remove only that range. Surviving text and emphasis remain; an ordinary partial join keeps the left element's type and identity. Removing a whole leading element retains the surviving next element. Enter then applies the contextual rule below. Each replacement/paste is one undo step. |
| Backspace/Delete inside an element | Qt's normal character/word deletion, including deletion of an authored break; unselected text and its styles survive. |
| Backspace at the start / Delete at the end of an ordinary body element | Join adjacent text, retaining the left element's type. No separator or automatic space is added. Undo restores both elements and identities. Protected boundaries below take priority. |
| Enter inside dialogue | Add a deliberate break inside the same speech, including a blank authored line when used at a line start. At the element's final end, make a following action. |
| Enter in a nonempty cue / speech parenthetical at its end | Make following dialogue. An orphan parenthetical instead makes action. |
| Enter inside other nonempty body text | Split at the caret, keeping the type. The original identity stays with surviving text; a second split part gets a new identity. At the final end, make action except for the speech contexts above. |
| Enter on an empty body element | Change to action; an empty parenthetical under a cue changes to dialogue. No additional block. |
| Shift+Enter | Alias for Enter, retaining the established interaction. It is not a separate always-insert-line-break command. Ctrl+Enter is the dedicated page-break command. |
| Tab / Shift+Tab | Retain the six-type caret cycle; direct type choices refuse cross-element selections. Suggestion-menu key handling belongs to the subsequent assistance task. |
| IME preedit / commit | Qt owns composition; preedit is transient. The existing event filter protects replacement ranges across page breaks, paired-speech structure and title boundaries. No general text-control replacement is introduced. |

Title fields remain separate: replacement or deletion across their boundaries,
boundary joins and Enter on a title are refused with an explanation. Plain text
can replace text inside one field, including deliberate lines; use the Title
page form to add/remove fields or author title line breaks. Apollo MIME paste of
title fields is refused in favor of that form. Copying remains available.

The [page-break](source-format.md#manual-page-break-authoring-slice-3) and
[dual-dialogue](dual-dialogue.md) contracts take priority over ordinary editing.
Mixed marker/text replacement remains refused; marker-only deletion removes
only the marker. A paired speech requires explicit unpairing before structural
changes; text and emphasis within it stay editable. Refusal preserves writing,
selection and undo history. Existing incomplete/unsupported writing stays
saveable and retains its diagnostics and export gates.

## Paste and copy

External text never infers Fountain syntax. A tab becomes one space; CRLF, CR
and U+2029 become paragraph separators. U+2028 is an authored line break.
All other text, line-edge spaces, blank paragraphs and trailing breaks survive.

| Destination / clipboard | Result |
| --- | --- |
| One external line | Insert inline in the current type; pasted text is plain, retaining surrounding emphasis. |
| External paragraphs in dialogue | Insert all lines, including blank/trailing lines, as deliberate breaks in the same speech element. Prefix/suffix, type, identity and existing emphasis survive. No repeated cue or extra dialogue elements. |
| External paragraphs in another body type | One element of the destination type per paragraph, including empty paragraphs. Preserve prefix/suffix as separate elements when nonempty; the original identity stays with its surviving text. U+2028 remains a break within its paragraph. |
| External paragraphs inside one title field | Deliberate lines within that field, with its identity and type retained. |
| Empty clipboard or clipboard without text/valid Apollo data | No change, including to selection or undo. Spaces-only text is a real paste. |
| Apollo copy | Selected text, types, deliberate breaks, clipped emphasis and dual flags in Apollo MIME; plain text uses LF for both element separators and authored breaks. Identities are not copied into MIME. |
| Apollo paste of one element matching the destination type and dual flag | Insert inline, retaining pasted emphasis and the destination identity. |
| Other Apollo body paste | Insert the selected elements between surviving destination halves, retaining their types/emphasis/flags. Newly inserted elements receive new identities. An empty destination is replaced. No synthetic leading/trailing half. |
| Invalid Apollo MIME | Fall back to its external plain text, never its invalid spans or types. |
| Apollo data containing page-break markers | Refuse with the existing explanation; insert markers using their dedicated command. |
| Middle-button primary-selection paste | Use the same paste rules at the clicked position when the platform provides primary selection. Existing refusal on/in dual pairs and markers remains; Ctrl+V is available there. |

Paste can create empty elements, multiline cues or other writing outside the
clean Fountain subset. It remains visible and saveable; export gives its
existing located refusal. Pasting does not widen eligibility or imported PDF
access. Styles on either side of plain text are retained, while the pasted
plain text receives no emphasis. Apollo text carries its selected emphasis.

## Verification

The initial regression run failed for type/whitespace preservation, empty-paste
selection retention and title-boundary replacement. Real editor-key tests cover
all eight body types, Unicode, CRLF/tabs/U+2028, blank/trailing lines, surrounding
styles/identities, reversed replacement through typing/Enter/delete/paste/IME,
protected title ranges, and one-step Undo/Redo. Existing internal-MIME,
invalid-MIME, page-break and dual tests remain in the suite.

An original synthetic [model](../fixtures/editing/clipboard-authoring.screenplay.json),
[Fountain](../fixtures/export/clipboard-authoring.fountain) and
[PDF expectation](../fixtures/editing/clipboard-authoring.expected) specify
two speeches with a blank line and styled suffix. Actual key-driven plain paste,
Apollo copy/paste, autosave/reopen and exports are checked against those
expectations; the process check also exports from a fresh executable and uses
Poppler and independent aw-parser. Agreement between shared-engine outputs is
not the sole oracle.

The new multiline-parenthetical test exposed duplicate interpretation of the
first authored separator and hidden leading spaces in hanging-indent wrapping.
The engine consumes that separator once; independent engine examples cover
ordinary, hidden-space, blank and trailing lines. The editor also guards an
invalid Qt line before accessing it, retaining its existing fallback/error path.

Final check results and environment limits are recorded in the
[task result](writing-build-plan.md#task-3f--selection-and-clipboard-behavior).
No new typing performance, crash-loss, owner comfort or full input-method
compatibility claim follows from these checks.
