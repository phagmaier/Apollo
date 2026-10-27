# Apollo test fixtures

These fixtures check Apollo's layout, export, import, and performance against expectations that do not come from Apollo's own engine. The profile they assume is [docs/export-profile.md](../docs/export-profile.md) (Academy sample profile v1).

## Layout fixtures — `layout/`

Eight small original scripts, each a `.fountain` file paired with an `.expected` file. The expected files were derived by hand from the profile rules and use original text only. Their line wrapping was cross-checked with `tools/afterwriting-oracle.sh`.

| Fixture | Checks |
| --- | --- |
| `01-element-geometry` | Every element's wrap width, hanging parenthetical, cue extension, emphasis that does not count toward width, lyric, transition, centered text, heading spacing |
| `02-dialogue-split` | Speech split at a mid-line sentence end, `(MORE)`, `NAME (V.O.) (CONT’D)`, continuation rewrapped |
| `03-heading-keep-with-next` | Heading alone at the bottom; heading whose unsplittable action follows |
| `04-dialogue-push` | Speech moves whole when a split would leave one line |
| `05-parenthetical-at-break` | Speech moves whole rather than leaving a parenthetical behind |
| `06-action-split` | Action split at the latest sentence end leaving two lines on each page |
| `07-title-page-and-manual-break` | Title-page placement, title page excluded from numbering, `===` break, body page 1 unnumbered |
| `08-dual-dialogue-and-contd` | Generated `(CONT’D)` after action, none after another speaker, dual columns, dual block moved whole |

`.expected` format: a line `page <n> number=<text|none>` starts each page (the title page is `page title`). Each following line is `slot<TAB>element<TAB>text[<TAB>styles]`, with blank slots omitted. Slot 1 is the first body line, 1.0 in from the top. Element x positions come from the profile's geometry table, so the files do not depend on the pending dialogue-column choice. Dual-dialogue lines carry `-L`/`-R`, and title-page lines carry their alignment.

How Apollo should pass:

1. Export each fixture to PDF.
2. Run `uv run tools/pdf_lines.py out.pdf` to dump its lines.
3. Map each line's x position to an element through the profile.
4. Require an exact match on page, slot, element, and text.

The spike also compares the editor's own line dump with the same expectations.

### What the afterwriting cross-check does and does not confirm

[afterwriting](https://github.com/ifrost/afterwriting-labs) (MIT) is an independent Fountain-to-PDF formatter. `tools/afterwriting-oracle.sh` installs version 1.17.3 into `~/.cache/apollo/afterwriting` with install scripts denied, and patches its US Letter profile to the Academy widths: 56 lines, dialogue 35, parenthetical 17. Checked on 2026-10-27:

- It reproduces 345 of 349 Academy body lines, and every expected line in fixtures 03, 04, 05, and 07.
- It differs by design where Apollo's profile follows the Academy rather than afterwriting:
  - it counts emphasis markers toward width (fixture 01);
  - it splits speeches at any line rather than at sentence ends (02 and 06);
  - it does not generate `(CONT’D)` after action, and uses its own dual-dialogue widths (08);
  - it puts one blank line after `FADE IN:` before a heading.
- Use it to catch wrapping mistakes and to estimate page counts. It is not a page-break authority.

## Academy reference — `../reference/academy/` (local only, gitignored)

This holds Final Draft's real output for the 11 body pages of the Academy guide. The guide's text belongs to the Academy, so it stays outside version control.

1. Download the [format guide PDF](https://www.oscars.org/sites/oscars/files/scriptsample.pdf) in a browser; the site blocks scripted downloads.
2. Save it as `reference/academy/scriptsample.pdf`.
3. Run `uv run tools/academy_reference.py`. It writes `expected.tsv`, every line with page, slot, and element, and `academy.fountain`, a transcription.

A copy was saved during planning on 2026-10-27.

Apollo should reproduce every line's text and slot, page by page, with these expected deviations:

- **Page 5 top:** Final Draft draws the `(CONT’D)` cue above slot 1; Apollo puts it on slot 1.
- **Page 6 → 7:** Final Draft breaks one speech early for no rule-based reason; Apollo keeps `JOE / How did we reach…` on page 6.
- **Page 7:** the guide's extra blank lines after the centered date have no Apollo equivalent yet.
- **Page 8 → 9:** `APRIL` is a Final Draft "Shot" line; as Apollo action it fits on page 8, slot 56.

Because these deviations shift later content, compare each page starting from the reference page's first line.

## Fountain syntax samples — `fountain-syntax/`

These are the official Fountain parser test files from [nyousefi/Fountain](https://github.com/nyousefi/Fountain) (MIT; see `fountain-syntax/LICENSE`). Each one covers one construct: boneyard, notes, dual dialogue, forced elements, sections, synopses, scene numbers, page breaks, transitions, centered text, and indenting. Use them for import coverage and for the unsupported-construct report. They have no expected layouts.

The same repository includes *Big Fish* (John August), a real full-length screenplay. Keep it in `reference/big-fish.fountain` as a local realistic import and performance input; it is gitignored. It renders to 124 pages with the oracle.

## Generated workloads — `generated/` (create on demand)

`tools/generate_script.py` writes deterministic synthetic scripts from an original word list. The same arguments always produce the same bytes. Each script has every element type, including dual dialogue, lyrics, transitions, centered text, and manual breaks.

```sh
uv run tools/generate_script.py --scenes 175 --seed 1 > fixtures/generated/feature-200.fountain   # 201 pages (oracle)
uv run tools/generate_script.py --scenes 444 --seed 1 > fixtures/generated/stress-500.fountain    # 501 pages (oracle)
```

Page counts are afterwriting's; Apollo's may differ by a few pages because its split rules differ. The outputs are gitignored, since they can be regenerated.

## Your own scripts

Your existing Fountain scripts are the best real-world import test. Copy them into `reference/own/`, which is gitignored and stays local. A professionally exported PDF of any of them would add a second page-break reference.
