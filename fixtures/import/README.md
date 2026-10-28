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
