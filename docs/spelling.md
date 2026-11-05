# US English spelling (task 4b)

The [specification](spec.md) requires potential-misspelling underlines,
replacements on demand and a personal dictionary shared by this computer's
scripts. Spelling never corrects text automatically. Character completion
continues to use only the current script.

| Action or context | Behavior |
| --- | --- |
| Authored text | Check US English, including dialogue, action, headings, cues, lyrics and title fields. Ignore page-break markers and generated page numbers/continuations. |
| Underline | Thin red editor overlay, separate from authored underlining; viewport clipping follows page layout, wraps and scaling. No marks in saved text, clipboard, Fountain or PDF. |
| F7 / Edit → Spelling at caret | Open a US English spelling panel for the word touching the unselected caret. Select an explicit replacement or add the word. No automatic menus while typing. Escape/Close returns to writing. |
| Replacement | Only a displayed dictionary suggestion can replace the queried word. One undo step retains element identities, type, speech pairing, deliberate breaks and matching prefix/suffix styling. |
| Selection / composition | Do not widen a selection to a word, or open during input-method composition. Caret/selection changes, later edits and changed requests discard the old query. |
| Add to personal dictionary | Explicitly store the word for all scripts on this computer. Case variants and straight/curly internal apostrophes count as the same personal word; adding does not teach inflections or alter writing. |
| Personal storage error | Report it in the panel/footer; refuse an unacknowledged addition. Preserve corrupt/unreadable files. The base dictionary continues to check spelling. Retry Add after correcting the problem. |

Words are Unicode letter/mark sequences with internal apostrophes. Punctuation,
digits and deliberate line breaks separate tokens. Tokens over 128 UTF-16 units
are retained without checking or adding. The menu offers at most 12 suggestions
in Hunspell order. Names and screenplay abbreviations may need personal entries;
this is a spelling hint, not a formatting diagnostic.

Checks begin after a 180 ms editing pause. A dedicated worker owns Hunspell and
receives one immutable block at a time. Editing immediately removes obsolete
marks and invalidates results; a complete current generation publishes together.
Only visible underline rectangles become QML items. Sustained typing, physical
input latency, large-document menu latency and memory targets need later
milestone measurements; passing functional checks does not establish those targets.

The local file is `$XDG_DATA_HOME/apollo/personal-dictionary.json`, falling back
to `~/.local/share/apollo/personal-dictionary.json`. It is a UTF-8 JSON array of
normalized words, limited to 1 MiB. Adds take a nonblocking lock, reload/validate
current entries, merge, then atomically commit with QSaveFile. Separate open
editors poll changes every 1.5 seconds; they do not overwrite each other's
additions. Corrupt input is preserved and additions refused. Personal words are
outside screenplay/project storage, Save a Copy and Git; synchronization and
additional languages remain deferred.

The [pinned dependencies and full license notices](../assets/spelling/README.md)
record Hunspell 1.7.2 and SCOWL US English data. Editor configuration downloads
only the hash-pinned engine source if not cached. Runtime spelling is offline.
The engine-only check retains its existing dependency boundary.
