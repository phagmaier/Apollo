# Writer layout

Owner decision, 2026-11-05, after writing in the imported script. The writing
screen should look and feel simple and clean. The owner's reference was a
professional screenwriting app: a slim labeled icon sidebar, the page, and a
pop-up element list. The look need not be copied; the simplicity should be.

## Layout

- **No top header.** The old slate header (title, save state, folder, Save a
  Copy, Export) is gone.
- **Sidebar** (left, labeled icons): Home, Scenes, Elements, Title page, Find,
  Export, Save copy, plus Toolbar, Shortcuts and Hide at the bottom. Older
  review-era imported projects also show Imported. **Hide** or
  **Ctrl+Shift+B** hides it; a small button at the top left brings it back.
- **Toolbar** (optional): Edit, element type, Page break, Format. Hide it with
  its × or the sidebar's **Toolbar** button.
- **Status line** (bottom): title (hover for the folder), save state and page
  count, any notice, and the latest operation. **Retry save** and **Reload…**
  appear there only when saving fails.
- The speech-issues bar is one line unless an issue is selected.

Sidebar and toolbar visibility are remembered across sessions in
`~/.local/share/Apollo/view.json` (beside the session state; tests use their
own state path). Icons are bundled SVGs in `app/icons/`, so they don't depend
on installed symbol fonts.

## Element picker

**Ctrl+0**, the sidebar's **Elements**, or the toolbar's element button opens a
dark list of the eight body types with their Ctrl+1–8 keys, highlighting the
current type. Up/Down (or Tab) move, Enter chooses, a number key chooses
directly, a click chooses, and Esc closes without a change. It opens beside the
page when there is room, otherwise just right of the caret, so it never covers
the line being changed. Writing keeps focus afterwards. Ctrl+1–8 and the Tab
cycle are unchanged ([element-types.md](element-types.md)).

## Caret visibility

The first caret fix addressed viewport edges: the editor scrolled only until
the caret touched the viewport's bottom edge, and the scrollbars' custom
handles were always drawn, so a 5 px bar lay over that bottom line. The editor
keeps about three lines of context (at most a quarter of the view) above and
below the caret, and a scrollbar handle is drawn only when there is something
to scroll.

The owner subsequently reported the caret still disappearing in the middle of
action lines after arrow navigation or clicks. This was reproduced on native
Wayland/OpenGL: at column 3, a valid caret rectangle painted zero pixels. Qt's
one-document-pixel caret shrank to 0.75 screen pixels with the page, leaving
some horizontal positions with no raster coverage. The same check passed with
the software renderer. A custom caret now keeps one screen pixel of width at
every page scale, follows Qt's caret coordinates and height, and uses the
desktop blink interval, resetting to visible on movement or editing. Selection
and read-only states suppress it.

## Verification

- `editor_tests::caret_is_painted_inside_action_lines`: checks actual painted
  pixels at every character across a wrapped action paragraph, using arrows
  and reverse-order clicks, 1280/900 px windows and 60%/75%/100% page scales.
  Run on native Wayland/OpenGL to catch this regression; offscreen software
  alone cannot detect the original failure. Both the stock caret and a custom
  caret left at one document pixel failed at column 3 before the width fix.
- `editor_tests::caret_blinks_and_reappears_after_navigation`: actual painted
  on/off states, movement during the off phase and collapsing a selection.
- `editor_tests::caret_stays_clear_of_viewport_edges_and_scrollbars`: 120 Down
  and Up steps at the app's default window size; no horizontal scrollbar, and
  the caret stays at least two lines from the edges. It fails against the old
  scroll rule and against the always-drawn handle.
- `editor_tests::element_picker_chooses_types_from_the_keyboard`: Esc, arrows
  plus Enter, a number key, and placement at 1280 and 900 px wide.
- `project_access_flow_tests::hidden_sidebar_and_toolbar_are_remembered_across_sessions`.
- Existing element-type, outline, export, autosave and flow tests pass against
  the moved controls, offscreen and natively on Wayland (`QT_IM_MODULE=fcitx`,
  as `tools/check-wayland.sh` sets).

Not done: the home page, export dialog and other panels keep their earlier
styling; owner comfort with the new layout is unconfirmed.
