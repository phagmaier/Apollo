# Fountain compatibility and intake

Investigated 2026-10-28 against the checked-in official samples and
[official Fountain 1.1 syntax](https://fountain.io/syntax/). Sample provenance and
MIT terms remain in [fixtures/README.md](../fixtures/README.md) and
[LICENSE](../fixtures/fountain-syntax/LICENSE); upstream is
[nyousefi/Fountain](https://github.com/nyousefi/Fountain).
The independent original example and hand-worked semantic expectations are in
[fixtures/import](../fixtures/import/README.md). Private reference files were
not changed or incorporated into fixtures.

## Result and scope

Task 2.6 is larger than one reviewable import change. The spike importer is not
a safe production converter: it joins source lines with spaces, trims action
indentation and transition-significant trailing spaces, accepts ellipses as
forced headings, lacks forced mixed-case cues and several heading forms, rejects
notes/organization, and does not implement Fountain's emphasis fallback rules.
Existing engine/editor gaps for deliberate breaks and case preservation also
prevent claiming faithful editing or PDF. There is no Fountain serializer yet.

**2.6a implements lossless intake and read-only review only.** The whole original
is pending conversion, including ordinary screenplay text. The preview labels
findings as candidates; it is not a parser and never grants a clean compatibility
result. Publishing retains the exact previewed bytes, even if the external file
later changes or disappears. The complete byte copy, SHA-256 and provenance are
validated before publication and on reopen. Invalid encodings use a hex view.
Review-only projects cannot enter the editor, autosave writing, or export a
screenplay through Apollo's project/instrumentation paths.

This is a useful preservation/reopen checkpoint, **not completion of task 2.6**.
No notes/stash are declared converted and no editable round-trip promise is made.
The existing unsaved Fountain spike tools remain limited research/instrumentation
paths; they do not become the new project import workflow.

**2.6b now has two implemented reviewable subtasks: located interpretations and
explicit interpretation copies.** `source/fountain_conversion.*` derives a
versioned screenplay interpretation, named read-only note/stash records,
organization/metadata associations, unresolved ranges and separate layout
limitations. `fixtures/import/interpretations.expected.json` contains hand-worked
complete element/text/dual expectations for every official sample and the
independent original example. These are semantic expectations, not layout
oracles. The interpretation reader replaces candidate hints with types, named
records and exact diagnostics when viewing the new version.

The result is still an **interpretation review**, not editable import acceptance.
No imported project enters the writer yet: even a source with no syntax errors
has not passed the independent edit/save/reopen/Fountain-export gate, and the
existing layout cannot faithfully represent all imported breaks/spacing/case.
The stored output policy and actual project paths block editing and both clean
exports. The limited spike CLI paths retain their explicit research scope.

### Conversion revision 1 and migration

- Element envelopes and contributing text slices use zero-based half-open
  original byte ranges, including any BOM/CRLF; one-based first/last occupied
  lines accompany them. Normalized text uses LF for authored breaks. Emphasis
  spans use the existing byte-in-memory/code-point-on-disk contract. Source
  slices describe contributing runs before emphasis decoding; they are not a
  per-character editor cursor map. Element envelopes locate normalized breaks.
- Notes and stash retain the exact UTF-8 interior text, including CRLF and
  spaces, with deterministic names (`Imported note 1 (line N)`), independent
  record IDs and source anchors. Anchors prefer a containing/preceding body
  element, or the following body element for leading material; byte provenance
  remains authoritative. Notes/stash are outside screenplay elements.
- Sections retain depth and the nearest preceding shallower section as parent,
  including skipped levels. Synopses anchor to the latest section or scene.
  Scene numbers retain exact values and heading IDs. Unknown title fields
  retain the whole field/continuations with their key and source range. None
  of these are silently turned into printed acts or screenplay text.
- Complete notes stop at a paragraph boundary unless a blank line contains at
  least two literal spaces. Unclosed notes quarantine only that paragraph;
  unclosed boneyards quarantine to EOF. Nested delimiters, orphan/escaped
  delimiters, invalid encoding/control characters, crossing/unsupported emphasis,
  unpaired duals and ambiguous opening title keys have located unresolved
  records. Known unmatched emphasis stays literal. Literal fallback text and
  incomplete relationships may appear in the interpretation, with output blocked.
  Crossing, unequal delimiter runs and intraword emphasis are conservatively
  reported with literal text; this is a tested subset of Markdown emphasis.
- The older sample's `I./E.` spelling is retained as action with a diagnostic;
  it is not silently accepted as a published heading prefix. Lyrics within
  speech retain lyric elements but flag the missing source-v1 speech relationship.
  Forced headings require a single dot directly followed by an alphanumeric.
- Action tabs/spacing, extra blank lines, authored case and deliberate breaks
  remain preserved in source and/or interpreted text, with located layout
  limitations. This does not establish faithful PDF layout or glyph coverage.
- Revision 1 is bounded to **1 MiB / 100,000 source lines**. Larger intakes keep
  their full original (up to the unchanged 16 MiB intake limit) and get one
  unresolved whole-source `conversion-limit` range. Interpretation metadata is
  bounded to 64 MiB at publication/reopen, without truncating stored records.
  The UI caps its display list at 200 and labels clipped excerpts; stored data
  and complete original are independent of that display cap.

Migration is explicit **Interpret in a New Copy**, available from a preview or
an existing review-only reader. It writes a new staged project with a fresh
identity; it never upgrades an existing project in place. Existing readers
first revalidate their locked project and retained evidence. Failure keeps the
old reader/lock/session state; success opens the new verified review copy. An
activation failure retains the published copy/path for inspection. The external
file is not reread during this operation. The CLI equivalent for a new capture
is `apollo_editor NEW_PROJECT --interpret-fountain FILE`; an existing destination
is refused. `--retain-fountain` continues to create the older intake version.

Project manifest v2 `import-review` remains the mode. Intake metadata v1 remains
unchanged/readable. New copies use metadata v2 `interpreted-review`, embed the
complete conversion revision 1, and carry the original bytes, origin, length and
SHA-256 unchanged. Reopen validates the evidence and compares the complete stored
interpretation against the frozen revision-1 converter. Unknown/newer fields or
revisions are refused; an older 2.6a build refuses metadata v2 instead of dropping
it. Copy/recovery retains both assets. Future converter revisions must preserve
the old reader/algorithm or explicitly require a new build, and migrate through
another copy. Manually changing a version, permission flag or stored result
never grants editable access. The frozen revision includes stored names,
diagnostic wording and output policy; changing those also requires revision
dispatch rather than invalidating existing interpretation copies.

## Compatibility table

The target column records the agreed milestone obligation plus selected canonical
export treatment. Everything in this table remains read-only source in 2.6a;
intake does not assert editable support. PDF refers to a future converted
screenplay, not a PDF of the source reader.

| Construct / evidence | Required editable conversion and Fountain re-export | PDF treatment / current gap |
| --- | --- | --- |
| Scene headings: SceneHeaders | Recognize boundary/context, all official prefixes and case, forced single dot; preserve ellipsis as action; force headings on canonical export when needed | Apply owner uppercase default without losing authored source case; incomplete in spike |
| Action: MultilineAction, Indenting | Preserve authored breaks, action indentation, tab intent and extra vertical spacing; explicit `!` where ambiguous | Whitespace/layout fidelity not established |
| Cues/extensions: Dialogue, ForcedElements | Context-sensitive cues; mixed-case `@`; letters outside extensions; preserve authored extensions and continuation labels; force mixed case on output | Resolve current uppercase/generated CONT'D behavior without duplicating authored labels |
| Dialogue/parenthetical: Dialogue | Preserve single speech with deliberate breaks and whitespace-only dialogue lines, including style boundaries; distinguish separator from empty speech line | Source storage supports breaks; layout/keyboard contract still incomplete |
| Dual: DualDialogue, Dialogue | Preserve paired speeches and second-cue caret, including valid zero/multiple-space markers; reject/surface unpaired or ambiguous relationships | Existing fixture is narrower than complete authoring/import contract |
| Lyrics: ForcedElements | Explicit tilde per lyric line; preserve lyric-in-speech intent for review | Current parser treats a tilde inside dialogue as text |
| Transitions: Transitions, Indenting, Simple | Honor blank boundaries and trailing-space disambiguation; use forced `>` for canonical transitions | Existing trim-based parser loses distinctions |
| Centered/act text: CenteredText | `>text<`; preserve intended printed label; never infer act label from a section | Existing centered rendering has limited independent fixtures |
| Emphasis/literals: independent example, layout 01 | Bold/italic/underline combinations, escapes, literal punctuation, unmatched delimiter fallback; byte/code-point offsets must agree after decoding | Existing emphasis parser removes unmatched markers then throws |
| Title page: Simple, independent example, layout 07 | All supported fields independently optional, Authors alias, multiline indentation, field emphasis; canonical known-field spelling | Existing parser requires Title first; future title form still pending |
| Unknown title fields: independent example | Retain separately with provenance; omit from canonical clean Fountain and disclose omission | Exclude from screenplay/PDF |
| Manual breaks: PageBreaks, Simple | Three or more equals only; canonical `===`; preserve surrounding content and empty pages | Existing importer recognizes exactly three |
| Inline/multiline notes: Notes, Simple | Convert to named read-only notes with text and source location; omit from clean Fountain and disclose conversion | Excluded from screenplay/PDF; 2.6a only locates candidates |
| Boneyard: Boneyard, Simple | Convert omitted passages to named read-only stash; retain exact text and provenance; omit from clean Fountain and disclose conversion | Excluded from screenplay/PDF; delimiter ambiguity needs explicit handling |
| Sections/synopses: SectionHeaders, Sections-Complex, Synopses, Simple | Preserve hierarchy/text outside screenplay; omit from canonical clean Fountain and disclose omission | Excluded; not printed acts |
| Production numbers: SceneNumbers | Retain value and source association separately; clean Fountain omits unsupported production data with explicit disclosure; no equivalent production-export claim | Excluded from submission PDF; production tools deferred |
| Unicode/encoding: independent and byte-level tests | Preserve valid Unicode and original encoding bytes; require explicit conversion for non-UTF-8; no replacement/deletion on decode | Font coverage needs separate diagnostics; no transliteration |
| Malformed/ambiguous: independent and byte-level tests | Retain affected source, exact location and reason; resolve before claiming affected clean output | Block affected clean export; 2.6a blocks all screenplay export |

Canonical treatment of retained-only organization/metadata is an engineering
choice under the delegated compatibility contract: omit with disclosure, retain
in the project and immutable original. It does not imply equivalent round-trip
of those constructs. Supported screenplay round-trip must still prove types,
text, emphasis, extensions, deliberate breaks, dual relationships and title
fields through an independent parse after edits/save/reopen/export.

## Official sample inventory

Every `.fountain` sample in the repository is exercised by the intake suite.
These are intake assertions, not layout or complete parser acceptance.

| File | Evidence to protect in conversion work |
| --- | --- |
| Boneyard | Inline, whole-line and multiline omitted text, including interior spaces |
| CenteredText | Trim padding inside centered delimiters; distinguish forced transition |
| Dialogue | Lowercase extensions, repeated CONT'D, parentheticals, numeric non-cues, two-space empty line and indentation |
| DualDialogue | Two speeches and the second cue's caret |
| ForcedElements | Forced multiline action, mixed-case cue and lyric lines within speech |
| Indenting | Indented transition/cue/parenthetical/dialogue, preserved action spacing |
| MultilineAction | Deliberate action lines, internal uppercase line that remains action |
| Notes | Whole-line, multiline and inline notes |
| PageBreaks | Both three and nine equals signs |
| SceneHeaders | Prefix variants, case, context and ellipsis; sample `I./E.` is outside the current published prefix list and needs explicit interpretation |
| SceneNumbers | Numeric, mixed-case, dashed and dotted numbers |
| SectionHeaders | Spaced/unspaced headers and nested depth |
| Sections-Complex | Hierarchy, synopses and scene boundaries together |
| Simple | Mixed document: title, lower-case heading, four-equals break, notes/stash, forced heading, ellipsis and trailing-space nontransition |
| Synopses | Spaced/unspaced synopsis with sections |
| Transitions | Standard/forced transitions; nontransition punctuation and trailing spaces |

## Remaining subtasks and acceptance

- **2.6b.1 / 2.6b.2 implemented:** located conversion model, named note/stash,
  metadata associations, hand-checked corpus interpretations, precise unresolved
  ranges and explicit new-copy migration/reader. Original bytes stay immutable.
  This begins 2.6b; it does not declare the full Fountain grammar complete.
- **2.6b remaining:** extend semantic coverage for syntax outside the exercised
  corpus and resolve diagnostics through an explicit correction workflow.
  Reassess the conservative conversion limits using measured realistic inputs.
  Editable activation requires a separately tested preservation contract for
  edits and imported associations, plus the authoring/layout work below. Keep
  the gate closed until that evidence exists.
- **2.6c / slice 3:** complete authoring/layout contracts, title form, case,
  spacing and deliberate breaks; expose unresolved imported material in context.
- **Slice 3/5 compatibility gate:** canonical Fountain serializer, independent
  parsing of import → edit → save/reopen → export, semantic expected fixtures
  and output-specific blocking/disclosure. Correct PDF still needs independent
  layout references, not just agreement with Apollo's engine.

The published syntax has examples that exceed the older parser samples; neither
corpus alone is exhaustive. Intake tests cannot establish font fidelity,
editing, note conversion, export compatibility, power-loss durability, or import
performance on representative full-length scripts.
