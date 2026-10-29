# Independent Fountain export expectations

Original test writing (2026-10-28) for the
[canonical Fountain export](../../docs/fountain-export.md). Each file was written
by hand from a semantic model and the contract's spelling rules before the
serializer produced any output; none is captured from Apollo.

| File | Semantic source |
| --- | --- |
| `writer-subset.fountain` | [`../import/writer-subset.expected.json`](../import/writer-subset.expected.json), the hand-worked model of the imported subset fixture, exported with its title page |
| `writer-subset-edited.fountain` | That model after the QML flow test's real-key edits: JUNE's speech broken after “At the”, then `INT. DEPOT - DAY`, `Mara waits.` and a `MARA` speech `Here.` appended |
| `writer-subset-edited-no-title.fountain` | The same, exported without the title page |
| `case-subset.fountain` | [`../import/case-subset.expected.json`](../import/case-subset.expected.json), whose headings and cue extensions are authored in lowercase, written with headings and cues in capitals as the [case contract](../../docs/case-handling.md) presents them |
| `spacing-subset.fountain` | [`../import/spacing-subset.expected.json`](../import/spacing-subset.expected.json), written as the [spacing contract](../../docs/spacing.md) presents it: no space at a line edge, the blank dialogue line as two spaces, repeated spaces in action and dialogue as authored |
| `punctuation-subset.fountain` | [`../import/punctuation-subset.expected.json`](../import/punctuation-subset.expected.json), preserving the six admitted Unicode punctuation marks and code-point emphasis after a curly apostrophe, with an authored break and dual pairing |
| `scene-spacing.fountain` | [`../import/scene-spacing.expected.json`](../import/scene-spacing.expected.json) after an explicit normal-scene-spacing choice and type-only corrections of the two cue/parenthetical-only speeches to action; text, styles and IDs remain authored, but neither spacing nor speech meaning is claimed unchanged |
| `emphasis-authoring.fountain` | [`../import/emphasis-authoring.authored.expected.json`](../import/emphasis-authoring.authored.expected.json), with keyboard/menu-authored bold Café, italic bells and underlined red; words, authored breaks and identities unchanged |

The original imported Fountain spells the same meaning differently (`!` before
plain action, notes, stash, sections, synopses, scene number and an unknown title
field). The clean exports omit those retained records; the tests check the
disclosure of what was left out.

`fountain_export_tests` and `project_access_flow_tests` compare Apollo's output
with these bytes. `fountain_export_oracle_tests` reads them, and the process
test's own exports, with aw-parser 0.1.1, an independent parser, and compares
its reading with the semantic models above. The process test derives its typed
expectation by hand from `writer-subset.fountain`: the final action gains the
typed text `the lamp turns and t`.
