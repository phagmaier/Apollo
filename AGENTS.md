# Apollo

Apollo is a Linux desktop screenwriting app for its owner, targeting Omarchy on
Arch Linux with Hyprland. The repository currently contains planning documents,
fixtures, and reference tools; it does not yet contain the application or a
build system.

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

- There are currently no application build, test, or CI commands. When the spike
  introduces them, document the actual commands here and keep them current. Use
  the tool and asset pins in the spike brief; record deviations and upgrades.
- Existing Python reference tools use `uv run`, for example
  `uv run tools/generate_script.py --help` and `uv run tools/pdf_lines.py --help`.
  PDF inspection requires Poppler utilities. See the fixture guide for complete
  commands and prerequisites.
- Verify layout against the hand-derived fixtures and the independent Academy
  reference as described in the brief. Agreement between two outputs produced
  by the same engine alone is not an independent correctness check. Report
  missing references, manual checks, and unmeasured performance honestly.
- `reference/` contains local copyrighted or personal material and is ignored by
  Git. Generated workloads are also ignored; see the fixture guide for how to
  create them. Preserve fixture and third-party license information.
