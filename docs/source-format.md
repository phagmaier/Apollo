# Apollo source format and saving (version 1)

2026-10-28. Status: implemented in [writing-build-plan](writing-build-plan.md)
slice 1. This is the working contract for the first usable writing build, not
yet a long-term compatibility promise: later slices may add files or fields
under a new format version. The owner chose a portable project folder with
versioned structured text; the details below are agent engineering choices.

Code: `source/screenplay_format.*` (schema), `source/project.*` (folder and
file replacement), `editor/screenplay_layout.cpp` (Qt document adapter), and
`editor/autosave.*` (saving). Tests: `tests/source_tests.cpp`,
`tests/autosave_tests.cpp`, and `tests/autosave_crash_tests.py`.

## Project folder

| Path | Contents |
| --- | --- |
| `project.json` | Format name and version, persistent project identity, and the screenplay file name |
| `screenplay.json` | The screenplay: title-page fields, ordered elements, emphasis, deliberate breaks, dual-speech flags, and revision |

Autosave changes only `screenplay.json`. The plan's `imported-material.json`,
`imports/original.fountain`, and `.recovery/` are not created yet; they arrive
with import and recovery (slice 2). Session state and library recents will
live outside the project folder.

`project.json`, exactly these keys:

```json
{
  "format": "apollo-project",
  "version": 1,
  "id": "0d3c7a52-1f0e-4f43-9a3e-2f6f3cb1f7a4",
  "screenplay": "screenplay.json"
}
```

`id` is a random UUID in lowercase canonical form, created with the project.
A copy made by Save a Copy gets a new one (slice 2). `screenplay` is a plain
`.json` file name in the folder: no path separators and no leading dot.

Creating a project writes and validates the complete folder under a hidden
`.apollo-staging-XXXXXX` name beside the destination, then renames it with
`renameat2(RENAME_NOREPLACE)`. An existing destination, even an empty folder,
is never replaced or reused; a failure removes the staging folder.

## Screenplay file

```json
{
  "format": "apollo-screenplay",
  "version": 1,
  "revision": 42,
  "titlePage": [
    {"id": "t1", "field": "title", "text": "The Last Broadcast"}
  ],
  "elements": [
    {"id": "e2", "type": "action", "text": "Café lights flicker.", "styles": [{"start": 5, "end": 11, "style": "italic"}]},
    {"id": "e5", "type": "character", "text": "ANNA", "dual": true}
  ]
}
```

[`fixtures/source/v1-example.screenplay.json`](../fixtures/source/v1-example.screenplay.json)
is a complete hand-written example covering every type, line breaks,
non-ASCII and non-BMP text, and styles; the tests compare Apollo's output with
it byte for byte.

| Key | Rule |
| --- | --- |
| `format`, `version` | `"apollo-screenplay"`, `1` |
| `revision` | Whole number, 0 to 2^53 − 1. Increases with every saved change, across sessions; values beyond this range cannot be written |
| `titlePage` | Title-page fields in order. `field`: `title`, `credit`, `author`, `source`, `contact`, `draftDate`. A field may repeat, such as several authors or contact lines |
| `elements` | Screenplay elements in order. `type`: `sceneHeading`, `action`, `character`, `parenthetical`, `dialogue`, `transition`, `lyric`, `centered`, `pageBreak` |
| `id` | Required, unique across both lists; 1–64 of `A–Z a–z 0–9 _ -` |
| `text` | Required string; may be empty. `\n` is a deliberate line break. U+2028/U+2029 and unpaired surrogates are invalid |
| `dual` | Optional, only on `character`: `true` pairs this speech with the preceding one |
| `styles` | Optional list of `{"start", "end", "style"}` with `style` `italic`, `bold`, or `underline` |

Style offsets count Unicode code points (not UTF-8 bytes or UTF-16 units),
with an exclusive end: `0 ≤ start < end ≤` the text's code-point count. A
code-point offset can never fall inside a character. Spans of different
styles may overlap.

Unfinished or currently unexportable writing is valid source: empty elements,
a character cue without dialogue, control characters, or text typed into a
`pageBreak` element (an editor limitation the plan schedules for slice 3) all
save and reopen unchanged. Layout errors never block saving.

### Canonical form and serialization

Apollo writes one canonical form, so equal screenplays give equal bytes:
title fields before elements, each list in screenplay order; spans sorted by
`(start, end, style)` with overlapping or touching spans of one style merged;
`dual` only when true; `styles` only when non-empty; keys in the order shown;
one element per line, so a later Git diff shows one changed element as one
changed line. Strings escape `"`, `\`, `\n`, and other control characters;
everything else is written as UTF-8. The file ends with a newline.

### Reading and versions

Reading validates the whole file before use and never writes. A file with a
higher `version` is refused as needing a newer Apollo, before any other check,
so this build cannot rewrite it. Anything else invalid — bad JSON, a missing
or unknown key, an unknown type, a repeated id, an out-of-range span — is
refused as malformed with its location. Unknown keys are refused rather than
dropped because a later save would lose them. Reading accepts non-canonical
span order and merges spans, so a reopened file may save with different bytes
but the same meaning.

## Mapping to the editor

Each element is one `QTextBlock`. Its type, dual flag, and id are block-format
properties, so undo/redo restores them with the text. Emphasis is character
formatting. In memory, `apollo::Element` style offsets are UTF-8 bytes like
the engine's; the reader and writer convert to and from code points.

A deliberate line break is `\n` in element text and `QChar::LineSeparator`
(U+2028) inside the block. `tests/autosave_tests.cpp` (`line_separator_probe`)
checks what this relies on: the separator stays inside its block as one
cursor position, `QTextBlock::text()` keeps it while `toPlainText()` turns it
into `\n`, selection and undo keep it, and `QTextLayout` always ends a line
after it, even when told the line is longer. The engine does not yet break
lines at `\n`, so the editor reports a layout error for such text. Enter,
Shift+Enter, and external text paste still create elements rather than
deliberate breaks. Engine, PDF, Fountain, and keyboard support for deliberate
breaks are slice 3 work.

Copy/cut and paste between Apollo selections preserve an existing deliberate
break inside its element, along with its type and emphasis. Clipboard offsets
use the same U+2028-to-`\n` mapping as the saved text. External plain-text paste
still follows the spike's paragraph rules pending slice 3.

Title-page fields are laid out on the title page wherever they appear in the
document; on save they move ahead of the body in their own order, which does
not change any page.

### Identity

- Import and new elements get a random 64-bit id written as 16 hex digits.
- A split (Enter inside an element, or pasting between two parts of one)
  keeps the id on the part before the split; the other part gets a new id.
  When the part before is empty, as with Enter at the start of an element,
  the part with the text keeps the id instead.
- A join (deleting across an element boundary) keeps the earlier element and
  its id, as Qt keeps the earlier block's format.
- Pasted elements are new elements, including Apollo-to-Apollo paste and
  cut/paste. A whole-scene move keeps every id. Undo and redo restore ids.
- A block that Qt splits without an editor command, such as an input-method
  commit containing a newline, would repeat its id. The document repairs later
  repeats synchronously on Qt's `undoCommandAdded` signal and joins the repair
  to that originating edit. Undo/redo restores the same ids; captures only
  read the document and never repair it or change its undo/redo history.

These rules are a seam for later history and links; no feature depends on
them yet, and branch or shared-material semantics are not solved here.

## Saving

`Autosaver` implements the plan's candidate protocol:

1. Every document change is a new revision. The first unsaved change starts a
   250 ms timer that later changes do not restart, so continuous typing cannot
   postpone a capture. The GUI thread captures an immutable snapshot of all
   elements.
2. One writer thread serializes the newest captured snapshot and replaces
   `screenplay.json` through `QSaveFile` with direct-write fallback disabled,
   checking the write and commit. A snapshot still waiting when a newer one is
   captured is superseded, so an older revision is never written after a newer
   one.
3. A completed write acknowledges its revision. The status is **Saved** only
   while the newest acknowledged revision is the current one; **Saving…**
   otherwise. Input-method preedit text is not part of the document and is
   saved once committed.
4. A failed write shows **Not saved** with the reason and a Retry save button,
   leaves the previous file in place, and keeps the writing in the window.
   Later edits keep trying; a later successful save clears the failure.
   Reaching the version-1 revision limit also fails visibly before any file
   replacement: keep the last valid file and newer in-memory writing rather
   than wrap/reset the revision or claim an unreadable file is saved. Reaching
   this limit is only practical with an artificially high starting revision.
5. Closing a window with unsaved writing waits for the save, without blocking
   the GUI. While saving is failing, the first close is refused and the
   status says to close again to discard; the second close discards. This is
   an interim form of the agreed close behavior: the emergency copy, Save a
   Copy, and a discard choice that describes the unprotected writing are
   slice 2 work. Destroying the saver writes uncaptured changes and waits for
   the writer.

Measured results are below. Not yet implemented, all planned for slice 2:
recovery snapshots and retention, the emergency location, damaged-save
recovery, Save a Copy, project locking, external-change detection, resuming
the last project and cursor, and cleanup of any stale temporary file.

### Crash-loss measurements

`tests/autosave_crash_tests.py` runs the editor's `--autosave-typing` mode
offscreen, killing it with SIGKILL at each save phase and at random moments,
and reads the saved file independently with Python's `json` module (see its
docstring). ctest runs a short version on the 15-element prototype script.

Results on 2026-10-28, release build, offscreen, projects on btrfs on the
internal SSD. 150 keys at one per 30 ms; one kill at the fourth occurrence of
each save phase plus 12 random kills, 18 kills per workload:

```sh
uv run tests/autosave_crash_tests.py --editor build/release/apollo_editor \
  --source fixtures/generated/feature-200.fountain \
  --output build/release/autosave-crash-feature-200 --keys 150 --random 12 --phase-occurrence 4
```

| Measure (ms) | 200-page primary (3,271 elements) | 500-page stress (8,184 elements) |
| --- | --- | --- |
| Lost writing at kill, p50 / p95 / max | 198 / 243 / 258 | 200 / 294 / 337 |
| Key to committed save, p50 / p95 / max | 190 / 294 / 365 | 212 / 319 / 587 |
| GUI-thread capture, p50 / max | 4.5 / 5.4 | 12.7 / 15.6 |
| Serialize and write, p50 / max | 3.6 / 4.4 | 11.9 / 13.9 |
| Commit (fsync and rename), p50 / p95 / max | 53 / 70 / 129 | 58 / 82 / 320 |

Every kill left a complete valid source with the original elements and a
prefix of the typed keys, including kills after a temporary write and after
its rename; none left a temporary file in the folder. On a tmpfs folder the
commit takes under 0.1 ms, so the commit time is dominated by `QSaveFile`'s
sync to disk.

Offscreen `--measure 300` with and without autosave (project folder versus the
same Fountain file) gave key-to-frame p95 20.4 vs 20.3 ms at 200 pages and
41.1 vs 40.8 ms at 500 pages; maxima rose from 22.3 to 24.8 ms and from 43.7
to 54.2 ms, when a capture coincided with a key.

Limits: these are offscreen synthetic keys timed inside the process, not
native Wayland, physical input, or compositor latency; kills are of the
application only, so power loss, a full disk, and a damaged file are not
covered. The capture is a full copy of the document on the GUI thread every
250 ms while typing; it is the first candidate to change if native typing
measurements with autosave (slice 5) show stalls.

### Review-fix verification (2026-10-28)

The copy/line-break, autosave/redo, and out-of-range-revision regressions were
each reproduced by a failing check before the corresponding fix. The updated
release suite passes 9/9; the debug ASan/UBSan suite passes 9/9; engine-only
passes 2/2. Debug used `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot
run in this sandbox; leak detection remains unverified. The independent Academy
comparison still passes with only its documented deviations.

Repeating the 18-kill crash workload above for each size after the fixes left
complete valid files in all 36 runs. Maximum lost writing was 347 ms at 200
pages and 339 ms at 500 pages. Logs are in `build/review-fixes/crash-{200,500}`.

Offscreen software-rendered `--measure 100` on disposable project copies gave
key-to-frame p95/max of 23.3/24.9 ms at 200 pages and 48.5/52.5 ms at 500 pages,
with no >100 ms stall or missed frame. These checks overlapped the sanitizer
run; they are a regression check, not a replacement for native measurements
or a controlled comparison with the earlier timing table. Timing logs are
`build/review-fixes/measure-{200,500}.txt`. Native Wayland remains unverified.
