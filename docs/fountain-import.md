# Fountain import

Owner decision, 2026-11-05, after trying the review-based import with a real
script (`reference/cross.fountain`). It replaces the review/retain/interpret/
edit-copy workflow of tasks 2.6a–2.6c as the way to import.

## Behavior

1. Home → **Import Fountain…**.
2. Enter a **project name** (required) and choose the **Fountain file**.
3. **Import**. Apollo converts the file into an ordinary Apollo project in the
   default location (`~/Documents/Apollo`, numbered if the name exists) and
   opens it for writing.

There are no other options: no location chooser, no review screen, no
"retain", "interpret" or "edit in a new copy", and no spacing choice. Extra
blank lines (for example before scene headings) simply take Apollo's normal
spacing. The Fountain file is only read; it is never written, moved or
copied into the project. The new project is ordinary writing: autosave, PDF
export and Fountain export behave exactly as for a project created in Apollo.

`apollo FILE.fountain` opens the home page with the import form showing that
file; nothing is created until the project is named and **Import** is pressed.
`apollo_editor NEW_PROJECT --import-fountain FILE.fountain` does the same import
from the command line at an exact new folder.

## Conversion

The existing Fountain interpreter (`source/fountain_conversion.cpp`) reads the
file. Headings, action, characters, parentheticals, dialogue, transitions,
centered text, lyrics, page breaks, dual dialogue, emphasis and the six
title-page fields carry over. Continuation lines of a title key, and repeated
consecutive keys, become one multiline field. Authored text is kept as written, including cues with no
dialogue (the writer's speech-issues bar lists those; see
[diagnostics.md](diagnostics.md)).

Apollo has no place yet for notes `[[ ]]`, boneyard `/* */`, sections `#`,
synopses `=`, scene numbers `#1#` or other title keys. They are left out and a
single line in the writer header says what was not imported. The line goes
away when the project is closed. A file that is not UTF-8, or is larger than
1 MiB / 100,000 lines, is refused with a message and nothing is created.

## Verification

- `project_access_flow_tests::fountain_import_opens_ordinary_project_and_leaves_original_unchanged`
  drives the real home-page controls: required name/file, a missing file,
  triple blank lines before a heading, curly punctuation, emphasis, a
  multiline title field, the left-out notice, an ordinary (non-import) project,
  autosave of an edit, and unchanged Fountain bytes.
- `fountain_path_prefills_import_instead_of_resuming_remembered_project` covers
  the command-line path.
- Owner script, 2026-11-05: all 2,319 elements of `reference/cross.fountain`
  match aw-parser 0.1.1 (the independent oracle) in type and text, apart from
  six elements where aw-parser keeps `*italic*` markup as text and Apollo
  applies italics. It lays out to 90 pages, and the original's SHA-256 is
  unchanged. With its two stray `(MORE)` / `DEAN (CONT'D)` cue blocks
  removed, Apollo's Fountain export reads back identically through aw-parser.

## Older imported projects

Projects created by the earlier review-based import (review-only, interpreted
or "editable copy" projects) still open through the legacy code in
`source/fountain_intake.*`, `source/fountain_working.*` and
`app/ImportedMaterial.qml`, with their previous limits. That code and its tests
are no longer reachable from the home page. Removing it is a separate cleanup.
