# Independent import examples

`independent.fountain` is original Apollo test material (2026-10-28), written
from the owner requirements and the official syntax, independently of the
implementation. It deliberately combines valid syntax and malformed material.
No private/reference screenplay was copied into it.

Hand-worked expectations for subsequent conversion checks:

- A title page can begin with Draft date. Retain the unknown field separately.
- The lowercase heading is a heading; `12A` is production metadata.
- Forced action includes WATCH THE DOORS and its following lines. Keep its
  line breaks and the notice's four leading spaces.
- McKay is a mixed-case cue with an extension. Its speech has three authored
  breaks, including the two-space empty dialogue line. JUNE is its dual partner.
- The action has literal asterisks around star, bold on bold, underline on
  underline, and unchanged Unicode café. These are not byte-offset assertions.
- One complete note and one complete omitted passage are candidates for
  named read-only note/stash conversion. The tags in the note are literal text.
- Section/synopsis are organization; centered End of Act One is printed text;
  five equals signs are a manual page break.
- Ellipsis is action. CUT TO followed by spaces is action. Unpaired telephone
  asterisks are literal punctuation. The final unclosed note must not consume
  writing across the empty line or disappear silently.

Task 2.6a only verifies exact-byte intake, provenance/review hints, immutable
reopen and output refusal. It does **not** claim these editable conversion
expectations pass. `fountain_intake_tests` exercises those storage contracts;
the compatibility table tracks the remaining conversion/export work.

Task 2.6b's `interpretations.expected.json` makes the hand-worked text/type/dual
expectations executable for this example and all 16 official parser samples.
The expected element sequences are written from the source examples and
[official syntax](https://fountain.io/syntax/), independently of Apollo's parser
output. `fountain_conversion_tests` compares whole sequences, retained record
counts and unresolved codes; focused examples independently check styles,
byte/code-point locations, record text and associations. The legacy `I./E.`
heading and lyrics within speech have explicit unresolved expectations, rather
than being waved through as supported. This tests interpretations, not PDF
layout or a Fountain import/export round-trip. The original whitespace-only
dialogue line and trailing transition spaces are intentional test data.

## Bounded writer subset

`writer-subset.fountain` is original test writing. Its `.expected.json` was
hand-written from the intended types, words, title fields, dual relationship and
code-point emphasis spans, independently of the converter. The
`.records.expected.json` gives independently worked record text, associations,
section depth and original byte/line locations. The byte counts were checked
with a separate Python byte listing, not generated from Apollo output.

Together these cover every saved screenplay type, all six title fields,
Unicode é, literal escaped punctuation, bold/italic/underline combinations,
deliberate action/dialogue breaks, paired speeches, named note/stash, unknown
title metadata, section/synopses and production number associations. Profile
slots/columns for the authored breaks are independently specified in the QML
flow test. LF and BOM/CRLF variants use the same screenplay expectations while
preserving their different original bytes and stash line endings.

`fountain_conversion_tests` compares the complete model and retained records;
`project_access_flow_tests` uses real QML controls and keyboard events through
editing, undo/redo, autosave/reopen, detached associations, Save a Copy and
explicit recovery from a damaged source. The actual executable process tests
independently compare saved JSON to these expectations after typing and SIGKILL
at four save phases, followed by normal app reopen/resume. See the
[preservation contract](../../docs/editable-fountain-subset.md) for admission
limits and remaining export/grammar/layout blockers.
