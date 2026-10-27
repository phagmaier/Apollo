# Apollo — stack evaluation and recommendation

Researched 2026-10-27. Status: candidate evaluation; no stack selected. The owner prefers native efficiency if development, maintenance, and visual design remain manageable. Electron is acceptable, not preferred. Rust, Flutter, and Tauri are explicitly part of the evaluation. No application, technical prototype, or benchmark has been built. The [product spec](spec.md) remains authoritative for agreed behavior.

## Current assessment

The owner clarified that native/low-level efficiency is the preference, while the concern about C++ is development time, code volume, maintenance, customization, and appearance. They accept Electron's costs if necessary. The question about C++ effort was not a rejection of native development. They also asked whether agent-assisted development changes that effort and explicitly raised Rust, Flutter, and Tauri.

Current agent shortlist: Qt/C++/QML for direct native text/layout integration, and Tauri/Rust/TypeScript for a Rust core plus flexible web UI/editor. Flutter/Dart is a serious additional candidate; Electron/TypeScript remains a fallback with an established browser PDF API. This is a provisional engineering assessment, not a measured ranking or owner selection. Prefer focused editor/export evidence over another language-only recommendation.

Separate three questions: how the UI is rendered, how much editor infrastructure already exists, and which language owns application behavior. Tauri's Rust core still uses a web UI. Qt Quick and Flutter render through their own toolkits; a native executable does not mean every control is a platform widget. Compiled code alone does not determine typing latency or memory use.

QML exposes [customizable controls](https://doc.qt.io/qt-6/qtquickcontrols-customize.html) independently of the C++ core. Native Qt does not require accepting a default desktop theme. The visual prototype should demonstrate the intended appearance before a large implementation is built.

There is no measured agent-development multiplier, comparative implementation size, or credible calendar estimate for Apollo yet. Agent assistance can support implementation and checking, but it does not remove pagination, ownership/lifetime, boundary, or integration problems. The current maintainability recommendation is a small set of coherent modules, public toolkit APIs, grouped edit commands, and behavior tests. Do not justify a stack with an unsupported claim that agents make C++ as cheap as TypeScript.

[Tiptap's open-source core](https://tiptap.dev/docs/editor/getting-started/overview) wraps ProseMirror and provides a modular extension API; custom screenplay nodes/commands remain Apollo work. Its [Pages package](https://tiptap.dev/docs/pages/getting-started/overview) is a subscription Pro feature, so it is not a free ready-made solution for Apollo's pagination. Evaluate an open-source-only editing/pagination path; no paid service or extension is selected. A browser editor's normal typing behavior does not establish screenplay pagination or PDF agreement.

Electron supplies a [PDF export API](https://www.electronjs.org/docs/latest/api/web-contents#contentsprinttopdfoptions), but screen and print layout still require explicit agreement tests. Its [performance guidance](https://www.electronjs.org/docs/latest/tutorial/performance) stresses profiling and avoiding blocking work. The expected reduction in UI/editor customization effort versus C++ is an engineering judgment, not a completed implementation comparison or schedule estimate.

Preserve the Qt findings below as the direct native candidate. Do not interpret them as a final selection. Use the same screenplay fixtures and acceptance requirements for candidates, and stage comparisons rather than implementing several complete applications.

## Rust options

Rust is a viable application language. The relevant question is which editing/rendering toolkit it uses.

| Route | Source-backed capability | Apollo assessment |
| --- | --- | --- |
| Rust + Tauri + TypeScript editor/UI | Rust core and system webview | Main shortlist candidate; web styling/editor reuse with native persistence/Git; WebKitGTK typing and PDF need validation |
| Rust + GTK4 | gtk-rs exposes GTK/GLib/Cairo; GTK provides TextView/TextBuffer editing with text attributes and undo | Credible native Rust route; the link between live editing, custom pagination, Pango layout, and PDF needs a concrete design/spike |
| Rust + Qt/QML using CXX-Qt | Generated bridge between Rust and Qt | Technically possible; does not automatically simplify document integration because it adds a Rust/Qt boundary |
| Rust + Iced or Slint | Documented multi-line editing primitives | Not ruled out, but the reviewed APIs do not establish a complete rich, paginated screenplay editor; require more editor-foundation investigation before prioritizing |

Sources: [gtk-rs](https://gtk-rs.org/), [GTK text buffer](https://docs.gtk.org/gtk4/class.TextBuffer.html), [Pango/Cairo integration](https://docs.gtk.org/PangoCairo/), [Rust Cairo PDF surface](https://docs.rs/cairo-rs/latest/cairo/struct.PdfSurface.html), [CXX-Qt](https://kdab.github.io/cxx-qt/book/), [Iced text editor](https://docs.rs/iced/latest/iced/widget/text_editor/index.html), and [Slint TextEdit](https://docs.slint.dev/latest/docs/slint/reference/std-widgets/views/textedit/).

The assessment concerns Apollo's required editor, not a blanket statement that Rust GUI libraries are unsuitable or immature. No Rust route has been prototyped or benchmarked here. Adding Rust merely to avoid writing a modest C++ adapter can increase integration work; choose it when the benefits justify that boundary.

## Flutter and Tauri in detail

Flutter supports [native Linux desktop applications](https://docs.flutter.dev/platform-integration/desktop); its [architecture](https://docs.flutter.dev/resources/architectural-overview) describes native compilation for release and its own rendering framework. It is a credible option for visual customization with Dart instead of C++. That is a capability fit, not proof of lower memory or easier Apollo-specific editing.

Flutter's [EditableText](https://api.flutter.dev/flutter/widgets/EditableText-class.html) supplies selection/cursor/input foundations. [Super Editor](https://github.com/superlistapp/super_editor) is an existing configurable document-editor toolkit to evaluate. Its README includes a June 2024 release/API-evolution note; check current releases and APIs before selecting a version rather than treating that old note as evidence of current abandonment or stability.

The Dart [pdf package](https://pub.dev/packages/pdf) supplies a separate PDF creation/widget system. Its Flutter-like API is not Flutter's on-screen layout engine. Choosing this path requires proving equivalent font metrics, line wrapping, pagination, and element positioning or arranging a genuinely shared layout result. A screen capture would not satisfy the required selectable, properly formatted screenplay PDF. Flutter is retained as a serious option, with this editor/export integration question unresolved.

Tauri's [process model](https://v2.tauri.app/concept/process-model/) combines a Rust core with webviews and uses WebKitGTK on Linux. It can reuse an open-source structured web editor and ordinary web styling while Rust handles local project operations. Native executables and a smaller distributed package do not establish total runtime memory or latency.

Tauri's [WebviewWindow API](https://docs.rs/tauri/latest/tauri/webview/struct.WebviewWindow.html) documents print-dialog behavior and platform-webview access; it does not establish an Electron-equivalent, controlled PDF-to-file workflow for Apollo on Linux. [WebKitGTK PrintOperation](https://webkitgtk.org/reference/webkit2gtk/stable/class.PrintOperation.html) exposes print settings/page setup and printing, so a native integration path exists to investigate. This is an unresolved implementation task, not a claim that Tauri cannot export PDF. Check compatibility with the installed WebKitGTK/Tauri versions and validate embedded fonts, text, margins, and page equality.

For Tauri, preserve frontend responsiveness by using incremental local editor transactions and a deliberate autosave/checkpoint protocol. Do not send the entire screenplay across the Rust boundary on every keypress. A proposed PDF path that adds a complete extra browser runtime must count that runtime in its resource and packaging assessment.

## Qt candidate: earlier recommendation

The original proposal was Qt 6 with a QML/Qt Quick interface and C++20 core, reusing Qt's text editing/layout foundations. Its proposed export design uses one Qt-based layout path for the editor and PDF rather than separately calculating screen and export pagination.

This is a middle ground between a browser application and building text interaction from low-level drawing/input primitives. It provides direct access to Qt's text APIs and room for deliberate visual design. It does not make the screenplay editor a drop-in component, and C++ alone does not establish responsiveness.

| Responsibility | Qt candidate components | What Apollo must add |
| --- | --- | --- |
| App interface | Qt Quick Controls/QML with a custom visual style | Home, navigation, notes/stash, history and merge screens |
| Core and editor integration | C++20 and Qt APIs | Screenplay commands, structured content, incremental updates |
| Text editing | Qt Quick TextEdit and QTextDocument as the first candidate | Element behavior, page presentation, dual dialogue, consistent command/undo semantics |
| PDF | Qt QPdfWriter/QPainter using the same approved layout decisions and fonts | The submission profile, page numbers, continuations, title page, verification |
| Source data | Versioned, stable text serialization; evaluate structured JSON | Element IDs/types, emphasis, cue extensions, migrations, notes/stash; exact schema/layout open |
| Git | Installed Git executable through asynchronous QProcess calls | Draft-local work, whole-project checkpoints, sync, comparison and resolution UX |
| Spelling | Hunspell with US English dictionary | Underlines, explicit suggestions, persistent personal words |
| Build/checks | CMake/Ninja, Qt tests, formatting and sanitizer checks | A small project tool setup and behavior-driven integration fixtures |

Qt supports [customizing Quick Controls](https://doc.qt.io/qt-6/qtquickcontrols-customize.html), [rich-text interaction](https://doc.qt.io/qt-6/qml-qtquick-textedit.html), [access to the underlying document from QML](https://doc.qt.io/qt-6/qquicktextdocument.html), and [PDF painting](https://doc.qt.io/qt-6/qpdfwriter.html). Its [Wayland client integration](https://doc.qt.io/qt-6/qtwaylandclient-index.html) supplies the relevant Linux platform path. These capabilities support the recommendation; none proves Apollo's eventual appearance or performance.

[QProcess](https://doc.qt.io/qt-6/qprocess.html) can run Git without a new embedded Git library. [Hunspell](https://github.com/hunspell/hunspell) supplies C/C++ APIs and spelling suggestions. Qt documents [CMake builds](https://doc.qt.io/qt-6/cmake-manual.html). These are component fits, not tested integrations.

## Alternatives and trade-offs

| Option | Relevant strengths | Assessment for Apollo |
| --- | --- | --- |
| Qt/C++/QML | Direct text/layout/PDF APIs, customizable presentation, Wayland integration | Native candidate; prove paginated editing and weigh C++ maintenance/build complexity |
| Electron/TypeScript with a structured web editor | Browser editing ecosystem; Chromium PDF API | Development-effort candidate; assess runtime size, memory and latency against actual targets |
| Tauri/Rust with a web editor | Rust core and system webview; web UI flexibility | Viable, but Linux WebKitGTK editing and export path need independent validation |
| Flutter/Dart with an existing document editor | Native Linux target, custom UI, Dart editor packages | Serious candidate; verify desktop editor behavior and screen/PDF layout equivalence |
| PySide6/QML | Official Python access to Qt with the same native text foundation | Reasonable for rapid development; Python is not automatically slow, but direct C++ integration is preferred here to keep editor work in one native codebase |
| Rust with Qt bridging | Rust core can interoperate with Qt | Reasonable if Rust is a strong preference; bridge/build integration adds work without a stated language preference |

Electron [embeds Chromium and Node](https://www.electronjs.org/docs/latest/), and its [printToPDF API](https://www.electronjs.org/docs/latest/api/web-contents#contentsprinttopdfoptions) supports a controllable export path. That architecture is a packaging/runtime trade-off, not evidence that Electron typing is necessarily slow. ProseMirror is an example of an existing [structured web editor](https://github.com/ProseMirror/prosemirror-view); its GitHub repository points to its current hosting, so follow the current upstream if evaluating it further.

Tauri's [Linux webview is WebKitGTK](https://v2.tauri.app/reference/webview-versions/). Using Rust in its core does not replace the web editor/rendering layer. A smaller distributed application package alone does not establish lower memory consumption or faster editing. For Apollo, its PDF path requires more investigation before recommendation over Qt.

[PySide6 is the official Qt Python binding](https://doc.qt.io/qtforpython-6/). Direct C++ integration with Qt is a maintenance trade-off rather than a measured speed advantage over bindings. Rust routes now have an initial assessment above; this comparison is still not exhaustive.

## Critical limitation: editable pagination

Qt's text engine supports paginated documents, but its generic editor controls are not complete paginated screenplay editors. The [QTextDocument documentation](https://doc.qt.io/qt-6/qtextdocument.html) distinguishes finite page size from continuous text width, describes device-dependent units and design metrics, and explains that unpaginated documents may be repaginated by its convenience print method. Those defaults must not silently determine Apollo's format.

The [Qt Quick editor source for Qt 6.12](https://raw.githubusercontent.com/qt/qtdeclarative/6.12/src/quick/items/qquicktextedit.cpp) updates document text width as part of size handling. That interaction must be tested with a finite page size, fixed logical page width, resize/zoom, cursor mapping, and selection. Supplying a QTextDocument does not itself prove page gaps and live editing will behave correctly.

Use public APIs. Avoid a dependency on Qt private text-control classes or a local Qt fork as the starting architecture. Qt Widgets/QTextEdit is a possible fallback within Qt, but is also not a ready-made paginated editor. Changing to it requires reevaluating the shell/editor boundary; embedding widgets into a QML scene is not a trivial replacement. Qt documents [rendering costs for QQuickWidget](https://doc.qt.io/qt-6/qquickwidget.html#performance-considerations), which embeds Quick in a widget application in the other direction.

Preferred constraint: retain an existing text interaction control for caret, selection, clipboard, navigation, and undo. Build screenplay behavior and layout coordination around it. If the prototype instead requires a substantial new text-control implementation, report that cost and revisit the stack before building the full app.

## Architecture guardrails

These are design recommendations for the forthcoming technical spec. Qt-specific API examples apply to the Qt candidate; web-editor and Flutter candidates must meet the same semantic, persistence, and layout contracts through their editor and platform APIs.

- Keep screenplay meaning explicit: typed elements, stable identifiers, styled text spans, base character names and per-cue extensions. Do not make generated HTML, PDF layout objects, or screen coordinates the sole durable source of truth.
- Keep the document model and Qt editor projection in step through well-defined edit operations. Prove grouping/undo across text edits, type changes, stash moves, and scene moves before committing to a second command history or a per-element editor design.
- Share page-layout decisions, fonts, and metrics between writing and PDF output at one document revision. Generated page continuations stay distinct from author-entered content. Export must wait for or compute the current revision, not use stale pagination.
- Apply incremental changes. Do not replace or serialize the entire screenplay through QML bindings on each keystroke; Qt itself recommends fine-grained updates for [large rich-text content](https://doc.qt.io/qt-6/qml-qtquick-textedit.html#text-prop).
- Keep expensive parsing, spelling work, persistence and Git/export jobs off the interaction path. Use immutable snapshots or another explicitly safe threading design; do not mutate a live GUI-owned document from a worker. Qt's [performance guidance](https://doc.qt.io/qt-6/qtquick-performance.html) favors event-driven work and short GUI-thread tasks.
- Treat project persistence and sync as a separate module with small operations and explicit results. A successful write of one file is not an atomic checkpoint of several files or branches. [QSaveFile](https://doc.qt.io/qt-6/qsavefile.html) is a useful file-write primitive, not a complete recovery protocol.
- Ordinary Git branches version repository-wide trees. Shared project notes/stash need an explicit storage strategy so switching a draft cannot rewind shared content. Do not claim the agreed behavior follows automatically from putting all files in one repository.
- Keep core behavior local. No hosted backend, account system, collaboration service, plugin system, or application server is required for the agreed v1. GitHub remains optional remote storage reached by manual Sync.

## Shared technical prototype: pass/fail gate

This is planned feasibility work, distinct from the later visual prototype and from production implementation:

Before a large implementation, establish two forms of evidence: a visual prototype that demonstrates the owner's aesthetic and a technical editor/export prototype. The appearance work does not prove pagination, and a headless export test does not prove an enjoyable writing interaction. Prefer the Qt and Tauri shortlist initially; retain Flutter as an alternative rather than committing to three full builds.

1. Reuse an existing editing control (Qt text control, open-source structured web editor, or Flutter document editor) for a formatted screenplay with scene headings, action, dialogue, cue extensions, emphasis, and dual dialogue. Exercise text selection, paste, undo, element changes, and Enter/Tab behavior across page breaks.
2. Show genuine page boundaries and numbers; handle insertion/deletion near a page edge, long speeches, generated continuations, dual dialogue crossing pages, and manual breaks. Resize and zoom must not change the export profile.
3. Export the same fixed revision through the candidate's PDF path (Qt PDF, verified Tauri/WebKitGTK integration, Flutter PDF layout, or Electron printToPDF). Check body-page count, line/page breaks, font embedding/metrics, text, element positions, and title-page numbering against the numeric profile and independent fixtures. Preserve text as selectable PDF content. A shared rendering engine alone does not establish identical screen/print layout.
4. Test 200-page and 500-page documents on the target computer under native Wayland. Record input-to-paint latency, navigation, memory, startup/open time, reflow behavior, and PDF time, with no live Git/spelling job blocking typing.
5. Record the integration approach, development complexity, runtime costs, and limitations. Pass only if the existing editing foundation is retained and the agreed pagination/interaction requirements are credible. Compare the other candidate if a premise fails. Do not spend weeks concealing a failed premise behind custom editor infrastructure.

No feasibility result has been established. The prototype still needs an exact font/numeric profile from the [export note](export-profile.md), a representative content fixture, and a way to measure interactive rendering.

## Proposed performance budgets

Agent-selected planning targets to validate and refine after the prototype; these are not existing measurements or newly promised hard guarantees. The owner accepted the workload sizes and target machine, not these exact numbers.

| Measurement, 200-page workload | Initial target |
| --- | --- |
| Input event to first painted text change | p95 at or below 33 ms; investigate any typing stall above 100 ms |
| Jump to an already indexed scene | p95 at or below 100 ms |
| Warm open of a local screenplay | At or below 1 second |
| Cold launch to a usable writer for a local screenplay | At or below 2 seconds |
| PDF export | At or below 5 seconds without freezing writing |
| Settled app memory with the screenplay open | At or below 250 MiB summed proportional set size across app-owned processes |

Record release build, display refresh/scaling, font files, fixture revision, cache state, and measurement method. Measure GUI processing separately from end-to-end input latency; timers around document mutation alone do not establish what the owner sees. Cold-launch disk-cache conditions need an explicit procedure rather than an assumed label.

The 500-page stress case must preserve content, avoid hangs/crashes, remain usable, and show bounded memory growth; record its results separately rather than assuming all 200-page budgets apply unchanged. The agreed one-second healthy-storage application-crash recovery target remains a separate persistence requirement.

## Environment and open work

Read-only checks found Qt Core/Gui/Quick and Arch Qt packages at 6.12.0 on the target host, Git 2.56.0, and CMake/mise available. Ninja was not on PATH. These observations show available dependencies, not compatibility or speed. No dependencies were installed.

Before scaffolding, select the candidate and pin supported tool/dependency versions in a reproducible local build consistent with the project's mise preference. A TypeScript candidate must follow the existing pnpm lockfile, install-script allowlisting, and release-age requirements. Keep system-package use explicit. Select the font and US English dictionary assets, parser approach, source schema, recovery/checkpoint protocol, shared-material Git strategy, and Arch install/package path. Add dependencies only where the selected platform/standard tools do not cover the need.
