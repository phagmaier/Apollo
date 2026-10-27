# Apollo — independent stack recommendation

Assessed 2026-10-27. Status: **provisionally adopted by the owner on 2026-10-27** (see [planning round 15](planning.md#round-15--provisional-stack-decision)). The adoption is pending the feasibility spike below. The [product spec](spec.md) remains authoritative for requirements; [stack.md](stack.md) is the earlier research this document re-examines. Nothing was prototyped, installed, or benchmarked for this assessment.

Evidence labels used below:

- **Documented**: stated by official docs or upstream package metadata (linked).
- **Source**: read directly in maintained upstream source code (linked).
- **Measured here**: checked on this machine or in downloaded upstream files during this assessment.
- **Inference**: engineering judgment, not yet tested.

## Recommendation in one paragraph

Build Apollo with **Qt 6 (system Qt 6.12 on Arch) + C++20 for the core + QML for the interface**. Write an **Apollo-owned screenplay layout engine** that decides every line and page break. The editor and the PDF exporter both draw from that one result. The editor stores and edits text in Qt's `QTextDocument`/`QTextCursor`, with a custom `QAbstractTextDocumentLayout` and an Apollo-written QML editor item handling input and painting. PDF export paints the same laid-out pages through `QPdfWriter`. **Fallback: Electron + TypeScript + ProseMirror**, built on the same layout-engine design, with PDF drawn from the engine's line model. **Confidence: moderate.** The documented APIs and two existing Qt screenwriting apps support the Qt choice, but the editor-interaction cost is unmeasured. One focused editor/export spike (below) decides whether Qt holds or Apollo switches to the fallback.

If you rank lowest development effort above native efficiency, the fallback would become the primary. That trade is the only preference that flips this recommendation, and your stated preference (native if manageable) decides it.

## The reframe that changes the comparison

The earlier evaluation asked whether each toolkit's own pagination or print path (`QTextDocument` paging, Chromium `printToPDF`, WebKitGTK printing, the Dart `pdf` widget layout) could match its editor. That is the wrong question for a screenplay.

Screenplay format is fixed-pitch text in fixed-width columns: 12-point Courier, about 10 characters per inch, six lines per inch, with element margins from the [Academy format guide](https://www.oscars.org/sites/oscars/files/scriptsample.pdf) ([export profile](export-profile.md)). Line wrapping can therefore be decided by **column counts**, not glyph measurement. Page breaks then follow screenplay rules (keep heading with action, MORE/CONT'D, dual dialogue) applied to whole lines. Both are deterministic computations Apollo can own and test without a GUI.

With an owned engine:

- **The PDF is drawn, not printed.** The exporter places each engine line at its page coordinates with an embedded font. Any PDF API that can draw positioned text with an embedded TrueType font is sufficient. This removes the open questions about Tauri's Linux print path and Flutter's separate `pdf` layout system. Those print paths are simply not used.
- **Agreement is by construction.** The editor shows the engine's page and line breaks, and the PDF reproduces them from the same revision. The remaining question for each stack is narrower: **can its editing surface display engine-decided breaks, including a page gap in the middle of a paragraph, without breaking caret, selection, input methods, or undo?**

Dedicated screenwriting tools already work this way. Trelby ([source](https://github.com/trelby/trelby), GPL-2.0) draws its own editor and PDF from its own line model. Story Architect and Scrite (below) both carry their own paginators.

**Measured here: font facts.** The [Courier Prime](https://github.com/quoteunquoteapps/CourierPrime) upstream TTFs are SIL OFL-1.1 and come in Regular, Bold, Italic, and Bold Italic. In all four, every glyph advance is 1228/2048 em with no `kern` or `GPOS` tables, so bold and italic do not change widths. That advance is **0.5996 em, not exactly 0.6**: 60 characters at 12 pt span 431.7 pt instead of 432 pt. Column-count wrapping makes this irrelevant to line breaks, and screen and PDF must simply use the same glyph placement rule. The font's ascent plus descent is about 1.12 em, so the 12-point line pitch (six lines per inch) must be set explicitly rather than taken from font metrics. Each style has 415–416 glyphs, so the unexpected-character policy in the spec needs a defined fallback. No Courier font is currently installed on the host, so Apollo should bundle its pinned font files.

## Recommended stack and components

| Responsibility | Component | Status |
| --- | --- | --- |
| UI shell (home, navigator, notes, stash, history, merge) | Qt Quick / QML with custom-styled [Quick Controls](https://doc.qt.io/qt-6/qtquickcontrols-customize.html) and [MultiEffect](https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html) for shadows and blur | Documented; Qt 6.12 installed (`qt6-base`, `qt6-declarative`, `qt6-wayland`, `qt6-svg`), measured here |
| Live document, cursor operations, undo | [`QTextDocument`](https://doc.qt.io/qt-6/qtextdocument.html) blocks with element type as a block-format property; `QTextCursor` editing and edit blocks; document undo stack | Documented API; screenplay use is Inference |
| Line and page layout | Apollo `ScreenplayLayout`, a [`QAbstractTextDocumentLayout`](https://doc.qt.io/qt-6/qabstracttextdocumentlayout.html) subclass driving per-block [`QTextLayout`](https://doc.qt.io/qt-6/qtextlayout.html) lines through [`QTextLine::setNumColumns` and `setPosition`](https://doc.qt.io/qt-6/qtextline.html), plus Apollo page-break rules | The class is documented as subclassable for custom layouts; the rules are custom work |
| Editor item | Apollo QML item (`QQuickPaintedItem` first) that paints visible pages through the layout and handles keys, mouse, input methods, and clipboard with `QTextCursor` | Custom work (see "Editor feasibility") |
| PDF | [`QPdfWriter`](https://doc.qt.io/qt-6/qpdfwriter.html) + `QPainter`, painting the same pages without screen-only decorations | API documented; font embedding and text extraction are verified in the spike |
| Font | Courier Prime TTFs, pinned and bundled | Measured here |
| Source of truth on disk | Apollo text format with typed elements, stable element IDs, inline emphasis, and per-cue extensions; Fountain is import/export only | Recommendation; schema still open |
| Autosave | Debounced write through [`QSaveFile`](https://doc.qt.io/qt-6/qsavefile.html), plus a recovery journal if the spike shows it is needed for the one-second target | Primitive documented; protocol is design work |
| Git | System `git` (2.56.0 installed) through asynchronous [`QProcess`](https://doc.qt.io/qt-6/qprocess.html) | Documented |
| Spelling | [Hunspell](https://github.com/hunspell/hunspell) library plus the installed `hunspell-en_us` dictionaries (`/usr/share/hunspell`), with a personal word list in project or user data | Dictionaries measured here; the Hunspell library package itself is **not** installed |
| Build and tests | CMake (4.4.4 installed) + Ninja (not installed), Qt Test with the `offscreen` platform for headless editor tests, ASan/UBSan builds, and `pdffonts`/`pdftotext`/`pdfinfo` (installed) for export checks | Measured here |

Qt's open-source licensing is LGPLv3/GPLv3 (the Arch package lists both). Dynamic linking against system Qt suits a personal app and leaves Apollo's own license open. Copying code from the GPL-3.0 reference apps below would make Apollo GPL-3.0, so treat them as references to study, not code to copy, unless that license is acceptable.

## 1. Editor feasibility

### What Qt reuses

- **Text storage, cursor movement, undo.** `QTextDocument` provides blocks, character formats (bold, italic, underline), block-format properties, word and line cursor movement, edit-block grouping, and undo/redo. A scene move becomes one edit block that removes a block range and inserts a fragment, so one Undo restores it. An element-type change is a block-format change. Whether user-defined format properties survive fragment copy and undo exactly is a spike check (Inference).
- **Line layout and line breaking.** `QTextLine::setNumColumns(n)` is documented as filling a line "with as many characters as are specified by numColumns", which matches column-based screenplay wrapping. `QTextLine::setPosition` moves individual lines, so the lines of one paragraph can sit on two pages with a gap between them. This is the hard case for every other stack.
- **Custom layout hook.** `QAbstractTextDocumentLayout` is documented as subclassable "to provide custom layout behavior." Its required methods (`draw`, `hitTest`, `pageCount`, `documentSize`, `blockBoundingRect`, `frameBoundingRect`, `documentChanged`) cover painting, click-to-cursor, and incremental relayout.
- **Input methods on Wayland.** Qt Wayland bundles the text-input-v3 protocol definitions ([module page](https://doc.qt.io/qt-6/qtwaylandclient-index.html)), and `fcitx5`, `fcitx5-qt`, and `fcitx5-gtk` are installed here. English-only writing makes input methods less central, but dead keys and compose sequences must still work.

### What Apollo must write

- **The layout engine:** the profile's column widths and indents, page breaking, MORE/CONT'D generation (drawn as labels, never inserted into document text), dual dialogue placed side by side, manual breaks, title-page numbering, and incremental repagination from the edited page onward.
- **The editor interaction controller:** key handling (Enter/Tab element rules and suggestion menus are custom anyway), mouse and drag selection, caret painting, scrolling to the caret, input-method preedit display, clipboard (an internal MIME type plus plain-text and Fountain paste), and spelling underlines.

The custom controller is the main extra Qt cost. Qt's own controls are not a fit:

- **QML `TextEdit`:** [Source for Qt 6.12](https://raw.githubusercontent.com/qt/qtdeclarative/6.12/src/quick/items/qquicktextedit.cpp) shows it hit-tests through the document layout and places text nodes from `blockBoundingRect` plus each block's `QTextLayout` lines. It also resets the document text width when it resizes. [`QQuickTextDocument::setTextDocument`](https://doc.qt.io/qt-6/qquicktextdocument.html) (since 6.7) lets it display a supplied document. So it *might* honor a custom layout's line positions, but this is undocumented. It would also not draw page cards or continuation labels, which would need separate QML layers. **Treat it as a cheap first probe, not a plan.**
- **Widgets `QTextEdit`:** [Source](https://raw.githubusercontent.com/qt/qtbase/6.12/src/widgets/widgets/qtextedit.cpp) shows it falls back to generic `documentLayout()` calls when the layout is not Qt's default class, as `QPlainTextEdit` does with its own layout. That makes it a credible in-Qt alternative editor host. The cost is a Widgets-based window, which works against the QML shell.
- **Forking Qt's private text-control classes** is the path to avoid. [Story Architect](https://github.com/story-apps/starc) (Qt Widgets, GPL-3.0, actively maintained) ships its own [`PageTextEdit`](https://github.com/story-apps/starc/tree/master/src/corelib/ui/widgets/text_edit/page): a roughly 4,000-line class that mirrors Qt's `QTextEdit` API and internals, plus copied scroll-area sources per Qt major version. It relies on `QTextDocument`'s built-in page size. This proves a paginated Qt screenplay editor is buildable, and it shows the maintenance cost of building it the inherited way.
- **Per-scene editors** are also a path to avoid. [Scrite](https://github.com/teriflix/scrite) (Qt/QML, GPL-3.0, actively maintained) is a QML screenwriting app with a separate paginator. Its editor is composed of per-scene editor delegates (`qml/screenplayeditor/delegates/sceneparteditors/`). That proves QML suits this kind of app, but per-scene text controls make selection, find, and undo across scenes harder. Apollo should use one document.

**Inference on cost:** the custom controller is bounded because Apollo's text is simple. It is English-only, monospaced, left-to-right, needs no shaping beyond what `QTextLayout` already does, and its caret and hit geometry is plain column arithmetic. It is still the single largest piece of non-reused interaction code in this recommendation.

### Required editor behaviors, by owner

| Behavior | Qt route | Electron + ProseMirror route |
| --- | --- | --- |
| Styled element types, Enter/Tab rules | Block properties + Apollo keymap | Schema node types + keymap plugin (mature) |
| Emphasis | Char formats | Marks (mature) |
| Dual dialogue | Engine places right-column blocks beside left; caret follows document order | `NodeView` with a two-column grid; engine must still paginate it |
| Caret/selection across page boundaries | Apollo controller over engine geometry (custom, deterministic) | Browser behavior around widget decorations (reused, but known fragile; see below) |
| Paste | Apollo clipboard handling (custom) | ProseMirror clipboard parsing (mature) plus schema rules |
| Undo, including scene moves | `QTextDocument` edit blocks | `prosemirror-history` transactions (mature) |
| Generated MORE/CONT'D | Painted labels, absent from text | Widget decorations, absent from document |

## 2. PDF agreement contract

1. Engine input is the document revision plus the export profile (fixed column widths, 12-point pitch, Letter page). Zoom, window size, and screen DPI are not inputs.
2. Engine output is the page model: for each page, the lines with (element ID, text range, x/y in points) and the generated labels, page numbers, and title-page offset.
3. The editor paints that model, scaled. The exporter paints the same model on a `QPdfWriter` page (612×792 pt) with Courier Prime embedded, skipping caret, selection, spelling marks, and page chrome.
4. Export runs on an immutable snapshot of the current revision, either off the GUI thread or by recomputing from it, never from stale on-screen pagination.
5. Verification is independent of the engine. `pdfinfo` checks page count and size, `pdffonts` checks that fonts are embedded, and `pdftotext -layout` compares each page's text lines to hand-written expected lines for small fixtures. Rendered pages are compared to approved images. A second export through the same engine is not an independent check, as [export-profile.md](export-profile.md) already notes.

This contract is the same for the fallback: the PDF is drawn from the engine's line model, either with [PDFKit](https://www.npmjs.com/package/pdfkit) (MIT; 0.20.2 published 2026-08) or with Electron's [`printToPDF`](https://www.electronjs.org/docs/latest/api/web-contents#contentsprinttopdfoptions) over a separate page DOM in which each engine line is its own non-wrapping element. Avoid `pdf-lib`: its latest release, 1.17.1, dates from 2021-11, per the npm registry checked 2026-10-27.

## 3. Appearance and customization

The look you want (Bear, iA Writer, Things 3, Superhuman) depends mostly on the chrome around the page: typography, spacing, a calm sidebar, soft shadows, quiet motion, and a well-presented paper page. The page itself is Courier on a card.

- **QML:** no native widget theme is imposed. Controls are restyled per [Customizing Qt Quick Controls](https://doc.qt.io/qt-6/qtquickcontrols-customize.html), rendering is GPU-composited, and states, transitions, shadows, and blur are built in. The editor page is fully Apollo-painted, so its appearance is unconstrained. Special infrastructure needed: one Apollo style module (colors, type scale, spacing tokens, a few custom controls). (Inference: no blocker; the effort is design work.)
- **Web (fallback):** CSS is the most flexible option and the one agents handle best, which is the web route's real advantage.
- **Process recommendation, independent of stack:** settle the visual direction with quick static HTML mockups first, since they are cheapest to iterate and you prefer reviewing visuals. Then reproduce one approved screen in QML during the spike, to prove the look carries over.

## 4. Development and maintenance

| Factor | Qt/C++/QML | Electron/TS/ProseMirror |
| --- | --- | --- |
| Languages and boundaries | C++ and QML, with one well-trodden boundary (C++ objects exposed to QML) | TypeScript in both renderer and main process; the IPC boundary is simple |
| Editor infrastructure written by Apollo | Layout engine and interaction controller | Layout engine and page-gap decoration logic; the interaction layer is reused |
| Build and debug | CMake, compile cycles, QML tooling, sanitizers; crashes possible | Instant reload, DevTools, Playwright; no memory-safety class of bugs |
| Testing editor behavior | Qt Test drives real key and mouse events headlessly (`offscreen`) and is deterministic | Unit tests on editor state are easy; contenteditable end-to-end tests are flakier |
| Dependencies | System Qt from Arch (tracks upstream quickly); Hunspell | Electron (major release every ~8 weeks; 45.0.0 as of 2026-10-21), ProseMirror (1.42.8, 2026-10-24), PDFKit |
| API stability | Qt 6 public APIs are stable within the major version | ProseMirror is stable; Electron upgrades need regular attention |

**Agent-assisted development (Inference, not measured):** both stacks are well represented in public code. TypeScript UI work is likely the smoother path for agents. C++ adds ownership and lifetime mistakes that agents can make and that only sanitizers and tests catch. Two things matter more than the language. First, a pure, deterministic layout engine with golden fixtures, which agents can work against safely in either stack. Second, headless behavior tests for the editor, which Qt Test makes straightforward. No effort multiplier or schedule is claimed here.

## 5. Runtime behavior

- **Qt (Inference, unmeasured):** a native process using shared system Qt libraries and native Wayland. Nothing is bundled, so the installed size is small. Typing cost is one block relayout, an incremental repagination, and painting the visible pages. Startup and memory are expected to be the lowest of the options. Measure before claiming it.
- **Electron (Documented/Inference):** a bundled Chromium plus Node, with a multi-process architecture, and Wayland support by default since Electron 38.2 ([Electron Wayland post](https://electronjs.org/blog/tech-talk-wayland)). Chromium has the most robust contenteditable engine. Package size and memory are larger by architecture. Typing latency in ProseMirror is generally good but unmeasured for a fully decorated 200-page document, because ProseMirror renders the whole document to the DOM.
- **None of these runtime claims are measurements.** The proposed budgets in [stack.md](stack.md) remain targets.

## 6. Supporting features (stack-neutral constraints)

- **Autosave and recovery:** a write-ahead journal or debounced atomic rewrite of the source file, sub-second, off the GUI thread from a snapshot. Fits any stack.
- **Source format:** typed elements with stable IDs, one element per line or record, so Git diffs and merges work at element level. The live `QTextDocument` is a projection of that source. IDs must be re-assigned on paste so they never duplicate (Inference).
- **Git:** the system `git` CLI in every stack. One storage idea fits the accepted behavior and should be tested in the Git spike, not adopted yet: give each draft branch its own [`git worktree`](https://git-scm.com/docs/git-worktree), so unfinished work stays with its draft without stashing, and keep notes and stash on a separate shared branch checked out in its own worktree, so switching drafts never rewinds them. This affects storage design, not the stack choice.
- **Spelling:** Hunspell in Qt (custom underline and suggestion UI). The fallback gets Electron's built-in Hunspell spellchecker with suggestion and add-word APIs ([docs](https://www.electronjs.org/docs/latest/tutorial/spellchecker)), but it downloads dictionaries from a Google CDN by default unless you configure a local URL.
- **Nothing found here would change the stack choice.**

## 7. Cost and dependencies

There are no recurring services and no paid components in either route. Licenses: Qt (LGPLv3/GPLv3), Hunspell (MPL/GPL/LGPL tri-license), Courier Prime (OFL-1.1, embedding allowed), ProseMirror (MIT), Electron (MIT), PDFKit (MIT). [Tiptap Pages](https://tiptap.dev/docs/pages/getting-started/overview) is a Pro package from a private registry, in beta. Its docs list no browser-print integration and an "infinite layout loop" for non-splittable blocks larger than a page. It is not needed, because the engine owns pagination. GitHub private repositories are optional remote storage.

## Alternatives compared

| Option | Verdict | Why |
| --- | --- | --- |
| **Qt 6 + C++ + QML** | **Primary** | The only stack whose public text APIs let Apollo position individual lines of one paragraph (page gaps mid-element) while reusing text storage, cursor operations, and undo. Native, already installed, Wayland-ready, fully customizable look. Costs: a custom interaction controller and C++ discipline. |
| **Electron + TS + ProseMirror** | **Fallback** | The most reused editing behavior and the easiest UI work. The risk is concentrated in displaying page gaps inside a paragraph: ProseMirror's [widget decorations](https://prosemirror.net/docs/ref/#view.Decoration^widget) note that keyboard motion "won't visit both sides" of a widget without custom handling. The maintainer [describes holding back decoration updates near the cursor during composition](https://discuss.prosemirror.net/t/prosemirror-view-upgrade-caused-chinese-to-be-affected-by-decoration-widget-during-input/2452), and recent releases still fix widget and composition edge cases ([changelog](https://app.unpkg.com/prosemirror-view@1.42.8/files/CHANGELOG.md)). Heavier runtime. Upstream ProseMirror now lives at [code.haverbeke.berlin](https://code.haverbeke.berlin/prosemirror), and the GitHub mirror is archived. |
| Tauri 2 + Rust + TS + ProseMirror | Variant of the fallback, not shortlisted | The same frontend runs on WebKitGTK, the least proven of the three engines for this editing workload on Linux. Tauri maintains a [Linux graphics-issues guide](https://v2.tauri.app/develop/debug/linux-graphics/). It also adds a second language and boundary. Its PDF question disappears with the owned engine (for example [krilla](https://crates.io/crates/krilla) 0.8.2), so PDF is no longer the reason it ranks lower. It becomes preferable to Electron only if a WebKitGTK test of the fallback editor on the 200-page document types and scrolls smoothly. |
| Flutter + Dart | Rejected for now | No maintained desktop document editor with verified Linux support: [super_editor](https://pub.dev/packages/super_editor)'s last stable release, 0.2.7, is about two years old, with API-churning dev releases, and it lists Linux as "unverified". It would need a custom editor surface like Qt's, but without `QTextDocument`'s model, cursor, and undo. Not installed. |
| Rust + GTK4 | Credible native runner-up, not chosen | `GtkTextView` styles paragraphs through tags but offers no per-line positioning, so mid-paragraph page gaps and side-by-side dual dialogue mean a custom view. That custom view would be Pango-drawn, and Pango can paint the same layout to a [Cairo PDF surface](https://docs.rs/cairo-rs/latest/cairo/struct.PdfSurface.html). Spelling via `libspelling` is installed and attractive. It loses to Qt on bespoke styling (GTK CSS and GNOME conventions versus QML's scene graph) and on the volume of Rust GObject-subclassing code. |
| Rust core + Qt via [CXX-Qt](https://kdab.github.io/cxx-qt/book/) (0.10.0) | Only if you want Rust | It does not reduce editor work and adds a bridge and build layer. |
| Qt WebEngine (6.11.2 is installed) or PySide6 | Rejected | WebEngine brings Chromium's weight without Electron's tooling. PySide6 splits the editor's hot path across a language boundary for no stated benefit. |
| From-scratch text control (any toolkit) | Rejected | Unnecessary. Qt supplies storage, shaping, and undo, and the web supplies the whole interaction layer. |

## Where this disagrees with the earlier evaluation

1. **Print and PDF paths were the wrong axis.** With an owned engine, Tauri's WebKitGTK printing and Flutter's separate `pdf` layout no longer matter. They are not reasons to rank those stacks lower, and `QTextDocument`'s built-in pagination or print is not used either.
2. **"Retain an existing text interaction control" is too strict as a pass condition for Qt.** A custom interaction controller over `QTextDocument`/`QTextCursor` is the likely and acceptable design. What should fail the spike is reimplementing text storage or undo, using Qt private headers, or forking Qt classes as Story Architect did.
3. **Electron, not Tauri, is the web fallback.** The fallback exists to minimize editor risk, and Chromium is the strongest contenteditable engine on Linux. Tauri's advantage is package size, which matters less than editing robustness here.
4. **Flutter drops from "serious alternative".** Its editor foundation is the problem, not its PDF.
5. **"12 pt, 10 pitch" is approximate for the likely font.** Courier Prime is 0.5996 em per character, so the engine should wrap by columns, never by measured widths.
6. **Run one spike, not two in parallel.** Spike Qt first, with explicit switch triggers. Spike the fallback only if Qt fails.

## Hardest custom work and trade-offs

1. **Page-breaking rules** for every element, including dual dialogue at boundaries (proposal: never split a dual-dialogue block; move it whole) and continuation labels. This is shared by every stack and needs the numeric profile finished first.
2. **The Qt editor interaction controller**: caret, selection, input methods, clipboard, and scrolling across page gaps and dual-dialogue columns.
3. **Incremental relayout** that stays responsive at 500 pages: relayout only edited blocks and repaginate forward until breaks re-synchronize (Inference: cheap, because breaks are simple arithmetic).
4. **Screenplay-aware merge UI and shared-material Git storage.** These are hard in every stack and do not discriminate between them.

Trade-off accepted by the primary: more editor-interaction code and C++ build and debug friction, in exchange for deterministic layout control, native runtime weight, and no browser engine.

## Smallest decisive feasibility work

**Step 0, stack-neutral prerequisites.** Pick the numeric profile values (column widths and indents per element, from [export-profile.md](export-profile.md)). Pin the Courier Prime files. Write about six small original fixtures with hand-written expected lines and page breaks: page break inside action, long dialogue with MORE/CONT'D, a cue at the page bottom, dual dialogue near a break, a manual break, and a title page. Generate 200- and 500-page documents from varied element mixes.

**Step 1, Qt editor and export spike.** One QML window with the editor item over `QTextDocument` and `ScreenplayLayout`. First run the cheap probe of QML `TextEdit` with a custom layout; expect the custom item. Include one approved mockup screen reproduced in QML.

| Check | Pass condition |
| --- | --- |
| Agreement | For every fixture and both generated documents, the editor's page and line dump equals the PDF's `pdftotext -layout` lines page for page, and both equal the hand expectations for the small fixtures. This still holds after scripted edits that push content across page boundaries. |
| Interaction | Scripted Qt Test runs plus a manual session cover: typing across a page gap without caret jumps; shift-selection and copy across a page gap and through dual dialogue; Enter/Tab rules; paste of plain text and an internal fragment; undo/redo of typing, type change, and scene move restoring the exact document; a dead-key or compose character through fcitx5 under native Wayland. |
| Format | MORE/CONT'D appear on screen and in the PDF but never in document text or Fountain. A cue extension survives. A manual break works. |
| PDF | 612×792 pt pages; Courier Prime embedded (`pdffonts`); text selectable and extractable; element x positions within a small tolerance of the profile. |
| Responsiveness | Measured on your machine with the 200- and 500-page documents, against the [stack.md budgets](stack.md#proposed-performance-budgets) as targets. The 500-page document stays usable. |
| Code shape | No Qt private headers, no copied Qt sources. The engine is testable without a window. |

**Switch to the Electron fallback if:**

- the interaction checks cannot pass without private Qt APIs or a fork of Qt's text control;
- caret, selection, or input-method defects across page gaps persist after focused fixing, and the controller keeps growing into a general text-control rewrite; or
- typing latency on the 200-page document misses the target and profiling shows the cause is Qt's text stack rather than Apollo's code.

The engine's rules and fixtures carry over to the fallback; the C++ engine code does not. If switching, first run a narrow ProseMirror probe of a mid-paragraph page gap under typing, selection, and input-method composition in Chromium before building more. Run the same probe in WebKitGTK only if Tauri's lighter package matters to you.

The visual direction (HTML mockups, then one QML screen) and Git storage design run separately and do not decide the stack, unless QML cannot reproduce an approved mockup's key qualities, which is not expected.

## Unresolved facts that limit this recommendation

- **Whether QML `TextEdit` honors a custom document layout:** not established by the docs. It only affects how much controller code Qt needs.
- **`QPdfWriter` output with Courier Prime:** font embedding type, subsetting, and text extractability are unverified.
- **Fragment and undo details:** whether `QTextDocument` undo and fragment copy preserve Apollo's custom block properties exactly is unverified.
- **No measurements exist** for runtime, memory, startup, or latency in any stack, and there is no measured comparison of agent productivity in C++/QML versus TypeScript.
- **The numeric export profile and page-break rules are incomplete.** The engine cannot be finalized until they are.
- **Hunspell library:** not installed. Only the dictionaries are present, and `ninja` is also missing, so the spike needs both installed through pacman or mise.
- **Licensing intent:** if you might publish Apollo, decide the license before borrowing anything from the GPL-3.0 reference apps.

## Host observations (read-only, 2026-10-27)

- **Installed:** Qt 6.12.0 (`qt6-base`, `qt6-declarative`, `qt6-wayland`, `qt6-svg`), `qt6-webengine` 6.11.2, WebKitGTK 2.54.1 (`webkit2gtk-4.1` and `webkitgtk-6.0`), GTK 4.24.1, libadwaita 1.10.0, `libspelling` 0.4.10, `enchant` 2.8.21, `hunspell-en_us` dictionaries, `fcitx5` 5.1.23 with Qt and GTK modules, GCC 16.2.1, Clang 23.1.3, CMake 4.4.4, Git 2.56.0, gh 2.103.3, Node 26.12.1, pnpm 12.15.0, Rust 1.99.1, mise, and poppler's `pdffonts`/`pdftotext`/`pdfinfo`.
- **Not installed:** Flutter, Dart, Electron, Ninja, the Hunspell library, any Courier font.
- **Package versions checked against npm and crates.io:** listed with each claim above.
