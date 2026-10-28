# Apollo

Apollo is a Linux desktop screenwriting app for its owner, targeting Omarchy on
Arch Linux with Hyprland. The repository contains planning documents, fixtures,
reference tools, and an early feasibility-spike build; it is not yet an app.

## Read before working

- [Product specification](docs/spec.md): confirmed product behavior and scope.
- [Planning](docs/planning.md): owner decisions, proposals, and open questions.
- [First writing build plan](docs/writing-build-plan.md): agreed writing scope,
  settled product decisions, proposed implementation slices, and verification.
- [Spike brief](docs/spike-brief.md): current feasibility work, scope, tool pins,
  order, and pass/fail checks.
- [Export profile](docs/export-profile.md) and [fixture guide](fixtures/README.md):
  layout rules, independent expectations, and verification procedures.
- [Stack recommendation](docs/stack-recommendation.md): technical
  direction and fallback; [Git design](docs/git-design.md) covers later history
  and sync work.

Some older sections describe decisions as open that were settled later. Prefer
the latest owner-confirmed decisions and the specific current brief over earlier
research or proposals. Keep confirmed requirements, delegated implementation
choices, and unverified assumptions distinct; update the relevant documents as
decisions or evidence change.

## Current direction

The owner accepted the QML Writing desk foundation and chose to keep Qt 6,
C++20, and QML on 2026-10-28 after the editor/export feasibility spike. The
screenplay layout engine is shared by the editor and PDF export. Keep the
spike report's measured limits and known gaps explicit; this is not validation
of a production app. Electron and ProseMirror remain the fallback if a later
limitation warrants reassessment.

The spike is an experiment, not the first release. Follow its stated boundaries:
source schema, autosave, Git integration, notes, stash, and other product
features were outside its scope. The first usable writing milestone now has a
requirements plan in `docs/writing-build-plan.md`; follow its confirmed
requirements and keep unvalidated engineering proposals explicit. Its interview
is complete; detailed contracts and validation belong to the implementation
slices. Planning is not authorization to implement. The accepted visual
foundation can be refined as screens develop; the owner has not supplied or
frozen every visual detail.

## Working and verification

- Install the tools in `mise.toml` using `mise install`. Run `mise run check` for
  the release configure/build/test cycle and `mise run check-debug` for the
  sanitizer build. They check engine lines and point geometry, the fixture
  PDFs, and engine/PDF agreement on regenerated workloads. Run `mise run
  check-engine` for the engine without Qt or PDF tooling. The Qt checks also run
  the offscreen editor tests and editor/PDF agreement, including 50 scripted
  edits on each workload; `build/release/apollo_editor FILE` opens the editor,
  and `--measure` times synthetic typing. Passing these checks does not
  establish native-Wayland interaction, input methods, or performance.
- The pinned Courier Prime fonts and OFL license are in `assets/courier-prime/`;
  the build checks their hashes. Use the other pins in the spike brief and
  record deviations and upgrades.
- Existing Python reference tools use `uv run`, for example
  `uv run tools/generate_script.py --help` and `uv run tools/pdf_lines.py --help`.
  PDF checks require Poppler's `pdftotext`, `pdffonts`, `pdfinfo`, `pdftohtml`,
  and `pdftoppm`. See the fixture guide for complete commands and prerequisites.
- Verify layout against the hand-derived fixtures and the independent Academy
  reference as described in the brief. Agreement between two outputs produced
  by the same engine alone is not an independent correctness check. Report
  missing references, manual checks, and unmeasured performance honestly.
- Before a manual handoff, test the commands and the relevant native execution
  path yourself when possible. Put longer command sequences in one executable
  `.sh` runner with clear manual checkpoints where needed, environment checks,
  failure handling, and saved output; the owner prefers running one script. Use
  `tools/check-wayland.sh` for native interaction, compositor/fcitx5 input,
  rendering, typing, and PDF-export-while-typing checks (`--preflight` omits
  full typing measurements).
  Automate the checks available to the agent; leave subjective comfort and
  appearance acceptance to the owner and state what was actually verified.
- The bounded Writing desk shell is in `app/WritingDesk.qml`. Use
  `tools/inspect-writing-desk.sh` for the prototype/QML comparison, or `--smoke`
  for its automated native launch/outline/export check. See
  `docs/spike-report.md` and `docs/spike-handoff.md` for the owner's acceptance
  and the remaining limits. The opening scene list is a read-only visual
  preview, not the product scene navigator. Undo is Ctrl+Z; redo is Ctrl+Shift+Z.
- Project folders and autosave follow [source-format.md](docs/source-format.md).
  `build/release/apollo_editor DIR --create [FILE.fountain]` creates a project
  and `apollo_editor DIR` opens it with autosave; a Fountain path still opens
  unsaved. `autosave_crash_tests` kills continuous typing at each save phase;
  the format document has the workload measurement command.
- `reference/` contains local copyrighted or personal material and is ignored by
  Git. Generated workloads are also ignored; see the fixture guide for how to
  create them. Preserve fixture and third-party license information.
