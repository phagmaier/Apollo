# Case in scene headings and character cues

2026-10-28, task **3a**: a bounded first step of slice 3's case contract. It covers
scene headings and character cues in writing, layout, PDF, import admission and
clean Fountain export. The title page, transitions and the generated `(CONT’D)`
keep their earlier behavior.

## Confirmed rule

The owner confirmed: *apply element spacing and indentation automatically, and
uppercase character cues and scene headings as normal editing behavior… Do not
silently rewrite dialogue or action. Exact diagnostics and exceptions remain to
be specified* ([spec](spec.md), [planning](planning.md)). The accepted
spike behavior is that the document keeps what was typed and shows these
elements in capitals ([spike brief](spike-brief.md)).

## Authored text and presented text

| Term | Meaning |
| --- | --- |
| **Authored text** | The element text in the project: what was typed, pasted or imported, in its own case. Saved, autosaved, recovered, copied and undone exactly. |
| **Presented text** | What Apollo shows and prints. For scene headings, character cues and the title, ASCII `a`–`z` become `A`–`Z`. Nothing else changes: other letters, digits, punctuation and every other element type are shown as authored. |

`presented_text()` in `engine/layout.h` is the one definition. The engine lays out
and the PDF prints presented text. The editor draws the same capitals through
character formats on the authored text, so the caret, selection, emphasis and
input methods work on what was typed. The opening scene list and the editor's
line dump use the same function. Because only ASCII bytes change, byte offsets,
emphasis spans and wrapping widths are the same for both texts.

Consequences for writing:

- Typing `int. depot - day` in a heading shows `INT. DEPOT - DAY`; the project
  keeps `int. depot - day`. Editing, undo/redo and Tab type changes act on the
  authored text, and the presentation follows the current type. Dialogue and
  action are never changed.
- A letter outside ASCII, such as é in `JOSé`, is shown as authored, so it is
  not in capitals. Apollo never rewrites the text to hide this; it reports it
  where an output depends on it (below).

## Fountain export (`fountain-canonical-2`)

Clean Fountain writes headings and cues as presented, so a reader sees what
Apollo shows and prints. `int. depot - day` is written `INT. DEPOT - DAY` and
`mara (v.o.)` `MARA (V.O.)`. Headings without a standard prefix are still
`.`-forced and cues starting with punctuation are still `@`-forced. When any
heading or cue was authored otherwise, the export panel, the completion status
and the command line say so, for example: `Written in capitals, as Apollo shows
and prints them: 1 scene heading and 2 character cues typed in lowercase. The
project keeps the case as typed.`

A heading or cue whose presented text would still contain a lowercase letter (é)
is refused with its element and letter named. The title must still be written in
ASCII capitals. See [fountain-export.md](fountain-export.md).

## Fountain import (`writer-subset-2`)

New editable copies are admitted under `writer-subset-2`, which differs from
`writer-subset-1` only in case:

- **Admitted:** headings in any ASCII case (`int. house - day`, forced
  `.flashback`), and cues whose lowercase letters are only in extensions
  (`EVE (cont'd)`, as in the official Dialogue sample). The authored text is
  retained and presented in capitals. The imported original keeps its exact bytes.
- **Still review-only:** a cue name with lowercase letters, which Fountain only
  allows when forced (`@McKay`). Its author wants mixed case printed, and the
  owner has not specified that exception, so it is reported as `forced
  mixed-case character name requires review`. Also review-only: a letter that
  cannot be presented in capitals (é) in a heading or cue, and a title not in
  capitals.

Copies made under `writer-subset-1` keep being validated by their own rules, which
required capitals; relabelling a copy cannot widen what it admitted, and an older
Apollo refuses `writer-subset-2` copies instead of misreading them.

## Owner decisions (2026-10-28)

The owner accepted these as the initial case policy
([planning round 24](planning.md#round-24--case-policy-for-headings-and-cues-settled-2026-10-28)):

1. Fountain export writes headings and cues in capitals, as Apollo shows and
   prints them, not in authored case; the project keeps authored case and the
   export discloses the change. Forcing authored case with `@` was rejected
   because other applications would then print `mara` where Apollo prints `MARA`.
2. Forced mixed-case names (`@McKay`) stay blocked until an explicit, faithful
   mixed-case exception is designed. Showing them in capitals would silently
   change what their author printed.
3. Imported lowercase headings and cue extensions are presented in capitals,
   like typed ones.

## Evidence

| Check | Independent expectation |
| --- | --- |
| `project_access_flow_tests` `typed_case_is_kept_and_presented_in_capitals` | Real keys in the Writing desk: a lowercase heading, an edit and its undo, a lowercase cue. Glyphs are compared with Courier Prime's glyphs for the capital text; autosave and reopen keep the typed case; the panel discloses it; the export through real controls and after reopening equals hand-written bytes |
| `project_access_flow_tests` 2.6d flow | A cue typed `Mara` in an import copy is eligible, is disclosed, and exports as the hand-written `writer-subset-edited.fountain` |
| `editor_tests` `typed_continuation_is_shown_once` | Typing ` (CONT'D)` or ` (cont'd)` after a cue that G1 continues: the authored suffix is kept as typed, hidden under one generated `MARA (CONT’D)` at the hand-derived column and slot, with no stray ink |
| `editor_pdf_tests` `authored-lowercase-contd` | Fixture 08 saved with the continued cue authored `mara (cont'd)` and other cues in lowercase prints exactly the hand-derived `08-dual-dialogue-and-contd.expected`, with the continuation once |
| `editor_tests` `presentation_capitalizes_ascii_letters_only` | A heading authored `int. café - day` draws Courier Prime's glyphs for `INT. CAFé - DAY`, and the opening scene list shows the same text |
| `editor_pdf_tests` `authored_lowercase` | A saved project with fixture 01's headings and cues in lowercase: `--check`, editor/PDF agreement, and a PDF identical to the hand-derived `01-element-geometry.expected` |
| `fountain_conversion_tests` | `fixtures/import/case-subset.fountain` converts to the hand-written authored model; `writer-subset-2` admits it with exact original bytes; a `writer-subset-1` relabel and `@McKay`, `int. café`, `@JOSé` are refused with their reasons |
| `fountain_export_tests` | Hand-written spellings for lowercase headings, forced headings, opening keys, extensions, dual and emphasized headings; located refusals for é; disclosure counts; the case model exports to hand-written `fixtures/export/case-subset.fountain`; the 40,000-snapshot explanation property still holds |
| `fountain_export_process_tests` / `fountain_export_oracle_tests` | `--edit-fountain` of the case fixture, real typing with autosave, export in a new process equal to hand-written text with the disclosure; authored source and original bytes unchanged; `@McKay` publishes nothing. aw-parser 0.1.1 reads the export and the hand-written file as the authored model with headings and cue names in capitals |

Interoperability observed with aw-parser 0.1.1: it prints lowercase scene
headings in capitals, as Apollo does, but reads `MARA (cont'd)` as action,
unlike Fountain 1.1 and its official Dialogue sample. Exported capitals are read
as cues by both.

`writer-subset-4` / `fountain-canonical-4` additionally preserve six punctuation
marks **— ‘ ’ “ ” …**, including an authored curly-apostrophe `(CONT’D)`.
Punctuation does not require case conversion. This does not relax the title's
ASCII-capitals restriction, the lowercase-letter gate in headings/cues, or the
owner's forced mixed-case-name policy.

## Remaining limitations

- Letters outside ASCII in headings and cues are not shown in capitals in the
  editor or PDF; Fountain export and import refuse them. Unicode case mapping
  can change length (ß → SS), so it needs its own layout and offset contract.
- There is no mixed-case exception (`McCLANE` prints `MCCLANE`), and no
  diagnostic or suggested correction for case yet.
- The title page, other title fields, transitions and centered text keep their
  earlier case behavior; the title must be authored in capitals for Fountain.
- PDF export of imported writing remains blocked pending independent layout
  evidence. Other import limits of the writer subset are unchanged.
- Not verified: owner comfort with the presentation and disclosure wording,
  and find/replace or suggestions, which do not exist yet and will need to
  choose between authored and presented text.
