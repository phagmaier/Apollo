# Apollo source format and saving (version 1)

2026-10-28. Status: implemented in [writing-build-plan](writing-build-plan.md)
slice 1 and slice 2 tasks 2.1–2.3. This is the working contract for the first usable
writing build, not yet a long-term compatibility promise: later slices may add files or fields
under a new format version. The owner chose a portable project folder with
versioned structured text; the details below are agent engineering choices.

Code: `source/screenplay_format.*` (schema), `source/project.*` (folder and
file replacement), `source/recovery.*` (recovery packages),
`editor/screenplay_layout.cpp` (Qt document adapter), and
`editor/autosave.*` (saving). Tests: `tests/source_tests.cpp`,
`tests/autosave_tests.cpp`, `tests/autosave_crash_tests.py`,
`tests/project_lock_tests.py`, and `tests/recovery_tests.cpp`.

## Project folder

| Path | Contents |
| --- | --- |
| `project.json` | Format name and version, persistent project identity, and the screenplay file name |
| `screenplay.json` | The screenplay: title-page fields, ordered elements, emphasis, deliberate breaks, dual-speech flags, and revision |
| `.recovery/` | Complete local recovery packages and their integrity/timestamp metadata; exclude from future Git history |
| `.apollo.lock` | Runtime edit lock; released on normal exit, recovered by process identity after a crash; exclude from future Git history |

Autosave changes the authoritative `screenplay.json` and adds bounded recovery
packages under `.recovery/`. The import flow does not yet create the plan's
`imported-material.json` or `imports/original.fountain`. If the two import files
are present, opening retains their bytes in memory and Save a Copy preserves
them verbatim (including empty files).
Unreadable or symlinked dependencies are refused rather than silently omitted;
the `imports` folder must also be an ordinary directory. Import parsing and
its read-only reader remain pending. Session state and library recents will
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
A copy made by Save a Copy gets a new one. `screenplay` is a plain
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
   while the newest acknowledged revision is the current one and no destination
   change has been detected; **Saving…**
   otherwise. Input-method preedit text is not part of the document and is
   saved once committed.
4. A failed write shows **Not saved** with the reason and a Retry save button,
   leaves the previous file in place, and keeps the writing in the window.
   For ordinary I/O failures, later edits keep trying; a later successful save
   clears the failure. Destination changes instead pause saving as described below.
   Reaching the version-1 revision limit also fails visibly before any file
   replacement: keep the last valid file and newer in-memory writing rather
   than wrap/reset the revision or claim an unreadable file is saved. Reaching
   this limit is only practical with an artificially high starting revision.
5. Closing with unsaved writing waits asynchronously for saving. A project-save
   failure attempts an emergency copy and shows its exact coverage and location,
   with Retry, Save a Copy, Keep Open, and an explicit protected-close or discard
   choice. Repeated window-close requests never implicitly discard. See task 2.3
   below. Destruction drains pending writing through the same destination guards;
   paused writing is preserved to emergency storage rather than force-saved.

Measured results are below. Session resumption, damaged-save selection on open,
and stale temporary cleanup remain later slice 2 work. Recovery packages can
already be opened directly as ordinary projects.

### Editable sessions and destination changes (slice 2 task 2.1)

`ProjectSession` acquires `.apollo.lock` **before reading** the manifest or
screenplay. The editor transfers this session to the saver and retains it until
the writer finishes. Read-only `open_project()` remains available to tools/tests.
Directory symlinks and relative aliases resolve to the same canonical folder;
the session keeps that destination even if an alias is subsequently retargeted.
Manifest and screenplay symlinks/hard links are refused for editing because
separate project locks would not protect shared files.

Lock acquisition is nonblocking: a competing editable open fails with the
location and available process/host information. It does not open another editable
window or take over. `QLockFile::setStaleLockTime(0)` disables expiration by age
while retaining dead-process recovery. On Linux, an open directory descriptor
anchors the lock through `/proc/self/fd/…`, so moving the project retains its
lock and cleanup cannot remove a lock in a replacement folder at the old path.
Focusing an existing window remains part of the later open-project flow.
[Qt QLockFile contract and limitations](https://doc.qt.io/qt-6/qlockfile.html).

The session remembers exact manifest/source bytes and filesystem identities
(device/inode), the folder identity, and its lock. The ordered worker checks
these before writing and after the temporary write immediately before commit.
After commit it verifies the new bytes and records their identity; it checks
again after the commit hook. Filesystem notifications request worker checks
while idle; a one-second poll covers unavailable or lost watches. File watches
are rearmed after Apollo's own replacements. File contents are read on the
worker, not in a notification callback on the GUI thread.
[Qt watcher replacement behavior](https://doc.qt.io/qt-6/qfilesystemwatcher.html).

A changed/deleted source or manifest, missing/moved/replaced folder, or lost lock
changes status to **Not saved**, explains that saving is paused, and retains all
writing and undo history in the window. Neither subsequent typing, queued saves,
Retry, nor destruction may overwrite the changed destination. Retry checks the
original baseline; it never accepts external bytes as permission to overwrite.
Restoring the same original folder/files allows an explicit Retry to resume,
including saving edits made while that retry was running. Recreating a missing
source with a different inode is still a replacement and is refused.

The error offers Retry, Save a Copy and preserved-work Reload. Task 2.3 below
adds recovery snapshots and the emergency/close dialog.
There is no automatic reload, merge, force-save, or durable recovery promise for
writing kept only in memory after a destination change until a copy succeeds.

**Race boundary:** a noncooperating writer can still change a file between the
last comparison and `QSaveFile::commit()`'s rename. The filesystem offers no
compare-and-replace guarantee here; post-commit checking cannot reconstruct an
external version overwritten in that interval. Live simultaneous external editing
is unsupported. Locks coordinate cooperating local processes, not arbitrary
writers, tools deleting/replacing lock files, other machines, or differing
container filesystem/PID views. Keep these limits explicit; passing deterministic
injected-race checks does not prove every external race is prevented.

### Save a Copy (slice 2 task 2.2)

The Writing desk offers **Save a Copy…** and **Ctrl+Shift+S**, with an absolute
new-folder path inside an existing parent folder. Keep Writing/Escape dismisses
the destination form without creating anything. An accepted operation captures
the current writing, creates a complete staged project on the ordered worker,
and publishes it without replacing any existing destination, including an empty
folder or symlink. Import dependencies come from the opened session's cached
bytes, so a moved/missing/changed original does not prevent a complete copy.
This is a copy of the supported project files, not an arbitrary folder backup;
locks, recovery files, unrelated files, and future Git history are not copied.

The new project has a new UUID and a revision sequence starting at zero.
Element identities, text, title fields, emphasis, and deliberate breaks are
retained. Before activation, acquire its edit lock and verify the saved content
and dependencies against the requested snapshot. A changed or locked copy is
not activated. If publication succeeded but activation failed, retain the
published folder and report its location; do not delete potential external work.
The old session remains active on failure, including its paused-saving state
when its destination has changed.

On success release the original lock, switch autosave, show the canonical active
folder under the save status, and update the PDF default destination. Keep the
same Qt document, cursor, selection, and undo/redo. The copy's completion covers
its captured writing; later edits show Saving until committed there. Creating
another copy is disabled while one is running. A failure reopens the destination
form with its reason and retains in-memory writing and the old location.

An already-running old save finishes under its original destination checks;
queued old writes are superseded by the copy snapshot. Writing continues during
copying; after activation immediately capture edits made during the operation.
A close during copying waits for the copy and those edits, and failure keeps
the window open. Destruction also drains later writing at the appropriate
destination. No partial staged folder is published; ordinary failures clean up
the staging folder. SIGKILL can still leave staging material; stale cleanup
remains pending. Task 2.3 below adds emergency/recovery storage.

Later edits wait in memory while the copy worker is busy. Crash-loss/latency
during a slow copy has not been measured, and this does not extend the earlier
healthy-storage autosave measurements to copying or unavailable storage.

Verification on 2026-10-28: release `mise run check` passes 10/10,
`mise run check-debug` passes 10/10 (ASan/UBSan, `detect_leaks=0`), and
`mise run check-engine` passes 2/2. After the final copy review/test extensions,
the rebuilt debug autosave and source suites pass 42/42 and 72/72; final focused
offscreen selection/cancellation/alias checks also pass. The final native
autosave suite passes 42/42 with the following setup:

```sh
WAYLAND_DISPLAY=wayland-1 XDG_RUNTIME_DIR=/run/user/1000 \
QT_QPA_PLATFORM=wayland QT_IM_MODULE=fcitx QT_QUICK_BACKEND=software \
QT_FORCE_STDERR_LOGGING=1 build/release/autosave_tests
```

Use the actual owned compositor socket for other sessions, as selected by
`tools/check-wayland.sh`. The first launch without `WAYLAND_DISPLAY` aborted
inside Qt platform initialization before opening a project. A subsequent run
without the runner's fcitx configuration raised text-input-v3 focus warnings
and was stopped; that configuration is not validated by the passing run.
Native execution used approved session access because the sandbox cannot
connect to the compositor. This verifies synthetic native saving/copy behavior,
not physical input, OpenGL performance or owner comfort. Leak detection and
new representative-workload/copy-crash measurements remain unverified.

Saved logs: `build/task-2.2-{release,debug,engine}.log`,
`build/task-2.2-debug-final.log`, `build/task-2.2-final-focused.log` and
`build/task-2.2-native-final.log`. Inspected screenshots:
`build/release/autosave-tests/copy-{active,failed}.png`.

### Preserved-work Reload and recovery/emergency copies (slice 2 task 2.3)

`source/recovery.*` writes immutable complete project packages through staging
and no-replace publication. Each includes the screenplay, a project manifest,
cached imported material and original Fountain when present, and `recovery.json`:
format `apollo-recovery`, version 1, origin UUID/location, UTC capture timestamp,
reason (`autosave`, `emergency`, `reload`), and SHA-256 hashes of the packaged
files. A package is acknowledged only after a complete readback and integrity
check. It has its own project UUID and retains the captured revision and element
identities. Open its reported folder with `apollo_editor DIR`; no original drive
is required. Saving an emergency copy never switches the active project or
marks the original destination Saved.

Healthy saves keep the opened source before its first replacement and a recovery
package after each completed save under the project's `.recovery/`. Recovery
I/O stays on the ordered worker. A failed local snapshot does not invalidate a
successful authoritative save; the status reports the snapshot failure. Retain
the newly completed package, the newest previous capture, and up to ten older
UTC minute buckets. Always keep the package being acknowledged even after a
wall-clock rollback. Pruning runs only after a new package is verified, skips open/locked projects, and leaves
invalid/unrecognized packages and hidden interrupted staging as evidence. Failed
cleanup can exceed the nominal bound. Before pruning, acquire the package's edit
lock and recheck its integrity; an editor may have opened, changed and closed it
since the scan. Atomically move it to a hidden `.apollo-recovery-pruning-*` name
before recursive deletion so an opener cannot race removal of the lock file.
Interrupted pruning can leave hidden material. Future stale-temp cleanup must
respect these distinctions.

After a failed save, and on later bounded captures while destination checks are
paused, attempt a complete emergency package under
`$XDG_DATA_HOME/Apollo/recovery/<key>/` (normally
`~/.local/share/Apollo/recovery/<key>/`). The key hashes the original project UUID
and canonical location. Imports come from session memory, independently of a
missing/moved/changed original. Recovery stores use a nonblocking local lock;
both destinations may fail. The GUI acknowledges the captured revision, time
and path; later edits immediately cease to be described as currently protected.
The visible save-status binding is notified as soon as that coverage changes.
An older copy's success never covers newer writing. The same retention rule
applies to emergency storage.

**Reload…** opens a confirmation explaining the replacement and undo reset.
**Preserve and Reload** first writes and verifies the current window's complete
emergency package, then reads a valid candidate from disk while holding the
original edit lock. It revalidates before accepting a new baseline. The project
UUID and folder/lock must still match the editable session. Changed source and
manifest references can be adopted explicitly; a different project UUID requires
an open/copy flow. The editor is temporarily disabled during this asynchronous
operation. Changes from another document producer cancel adoption and preserve
the newer memory; Retry cannot overwrite a baseline accepted by a cancelled
Reload. Missing/malformed/newer-version data or failed preservation retains the
current document and undo history. Success installs the candidate in the same
Qt document, clears undo/redo, resumes autosave and reports where prior writing
was preserved. A later PDF export replaces that footer notice with its own
progress/result. No external bytes are rewritten by Reload itself.

A failed close shows the project error and whether an emergency package covers
**current** writing. Retry finishes closing only after the original saves; Save
a Copy uses task 2.2; Keep Open/Escape keeps writing. A protected close says
**Close with Emergency Copy**. Otherwise the explicit action says
**Discard Unprotected Writing and Close**. Repeating the window-close request
never substitutes for that choice. Closing during copy/Reload waits
asynchronously for the operation. As before, final saver destruction drains its worker; blocked storage
can delay process teardown.

The damaged-save picker, recovery reconciliation on project open, last-project/
cursor resumption, and stale-stage cleanup remain in the project-access work.
There is no automatic rollback. Recovery packages preserve evidence and can be
opened directly in the meantime. Leak detection and crash loss during Save a
Copy remain unverified; healthy-save measurements do not establish the loss
window during slow copying or unavailable storage.

Task 2.3 verification (2026-10-28): serial release and ASan/UBSan checks pass
11/11; engine-only passes 2/2. Leak detection was disabled as before. After the
final status-notification review fixes, release and debug autosave checks each
pass 48/48; native Wayland passes the full 48/48 and the final focused controls
7/7 using the same fcitx/software configuration documented for task 2.2. These
are synthetic native interaction checks, with no new physical-input, OpenGL,
latency or comfort claim. The final Reload confirmation also passes offscreen
in both builds, and its screenshot was inspected alongside both close choices.

The extended crash checker now kills before/after recovery snapshots as well as
the existing save phases and at random times. Python's independent JSON/hash
checks validate every published package and compare writing with the input log.
Run the two representative workloads sequentially, without competing test I/O:

```sh
QT_QPA_PLATFORM=offscreen UV_CACHE_DIR=/tmp/apollo-uv-cache \
uv run tests/autosave_crash_tests.py --editor build/release/apollo_editor \
  --source fixtures/generated/feature-200.fountain \
  --output build/task-2.3-crash-200 --keys 150 --random 12 --phase-occurrence 4
```

Repeat with `stress-500.fountain` and output `build/task-2.3-crash-500`.
Release, internal SSD/btrfs, 150 synthetic keys 30 ms apart, 20 kills per size:

| Measure (ms) | 200 pages (3,271 elements) | 500 pages (8,184 elements) |
| --- | --- | --- |
| Lost writing p50 / p95 / max | 215 / 412 / 473 | 420 / 708 / 712 |
| Key to committed save p50 / p95 / max | 257 / 429 / 601 | 404 / 616 / 731 |
| GUI capture p50 / max | 3.5 / 5.0 | 9.5 / 11.6 |
| Source commit p50 / p95 / max | 46 / 60 / 77 | 55 / 75 / 242 |

All 40 kills retained complete valid source; published recovery packages also
passed independent integrity/content checks. One package per size was reopened
through `apollo_editor DIR --check` without altering writing/evidence bytes.
Interrupted hidden stages are retained for later stale cleanup.

An earlier required-suite run concurrent with the sanitizer workload exceeded
the target on the small script: 1,861 ms lost while source commit blocked for
about 1,737 ms. The serial required suite and isolated workloads pass. This is
a measured I/O-contention limitation, not evidence of a universal one-second
bound. No power-loss or unavailable-storage guarantee is added, and copying
crash loss remains unverified.

Logs: `build/task-2.3-serial-release.log`, `build/task-2.3-final-debug.log`,
`build/task-2.3-engine.log`, `build/task-2.3-review-{release,debug,native}.log`,
`build/task-2.3-native.log`, `build/task-2.3-reload-ui.log`,
`build/task-2.3-debug-reload-ui.log`, `build/task-2.3-academy.log`, and
`build/task-2.3-reopen-recovery.log`. Crash summaries/events are in
`build/task-2.3-crash-{200,500}/`; the concurrent failure is saved in
`build/task-2.3-final-release.log`. Screenshots:
`build/release/autosave-tests/{reload-confirm,close-protected,close-unprotected}.png`.

Pre-commit review reran the final code: release and ASan/UBSan each pass 11/11,
engine-only passes 2/2, and release/debug autosave suites each pass 50/50.
The native Wayland autosave suite also passes 50/50 with the same fcitx/software
setup above. Both newly added status regressions failed before their fixes.
Retention coverage also verifies that corrupt/edited evidence survives later
pruning; the concurrent opener/pruner ordering was checked in code, without a
test forcing every race. Logs: `build/recovery-review-{red,green,release,debug,engine,native}.log`.
Leak detection remains disabled; the earlier representative workload timings
were not repeated, and no new performance or physical-input claim is made.

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
`build/review-fixes/measure-{200,500}.txt`. These timing runs did not verify
native Wayland.

### Task 2.1 verification (2026-10-28)

Release passes 10/10, debug ASan/UBSan passes 10/10, and engine-only passes 2/2.
Debug used `ASAN_OPTIONS=detect_leaks=0`; leak detection remains unverified.
In this sandbox, tool caches were redirected with
`MISE_CACHE_DIR=/tmp/apollo-mise-cache UV_CACHE_DIR=/tmp/apollo-uv-cache`.
The independent hand-derived fixtures and local Academy agreement also passed:

```sh
PYTHONDONTWRITEBYTECODE=1 uv run tests/pdf_tests.py \
  --exporter build/release/pdf_export --probe build/release/layout_probe \
  --output build/slice-2-locking/independent-pdf --academy
```

The actual native Wayland `autosave_tests` suite passed 30/30 (log:
`build/slice-2-locking/native-autosave.log`). The sandbox could not connect to
the compositor; this check ran with approved native-session access. It used
Qt Quick software rendering and synthetic test input. Native saving behavior
is verified; new native latency, OpenGL rendering, physical-input timing and
owner comfort were not assessed here.

The 18-kill workload per size from the earlier command was repeated with the
new session checks, concurrently with other verification. All 36 runs retained
complete valid source. Maximum lost writing was 259.1 ms at 200 pages and
288.5 ms at 500 pages; these are offscreen application-crash checks, not a
power-loss guarantee or a controlled performance comparison. Logs and summaries:
`build/slice-2-locking/crash-{200,500}`. Dead processes intentionally leave
`.apollo.lock`; the next editable open recovers it.

Direct review covered requirements, implementation and test necessity. The idle
and during-save regression checks failed against the original saver; a later
failing regression exposed and fixed missed captures during slow Retry. All
added checks passed after the fixes. See the plan's task 2.1 evidence table for
the specific competing-instance and destination-change scenarios.
