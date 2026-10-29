# Apollo source format and saving (ordinary v1, review v2, imported writer v3)

2026-10-28. Status: implemented in [writing-build-plan](writing-build-plan.md)
slice 1 and slice 2 tasks 2.1–2.5, bounded import intake 2.6a, and the initial
located interpretation/copy workflow 2.6b. This is the
working contract for the first usable writing build, not yet a long-term compatibility promise: later slices may add files or fields
under a new format version. The owner chose a portable project folder with
versioned structured text; the details below are agent engineering choices.

Code: `source/screenplay_format.*` (schema), `source/project.*` (folder and
file replacement), `source/recovery.*` (recovery packages),
`source/project_access.*` (safe open/recovery decisions),
`source/project_creation.*` (named staged creation),
`source/fountain_intake.*` (lossless intake, integrity and review hints),
`source/fountain_conversion.*` (located interpretations and retained records),
`editor/project_access_flow.*` (home/open/recovery and writer transitions),
`editor/project_library.*` (app-local recents),
`editor/session_state.*` (app-local resume), `editor/screenplay_layout.cpp`
(Qt document adapter), and `editor/autosave.*` (saving).
Tests include `source_tests`, `autosave_tests`, `recovery_tests`,
`project_creation_tests`, `fountain_intake_tests`, `fountain_conversion_tests`, `fountain_intake_process_tests.py`,
`project_library_tests`, `project_access_tests`,
`session_state_tests`, `project_access_flow_tests`, `autosave_crash_tests.py`,
`project_lock_tests.py`, and `project_access_process_tests.py`.

## Project folder

| Path | Contents |
| --- | --- |
| `project.json` | Format name and version, persistent project identity, and the screenplay file name |
| `screenplay.json` | The screenplay: title-page fields, ordered elements, emphasis, deliberate breaks, dual-speech flags, and revision |
| `.recovery/` | Complete local recovery packages and their integrity/timestamp metadata; exclude from future Git history |
| `.apollo.lock` | Runtime edit lock; released on normal exit, recovered by process identity after a crash; exclude from future Git history |

Autosave changes the authoritative `screenplay.json` and adds bounded recovery
packages under `.recovery/`. Task 2.6a creates `imported-material.json` and
`imports/original.fountain` for explicit review-only intakes (see its contract
below). Existing editable projects with legacy opaque import dependencies
retain those bytes in memory, and Save a Copy preserves them verbatim
(including empty files).
Unreadable or symlinked dependencies are refused rather than silently omitted;
the `imports` folder must also be an ordinary directory. Complete editable
import remains pending; the 2.6a reader retains the whole unconverted source.
Session state and recents live outside the portable folder, as documented in tasks 2.4–2.5 below.

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

- New ordinary elements get a random 64-bit id written as 16 hex digits.
  Bounded converted imports start with the frozen interpretation’s `fN` IDs.
- A split (Enter inside an element, or pasting between two parts of one)
  keeps the id on the part before the split; the other part gets a new id.
  When the part before is empty, as with Enter at the start of an element,
  the part with the text keeps the id instead.
- A join between surviving text keeps the earlier element and its id, as Qt
  keeps the earlier block’s format. Deleting an entire leading element including
  its separator keeps the surviving following element’s identity instead.
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

Measured results are below. Safe open, damaged-save selection and session
resumption are documented in task 2.4 below. Stale temporary cleanup remains
later slice 2 work. Recovery packages can also be opened directly as projects.

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

Task 2.4 below adds the damaged-save picker, reconciliation on open and
last-project/cursor resume. Stale-stage cleanup remains pending. There is no
automatic rollback. Leak detection and crash loss during Save a Copy remain
unverified; healthy-save measurements do not establish the loss window during
slow copying or unavailable storage.

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

### Safe project access and resume (slice 2 task 2.4)

`apollo_editor PROJECT` uses the bounded open/recovery screen. A healthy current
save opens directly unless a verified newer revision or same-revision conflict
needs an explicit choice. A missing path is a project-open failure, never an
empty screenplay or a guessed Fountain import. At the 2.4 checkpoint, an existing
`.fountain` file opened unsaved through the spike path; 2.6a replaces ordinary
Fountain opening with the review preview. Instrumentation modes
(`--check`, `--measure`, `--agreement`, `--snapshot`, `--measure-export`,
`--autosave-typing`) and `--create` do not change the owner's resume state.

`ProjectAccess` holds the canonical original's edit lock before reading, including
when loading fails. A competing original lock cannot be bypassed by recovering.
Inspection validates complete packages under `.recovery/` and app-data emergency
storage, including metadata, hashes, source versions and cached import files.
Open/locked, changed, invalid, symlinked or incomplete packages are not offered
as verified alternatives. Invalid and hidden interrupted material is retained,
not cleaned up. Moving a valid project does not sever its recovery relationship:
origin UUID establishes identity even when the recorded origin location is old.
If the manifest/folder is unavailable, saved session identity or an unambiguous
verified origin location can identify emergency copies. Ambiguous identities
are explained rather than guessed. An ordinary file replacing the original
location is retained and can still have identity-matched emergency alternatives.

For a healthy project, compare revisions before wall-clock capture time.
Lower-revision packages and equivalent copies do not interrupt ordinary opening;
they are not deleted by inspection. A higher revision or different writing/import
bytes at the same revision requires a choice. Show current revision, recovery
revision, capture timestamp with timezone, reason, package path and recorded
origin. Timestamps order equal revisions but never establish that a lower
revision should replace a higher one. If current storage is damaged, offer all
available verified alternatives, without selecting one automatically. If none
exists, explain the failure and retain every file; do not install an empty writer.

**Open Current Project Without Recovery** explicitly keeps the authoritative
project. **Recover Selected Copy as New Project** requires an explicit selection
and an absolute new-folder destination in an existing parent. It stages and
validates a complete project with a new UUID, retaining the selected screenplay
revision, element identities and imported assets. It never edits the damaged
project or the recovery package in place. Only successful validation/locking
activates the new location and autosave. A published project that cannot activate
is retained and its path is reported. Collision, stale selection or I/O failure
keeps the chooser, original writing and evidence intact.

Original and selected-package state is checked again before and after staging.
Missing/damaged/unreadable originals are decisions too: restoring or replacing
their manifest/source/dependencies invalidates an already displayed decision.
Retention may retire an offered package before selection; revalidation then
refuses it and asks for a fresh inspection instead of opening stale writing.
Locks coordinate cooperating processes, not arbitrary external writers. The
existing noncooperating-writer race boundary still applies; the checks are not
a filesystem transaction or a power-loss guarantee.

#### Last project and cursor

No-argument `apollo_editor` resumes the last successfully activated project.
Without saved state it shows the same project-folder chooser, not a project
library. Failed opens do not replace the last successful entry. Resume checks
the remembered UUID: a different project at the same folder is not implicitly
adopted. The chooser allows explicitly opening another/moved folder.

State is atomically replaced at
`$XDG_DATA_HOME/Apollo/session.json` (normally
`~/.local/share/Apollo/session.json`), never inside a portable project:

```json
{
  "format": "apollo-session",
  "version": 1,
  "directory": "/absolute/canonical/project",
  "projectId": "0d3c7a52-1f0e-4f43-9a3e-2f6f3cb1f7a4",
  "elementId": "e2",
  "offset": 7,
  "position": 12
}
```

Cursor coordinates count Qt UTF-16 units. Restore the stable element and offset
first; clamp shortened text, or use the bounded absolute fallback if the element
was deleted. Never land inside a surrogate pair. Cursor/document changes trigger
a bounded 250 ms capture, with file I/O on the access worker, not a typing
debounce. Normal teardown also records the final cursor. Save a Copy updates the
remembered destination and UUID only after activation; failure retains the
original resume identity. Cursor state is separate from source-save acknowledgment:
after a crash it may be ahead of saved text and is clamped on reopen.

Malformed, newer-version, unreadable or nonregular state is not overwritten.
The UI reports that resume is unavailable/not saved; explicitly opening a project
can still succeed and autosave its writing. The bad session file remains evidence.
Resume is one app-local last-project entry, not a multi-project library or a
cross-process focus/history service.

#### Verification and retained limits

The four focused suites cover disk decisions, cursor/state boundaries, real
QML controls and actual executable dispatch. The process test uses isolated
app data, confirms a fresh stable-element cursor capture after a SIGKILL restart,
and checks that instrumentation/failed opens leave resume data unchanged.
Its missing-path process case is a bounded liveness smoke; the QML suite checks
the actual failed-open surface and subsequent explicit opening.

| Exercised check | Result |
| --- | --- |
| Final `mise run check` | 15/15, including independent fixtures, editor/PDF agreement, small-script crashes and process locking |
| Final `mise run check-debug` | 15/15 with ASan/UBSan; `ASAN_OPTIONS=detect_leaks=0`, leak detection not validated |
| `mise run check-engine` | 2/2, without Qt/PDF |
| Academy reference comparison | Passes with the four existing documented deviations |
| `tools/check-wayland.sh --preflight` | Native input/compose, export lifecycle, editor/PDF agreement and OpenGL first paint pass; runner now includes project-access controls |
| Final native project-access suite | 11/11, fcitx and software renderer; actual chooser/recovery/resume/copy controls |
| Native executable smoke | Create, type `SmokeAlpha`, save cursor 6, SIGKILL, no-argument restart; typing `X` gives exactly `SmokeAXlpha` |

Direct and independent code reviews covered requirements, locking, stale
decisions, worker teardown, state identity and test quality. Review reproduced
13 failures before correcting damaged-original restoration and non-directory
original discovery. A separate failing regression caught a dangling alias whose
target reappeared with an active editor; the final guard refuses that stale
decision. Publication/activation fault injection verifies retained new projects,
original writing and package evidence. The process restart oracle was strengthened
so the preceding process's already-saved cursor cannot falsely satisfy activation.
No remaining in-scope finding was identified.

Logs: `build/task-2.4-final-{release,debug}.log`, `build/task-2.4-engine.log`,
`build/task-2.4-academy.log`, `build/task-2.4-final-native.log`,
`build/task-2.4-review-{red,green}.log`, `build/task-2.4-alias-red.log`, and
`build/task-2.4-focused-final.log`. Native preflight logs:
`build/wayland-check-20261028-070029-dfaUCQ/`. Launcher smoke evidence and harness
limits: `build/task-2.4-launch-smoke/{results.txt,launcher.log,resumed.png}`.
Inspected chooser screenshots:
`build/release/project-access-tests/{damaged-save,reconcile}.png`.

Finalization review additionally caught a dangling alias regaining a regular-file
target while recovery was displayed. The focused check failed before the fix;
source access now captures/revalidates followed-target state as well as the link,
and `project_access_tests` passes 46/46. Unintended Python bytecode changes were
restored/removed without changing implementation files. Repeated native preflight
logs are in `build/wayland-check-20261028-143711-nyi2Zy/`.

The native smoke used `wtype`, not physical input. Initial harness focus syntax
was incompatible with the compositor; automatic focus was verified instead.
Input sent before the writer settled did not edit; the successful smoke waited
for the writer. Alt+F4 did not close that compositor window, so cleanup used
SIGKILL; this launcher smoke does not establish normal window-close behavior.
The existing close/save suites remain the close contract's automated evidence.

At the 2.4 checkpoint, home/new/open and switching were still pending; task 2.5
below adds them. Import, focus-existing-process IPC and stale-stage cleanup remain
separate work. Leak detection, power loss,
unavailable-storage loss bounds, crash loss during Save a Copy and fresh
representative startup/typing/recovery-discovery performance remain unverified.
The earlier workload timings and I/O-contention limit remain unchanged.
No extended comfort, physical-input latency or new universal crash-loss claim.

### Bounded home/new/open (slice 2 task 2.5)

The home screen lists recent successfully activated Apollo projects, and offers
New Project and Open Project. A no-argument launch still resumes the last
project/cursor from task 2.4; Home does not erase that resume entry. In the
writer, **Home / Ctrl+Shift+H** saves and leaves the project before opening this
screen. Switching uses Home, then new/open/recent selection, not a second editor
or a silent document replacement. Fountain import is explicitly task 2.6.

#### New project and locations

The delegated default is the platform's Documents location plus `/Apollo`,
falling back to `$HOME/Apollo` when no Documents location exists. The missing
default path can be created automatically; other parent locations must already
exist. The form supports a typed parent path and an actual folder chooser.
Folder URLs are converted with `QUrl::toLocalFile`, not stripped as strings.
Folder-chooser cancellation does not submit creation or change writing.

The name is a trimmed single folder component, not screenplay/title-page
content. Empty names, `.`, `..`, separators, control characters and invalid
Unicode are refused; filesystem name-length and permission failures are reported.
Creation starts one empty action element at revision 0. No Fountain is parsed.
Existing files, empty/nonempty folders and dangling links are collisions:
try `Name`, `Name (2)`, etc., up to 1,000 candidates. Display the actual
activated canonical location. Existing parent aliases resolve canonically.

`create_project` remains the single staging/publication implementation:
write and independently read back the complete project beside its destination,
then publish with `RENAME_NOREPLACE`. A destination appearing during publication
fails safely; it is not overwritten or treated as an arbitrary retryable I/O
error. Ordinary failure removes the private stage, not unrelated data.
Interrupted-process stages are retained for the later stale-stage task.
After publication, the UI routes through `ProjectAccess` with the new UUID.
If activation fails, retain the complete published folder and show its location;
do not pretend an empty writer opened or delete potentially useful writing.

#### Recent projects

`$XDG_DATA_HOME/Apollo/recent-projects.json` is a separate version-1
`apollo-recent-projects` object with a `projects` array of `{directory,
projectId}` entries, most recent first, capped at 20. Entries display the
folder name and full location. It is not a source index, directory scan or
cross-process window registry. Record only successful activations, including
Save a Copy; failed create/open/recovery does not promote a project.
Merge under a nonblocking `QLockFile`, with stale-age expiry disabled, then
atomically replace the list. Deduplicate by UUID or canonical location.
Opening a moved folder explicitly updates its existing UUID entry; missing
paths remain visible and can offer identity-matched recovery. A replacement
UUID at an old recent location is refused until explicitly opened.

Every recent/open choice uses the same guarded `ProjectAccess` decisions as
task 2.4. Locks, damaged source, recovery choices and stale evidence are never
bypassed. Malformed/newer/nonregular recents remain untouched and visibly
reported; list or cursor-state failure does not prevent saving screenplay
writing at its active project. Cursor resume is still the last-project entry,
not a persisted cursor for every recent project.

#### Leaving without losing writing

Home retains the live document and temporarily disables editing while flushing
its current revision. The ordered saver drains original saves and local/
emergency recovery work; accepted PDF export and final cursor/list I/O also
finish. After that work, a final saver-worker destination check precedes
worker-side `ProjectSession`/lock teardown. Only its successful release replaces
the writer with Home. GUI teardown does not unlink the project lock on storage.

On a failed save or final check, retain the same writing, selection/cursor and
undo history. Show **Keep Writing**, **Retry and Leave**, and **Save a Copy**.
Retry must protect current writing before leaving. Copy failure keeps the
original writer; successful Copy becomes active as before, then Home can leave
the saved copy. No implicit emergency-only departure and no discard choice are
added to switching. The existing explicit window-close failure choices remain
unchanged. Session/list failure is shown separately after writing is safely
saved; restart may then have stale or unavailable app-local navigation state.

Verification and review results are recorded with task 2.5 in
[writing-build-plan.md](writing-build-plan.md#task-25-evidence-and-review).

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

### Lossless Fountain intake and review (slice 2 task 2.6a)

This is a deliberately bounded first subtask of 2.6, not editable Fountain
import. See [fountain-compatibility.md](fountain-compatibility.md) for every
official sample, independent expectations, canonical export decisions and the
remaining editing/round-trip gate.

Home offers **Import Fountain for Review…**. Choose/type a local file, review
the captured source and candidate findings, then **Retain for Review** in a
named new project. The parent/default-folder and collision naming rules are the
same as blank creation. Preview and cancellation create no project and do not
change session/recents. A publication failure keeps the preview available to
retry or cancel. Publication uses the exact previewed snapshot; external-file
changes or removal after preview do not alter it. Apollo never writes the
external file. A publication/activation failure retains the complete published
folder and shows its path through safe access rather than claiming activation. Activation compares the locked, inspected
project assets against the reviewed snapshot as well as checking its UUID;
consistent replacement of both dependencies cannot silently substitute a
different import during handoff.

An ordinary `.fountain` command-line input now opens this preview, taking
precedence over last-project resume. Explicit instrumentation (`--check`,
`--agreement`, `--create [FILE]`, measurements) still uses the limited spike
parser for fixtures. That old `--create [FILE]` is not the product import path
and has no preservation/compatibility guarantee. For repeatable byte-preserving
intake without UI, use:

```sh
build/release/apollo_editor /absolute/new/project --retain-fountain /absolute/source.fountain
build/release/apollo_editor /absolute/new/project
```

Review-only projects use **project manifest version 2**, exactly version 1's
four keys plus `"mode": "import-review"` and `"version": 2`. Ordinary editable
projects remain version 1. Older Apollo builds refuse version 2. The v2 source
is a revision-zero screenplay with one empty action and an identity, a reserved
structural placeholder, never a converted screenplay. Loading refuses authored
writing in that placeholder. ProjectSession saving, Autosaver construction and
editor layout/export instrumentation refuse review-only projects. The UI
installs a reader with no writing document, autosaver or PDF job. It cannot
claim clean screenplay output or silently treat retained writing as empty.

Required immutable dependencies:

| File | Contract |
| --- | --- |
| `imports/original.fountain` | Exact captured bytes, including empty files, BOM, all whitespace/line endings, invalid encodings and control characters |
| `imported-material.json` | JSON object with exactly `format: "apollo-fountain-intake"`, `version: 1`, `status: "pending-conversion"`, `origin` (captured absolute source path), `bytes` (integer byte length), and `sha256` (lowercase SHA-256 of original) |

Validation checks both files, schema/version, length and hash on staging and
reopen. Missing/damaged/newer dependencies remain evidence, not a successful
empty writer. Reserved intake metadata in a version 1 project is refused, so a
manifest-only downgrade cannot enable editing. Existing opaque pre-intake
assets retain their earlier byte-preserving copy contract. Source intake opens
one regular file descriptor with no-follow/nonblocking flags and checks its
size/timestamps before and after reading. Input symlinks, FIFOs, directories,
unreadable files and files over **16 MiB** are refused. Reopened v2 originals
are bounded to 16 MiB and metadata to 64 KiB. Parent-folder aliases are allowed;
source file aliases must be selected by their actual target path.

The reader exposes the complete retained source as plain selectable read-only
text. It removes a UTF-8 BOM from the display only, visibly escapes control and
format characters (including bidirectional controls), and uses a hex byte view
for invalid UTF-8/other encodings. Text rendering can normalize line endings;
the file copy remains authoritative. Candidate findings include note/boneyard
delimiters, organization, production numbers, whitespace and syntax needing
interpretation, with one-based lines and zero-based half-open original byte
ranges. The list is capped at 200 with explicit notice; excerpts are bounded
and labelled when truncated. The full source remains accessible. Findings are
regenerated review hints, not stored semantic conversion, named notes/stash,
font diagnostics, or exhaustive syntax recognition. Nested/escaped delimiters
and malformed note boundaries remain unresolved, and all source remains pending
regardless of the number or absence of findings.

Active readers use the same identity-checked project access and lock, recents
and no-argument resume. Home releases the lock on the worker, and normal writer
switching still drains writing before Home/import. Reader selection/scroll is
not persisted yet. Changes to the external source do not affect the retained
project; later changes to retained dependencies are refused on reopen. During
a session the reader is the validated in-memory snapshot. Recovery packages
and new copies preserve both dependencies and automatically retain review-only
mode; damaged intake uses the existing explicit recovery chooser. The reader
has no writing to autosave and does not create routine snapshots or expose a
Save a Copy UI of its own. Original project, existing recovery evidence and
published copies remain untouched by recovery selection.

Verification is recorded with task 2.6a in [writing-build-plan.md](writing-build-plan.md).
Full conversion, note/stash naming, correction, editing, canonical Fountain
serialization and independent semantic round-trip remain incomplete. No new
representative performance, physical-input, power-loss or import interruption
loss bound is claimed.

### Located interpretation copies (initial task 2.6b)

The preceding 2.6a section describes metadata v1. Explicit **Interpret in a New
Copy** creates a fresh review-only project with intake metadata v2. It never
rewrites a v1 intake or changes its identity. A validated snapshot is retained
even when the external file is later changed/deleted. The prior project is
revalidated before a reader migration; failures keep its reader and lock.
Publication/activation follows the same staged no-replace and reviewed-assets
comparison as intake. `--interpret-fountain FILE` captures directly into a new
destination; `--retain-fountain FILE` keeps its v1 behavior.

Metadata v2 has exactly the six v1 keys plus `conversion`; `version` is `2` and
`status` is `interpreted-review`. The original bytes, origin, length and SHA-256
stay unchanged. `conversion` contains exactly:

| Key | Contract |
| --- | --- |
| `revision` | Converter revision `1`; newer revisions are refused |
| `screenplay` | Embedded source-v1 screenplay at revision 0, with deterministic `fN` IDs; title fields, authored case/text, emphasis, deliberate breaks and unambiguous dual flags |
| `locations` | One entry per interpreted element: `id`, original envelope `range`, and contributing `textSources` runs before emphasis decoding |
| `retained` | Named records with `id`, `kind`, `name`, `text`, `key`, `anchor`, `parent`, `depth`, `range`; kinds `note`, `stash`, `section`, `synopsis`, `titleMetadata`, `sceneNumber`, `spacing` |
| `unresolved` | Complete array of `code`, `message`, `range` records, separate from layout limitations |
| `limitations` | Located authoring/rendering gaps that prevent claiming faithful layout, even for recognized syntax |
| `editable`, `fountainExport`, `pdfExport` | All `false`; no syntax-error count can enable these paths |
| `outputReason` | Explicit review-only/output-blocking and future retained-data omission disclosure |

Every `range` has `startByte`, `endByte`, `firstLine`, `lastLine`. Byte offsets are
zero-based, half-open, into the immutable original including BOM/CRLF. Lines are
one-based and include the last occupied byte; an empty range points to its
insertion line. Contributing runs exclude removed notes/delimiters/indentation;
element envelopes locate normalized separators. They do not yet map cursor
positions after edits. Notes/stash store exact interior UTF-8 text and source
ranges; section hierarchy, synopsis/scene-number associations and unknown title
fields remain outside the screenplay. The complete provenance/interpretation
rules and conservative limits are in
[fountain-compatibility.md](fountain-compatibility.md#conversion-revision-1-and-migration).

The project manifest remains v2 `import-review`, and `screenplay.json` remains
the empty revision-0 placeholder required by that mode. The embedded
interpretation is never silently substituted into the writer or PDF snapshot.
Assets are immutable during a reader session and copied into recovery packages.
Metadata is now bounded to **64 MiB** at publication/reopen, original to **16 MiB**;
the converter separately bounds interpretation work to **1 MiB / 100,000 lines**.
Larger sources receive an unresolved whole-source range without dropping bytes.
Display lists/excerpts are bounded independently and do not truncate stored data.

Reopen validates metadata/evidence first, then recomputes and compares the entire
stored conversion using the frozen revision-1 algorithm. Edited records/ranges,
missing keys, unknown fields, changed original bytes and fabricated output
permissions are refused. Newer algorithms require version dispatch and explicit
new-copy migration; they must not silently reinterpret old projects. A 2.6a-only
build rejects v2 metadata. This is a conservative read-only checkpoint; correction
UI, editable association preservation, complete grammar, canonical Fountain
serialization, independent post-edit semantic round-trip, faithful layout/font
diagnostics and production export remain pending.

### Bounded editable Fountain copies (task 2.6c)

The [writer subset contract](editable-fountain-subset.md) defines exact admission,
association behavior, independent fixtures and remaining blockers. This adds a
new manifest **v3**, with the five keys `format`, `version`, `id`, `screenplay`,
`mode`; `mode` is `fountain-working`. Older builds refuse v3. Ordinary projects
stay v1; v1/v2 intake metadata and review manifests retain their meanings.

`imported-material.json` uses format `apollo-fountain-working`, version `1`,
contract `writer-subset-1`, (from task 3a) `writer-subset-2`, (from task 3b)
`writer-subset-3`, or `writer-subset-4` (six exact Unicode punctuation marks), and
`intake` containing the complete frozen v2 review metadata. These are exactly
four keys. New copies record `writer-subset-4`; each copy is recomputed under
the contract it records ([case-handling.md](case-handling.md),
[spacing.md](spacing.md), [writer subset](editable-fountain-subset.md)).
Earlier contracts retain their own stricter rules; earlier builds refuse
unrecognized contracts. The original Fountain bytes, digest,
origin, source ranges, interpreted screenplay, records and associations remain
immutable; the nested review permissions remain false. The outer preservation
contract is independently recomputed on create/open/recovery, never trusted as a
permission boolean. Missing, altered, unsupported or newer evidence is refused.

Explicit **Edit in a New Copy** (also `NEW_DIR --edit-fountain FILE`) creates a
fresh staged project initialized with the located interpretation's exact elements
and IDs. Prior review projects and the external source stay untouched. Failed
publication retains the reader/lock. Activation compares the published assets
with the reviewed snapshot before offering a writer.

The mutable screenplay still uses source v1, with stable IDs, normalized LF,
code-point spans on disk and UTF-8 spans in memory. Later unfinished/unsupported
edits remain saveable; admission limits apply to the original conversion, not to
whether new writing can be preserved. The original note/stash/metadata records
are copied verbatim through Save a Copy, local/emergency packages and recovery
forks. Their historical IDs are resolved against the live document; missing
anchors show Detached, changed types show Type changed, and undo reconnects.
There is no automatic retargeting or claim of tracking a text passage after edits.

The writer's imported-material reader shows names, original ranges/text and live
association states, plus the complete original source. Qt's text view normalizes
line endings for display; the retained bytes and record strings keep CRLF/BOM
as specified. Save a Copy updates the displayed active location. Saving and
Reload check immutable dependencies; changed import evidence pauses saving and
cannot be adopted by Reload. Cached evidence remains available for Save a Copy
and emergency protection.

Imported documents carry a PDF export block for their lifetime. The PDF request
checks it before staging or replacing output, including instrumentation, reopened
copies and recovery writers, even after editing or removing all imported text.
Clean Fountain export (task 2.6d) is separate: each request checks the current
writing snapshot against [`fountain-canonical-4`](fountain-export.md), for these
copies and ordinary projects alike. It writes no project file and does not change
the stored `fountainExport: false` of the frozen review interpretation.
