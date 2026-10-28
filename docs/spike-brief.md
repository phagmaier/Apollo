# Apollo — editor/export feasibility spike brief

Status: implementation, technical checks, and the [spike report](spike-report.md) are prepared for owner assessment. The owner approved visual direction B with C's expandable outline ([prototype](../prototype/editor.html)); acceptance of its QML reproduction remains pending. The provisional stack is Qt 6 + C++20 + QML ([stack recommendation](stack-recommendation.md)). The owner still decides whether to keep it or switch/reassess after inspecting the result. The criteria below are unchanged.

## The question

Can a QML editor built on `QTextDocument` with an Apollo-owned layout engine show the screenplay's real pages, edit comfortably across page breaks, and export a PDF that matches the editor line for line? It must do this at 200 and 500 pages on this machine, without forking Qt or using private Qt APIs.

The spike is not the first Apollo release. Its code may be thrown away. Two parts are intended to be kept, if they pass review:

- the layout engine and its tests;
- the fixtures and tools.

## In scope

| Part | Contents |
| --- | --- |
| Layout engine (`engine/`) | Pure C++20 with no GUI dependency: a screenplay model to lines and pages, implementing the [profile](export-profile.md) (wrapping, spacing, B1–B9, generated CONT'D, dual dialogue, title page) |
| Minimal Fountain import (`fountain/`) | Only the constructs in `fixtures/layout/` and the generated workloads. The full mapping is later work. |
| Editor (`editor/`) | `QTextDocument` as the live model; `ScreenplayLayout` (`QAbstractTextDocumentLayout` subclass) positioning `QTextLine`s at the engine's breaks; a QML editor item handling keys, mouse, input method, clipboard, and painting. First, a short probe of QML `TextEdit` with the custom layout. |
| PDF (`export/`) | `QPdfWriter` painting the engine's pages with embedded Courier Prime |
| Shell (`app/`) | One QML window: the editor plus one screen reproducing an approved visual mockup, to show the look carries over |
| Tests (`tests/`) | Engine against `fixtures/layout/*.expected`; PDF through `tools/pdf_lines.py`; editor through Qt Test on the `offscreen` platform |

## Out of scope

The on-disk source format, autosave, Git, notes and stash, spelling, find/replace, suggestion menus, the scene navigator, the home screen, packaging, and settings UI. Do not design these during the spike.

## Tool and asset pins

| Item | Pin | Source |
| --- | --- | --- |
| Qt | System packages `qt6-base`, `qt6-declarative`, `qt6-wayland`, `qt6-svg` **6.12.0** (pacman) | Installed. Qt stays a system dependency because mise does not manage it. Record any pacman upgrade in the spike report. |
| Compiler | GCC 16.2.1 (system) for builds; Clang 23.1.3 (system) for `clang-tidy`/`clang-format` if used | Installed |
| C++ standard | C++20 | |
| Build | CMake 4.4.4, Ninja 1.13.2 via `mise.toml` (`[tools] cmake = "4.4.4"`, `ninja = "1.13.2"`) | Versions listed by `mise ls-remote` |
| Sanitizers | Debug preset with `-fsanitize=address,undefined` | |
| Font | Courier Prime TTFs from `quoteunquoteapps/CourierPrime` commit `7fd585a2dd4c1612c79b3308e300923d1c13df93`, checked by SHA-256 at build time (hashes below) | Downloaded during planning; OFL-1.1, ship `OFL.txt` |
| Python tools | `uv run tools/…` (standard library only) | |
| PDF inspection | poppler `pdftotext`, `pdffonts`, `pdfinfo` (installed) | |

Courier Prime SHA-256:

```
72f793376f8e2841656bf21d77a5de010f2929bd6956a22ee848ad0c7eb978af  CourierPrime-Regular.ttf
ff1f38786c849d1c41fa8e447960abdb2bd75fdfb0cfcdeb524fad65a5af3638  CourierPrime-Bold.ttf
f1b9a5829789f7e56432a9f3bc7665ef4531dbba1c112639e48bef39621a006b  CourierPrime-Italic.ttf
3355273f3ea6d6362658f9564f4e83d59e156cf1a182e3c1592b5c5943627dcc  CourierPrime-BoldItalic.ttf
```

## Editor behavior under test

Only the subset the spike needs. Everything else in the spec's keyboard section stays open.

| Situation | Behavior |
| --- | --- |
| Enter at the end of an element | New element below: dialogue after a cue; dialogue after a parenthetical inside a speech; otherwise action |
| Enter in the middle of an element | Split it into two elements of the same type |
| Enter on an empty element | Change it to action (provisional, so the spike has a defined behavior) |
| Tab with no menu open | Cycle the current element through scene heading → action → character → parenthetical → dialogue → transition; Shift+Tab cycles backward |
| Typing in a cue or heading | Displayed uppercase; the document keeps what was typed |
| Paste of plain text | Inserted as action paragraphs at the cursor. Paste inside a dialogue element stays dialogue. |
| Paste of an Apollo selection | Element types and emphasis preserved |
| Undo/redo | Typing (word-grouped), element-type change, and scene move are each one undo step that restores the exact prior document |
| Scene move | A test command moves a whole scene (heading through the element before the next heading) above the previous scene |

Two rules apply throughout. Page continuations (MORE and the CONT'D labels) are painted by the layout and are never document text. Caret and selection move through page gaps and dual-dialogue columns in document order.

## Pass/fail checks

| # | Check | Pass |
| --- | --- | --- |
| 1 | **Engine fixtures** | All eight `fixtures/layout/*.expected` match exactly. Academy pages match `reference/academy/expected.tsv` page by page except the four listed deviations. |
| 2 | **Editor/PDF agreement** | For every fixture and both generated workloads, the editor's line dump (page, slot, element, text) equals the PDF's (`tools/pdf_lines.py`). This still holds after 50 scripted edits that push content across page boundaries. |
| 3 | **PDF quality** | 612×792 pt; `pdffonts` shows Courier Prime embedded with Unicode mapping; `pdftotext` extracts the text; x positions are within 0.01 in of the profile. |
| 4 | **Interaction** | Qt Test scripts plus your manual session cover: typing across a page gap without caret jumps; shift-selection and copy across a page gap and through dual dialogue; the Enter/Tab table; both paste kinds; undo/redo of typing, type change, and scene move; a dead-key or compose character through fcitx5 under native Wayland. |
| 5 | **Responsiveness** (release build, native Wayland, this machine) | Key event to `QQuickWindow::frameSwapped`: p95 ≤ 33 ms with no stall above 100 ms on the 200-page workload while typing at a page boundary. Opening the 200-page workload ≤ 1 s warm. PDF export ≤ 5 s without freezing typing. Memory PSS ≤ 250 MiB. The 500-page workload stays usable, and its numbers are recorded. These are the targets from [stack.md](stack.md#proposed-performance-budgets), not promises. |
| 6 | **Code shape** | No Qt private headers, no copied Qt sources, and the engine builds and tests without QtGui. |
| 7 | **Look** | The QML screen matches the approved mockup's key qualities in your judgment. |

## Switch triggers

Switch to the Electron + ProseMirror fallback, or reassess, if any of these happens:

- Check 4 cannot pass without private Qt APIs or a fork of Qt's text control.
- Caret, selection, or input-method defects across page gaps persist after focused fixing, while the controller keeps growing toward a general text-control rewrite.
- Check 5 misses because of Qt's text stack rather than Apollo code (shown by profiling).
- Check 7 fails for reasons QML cannot address.

Checks 1–3 failing is an engine bug, not a reason to switch, because they are stack-neutral.

## Order of work

1. Scaffold `mise.toml`, CMake presets, the font fetch with hash check, an `AGENTS.md` with build and test commands, and CI-style local check commands.
2. Engine plus the fixture tests (check 1).
3. PDF export (check 3), then engine/PDF agreement on the workloads.
4. Probe QML `TextEdit` with the custom layout for a short, fixed time; record the result.
5. Editor item, then checks 2 and 4.
6. Measurements (check 5) and the QML look screen (check 7).
7. Spike report in `docs/spike-report.md`: results, measurements with their method, the integration approach, open problems, and a keep/switch recommendation.

## What you will be asked to do

- Pick a visual direction from the HTML mockups before step 6.
- A short manual writing session at step 5: type, select, and undo across page breaks, and try a dead key or compose sequence.
- Read the spike report and decide keep or switch.
