# Inspect the Writing desk

The owner accepted the QML look as the visual foundation and chose to keep
Qt/C++/QML on 2026-10-28. This guide remains useful for further inspection.
Individual visual details can be refined as screens develop.

Run this from a terminal in your Hyprland session (any current directory):

```sh
~/Code/apollo/tools/inspect-writing-desk.sh
```

It checks the display environment, builds the release app, waits for Enter,
opens [prototype B](../prototype/editor.html?variant=b) in your browser, and
opens the same original sample text in the native QML editor. Close QML to
finish. Logs, a disposable sample copy, and any exported PDF stay in the
printed `build/desk-review-*/` directory. The source sample is preserved.

Compare at roughly the same window size, ideally at least 1100 px wide:

1. **Overall look:** slate header, compact left rail, quiet gray desk, centered
   white paper, type size, contrast, and density. Does this carry over the
   approved Writing desk direction comfortably on your screen?
2. **Outline:** click the second rail icon in both versions. In QML, close it
   with × or the same icon; Ctrl+Backslash also toggles it. Check the pale
   panel's width and separation from the page, and whether hiding it restores
   the writing space. At narrow widths the page can scroll horizontally.
3. **Live editing:** type a little, select and copy, undo with **Ctrl+Z**, redo
   with **Ctrl+Shift+Z**, and open/close
   the outline with a selection. The caret and selection should stay intact.
   Enter/Tab use the spike rules. Longer page-gap and dual-dialogue behavior
   is already covered by the automated checks and earlier manual observations.
4. **Export:** the top Export button opens a destination field and title-page
   option; Export PDF takes a snapshot. Ctrl+E uses the current options directly.
   Keep writing during export; the footer reports completion of that snapshot.

The QML page uses the pinned Courier Prime and real export geometry, so its
line breaks, parenthetical wrapping, spacing, and first-page numbering differ
from the HTML's approximate page-12 mockup. The sample has two actual scenes.
The outline lists scenes **at opening** and highlights its first preview row;
it does not track edits or the caret, navigate, or reorder. Notes and stash are
dimmed placeholders. History, saving, and other product features are outside
this spike; the header accurately says edits are not saved.

The owner has assessed and accepted the visual foundation. The agent also
inspected native open/closed/compact captures and tested native launch/export.
Extended writing comfort remains unestablished. The owner reported trying
Ctrl+U and had not manually tested undo with Ctrl+Z yet; the existing automated
undo/redo tests pass. This records an unperformed manual check, not a reproduced
Ctrl+Z failure. See [the report](spike-report.md) for the measured limitations.

The previous visual/stack gate for merging `spike/editor-export-final` is now
clear. The runner records test output; further owner feedback must still be
recorded separately.

For reproducible automated native checks, including the 200-key measurements
on both workloads, run `tools/check-wayland.sh`. For a brief automated check of
the handoff's native screen and export, run
`tools/inspect-writing-desk.sh --smoke`. Neither records owner acceptance.
