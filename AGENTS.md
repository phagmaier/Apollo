# Apollo

Apollo is a Linux desktop screenwriting app for its owner, targeting Omarchy on
Arch Linux with Hyprland. The repository contains planning documents, fixtures,
reference tools, and an early feasibility-spike build; it is not yet an app.

## Read before working

- [Product specification](docs/spec.md): confirmed product behavior and scope.
- [Planning](docs/planning.md): owner decisions, proposals, and open questions.
- [Spike brief](docs/spike-brief.md): current feasibility work, scope, tool pins,
  order, and pass/fail checks.
- [Export profile](docs/export-profile.md) and [fixture guide](fixtures/README.md):
  layout rules, independent expectations, and verification procedures.
- [Stack recommendation](docs/stack-recommendation.md): provisional technical
  direction and fallback; [Git design](docs/git-design.md) covers later history
  and sync work.

Some older sections describe decisions as open that were settled later. Prefer
the latest owner-confirmed decisions and the specific current brief over earlier
research or proposals. Keep confirmed requirements, delegated implementation
choices, and unverified assumptions distinct; update the relevant documents as
decisions or evidence change.

## Current direction

Qt 6, C++20, and QML are provisionally selected, subject to the editor/export
feasibility spike. The screenplay layout engine is intended to be shared by the
editor and PDF export. Electron and ProseMirror are the fallback under the
brief's switch conditions; do not present the Qt approach as validated yet.

The spike is an experiment, not the first release. Follow its stated boundaries:
source schema, autosave, Git integration, notes, stash, and other product
features are outside its scope. The owner will choose a visual direction and
participate in the manual editing check; record measurements and limitations
before recommending whether to keep or switch stacks.

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
- `reference/` contains local copyrighted or personal material and is ignored by
  Git. Generated workloads are also ignored; see the fixture guide for how to
  create them. Preserve fixture and third-party license information.
