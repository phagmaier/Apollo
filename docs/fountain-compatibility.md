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

- **2.6b:** implement a located conversion model, named note/stash records and
  metadata associations; supported screenplay conversion and precise unresolved
  ranges. Keep original bytes immutable. Replace candidate hints with tested
  interpretations. Choose a migration from review-only projects explicitly.
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
