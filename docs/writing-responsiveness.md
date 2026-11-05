# Current writer responsiveness

Task 5c measures ordinary editable projects through `ProjectAccessFlow` and the
actual Writing desk, with continuous autosave, spelling, suggestions, live
scene tracking and speech diagnostics. It leaves import admission and imported
PDF restrictions unchanged. The primary/stress fixtures are generated original
writing (175/444 scenes, seed 1), using the explicit spike `--create` path only
to prepare disposable projects. Only the generated title is capitalized to
meet the existing bounded Fountain contract. The preparation also removes
exact whole-lyric italic spans that the spike parser adds implicitly, producing
the model used by ordinary type authoring. Counts are reported; no user project
or import is normalized. Authored lyric italics now receive a conservative
export refusal because the independent reader changes their meaning; see the
[Fountain contract](fountain-export.md#eligibility-checked-on-every-snapshot). Their opening and subsequent writing use the
ordinary project flow.

Run `tools/check-writing-responsiveness.sh` from the repository. It checks
prerequisites, selects the owned native Wayland socket, builds/runs release
checks with isolated headless rendering settings, then measures native threaded
OpenGL with Compose by default. Set `QT_IM_MODULE=fcitx` only when its Qt plugin
and daemon are installed. Keep each disposable window visible and focused; do
not use the keyboard/mouse during measurement. `--smoke` checks the harness on
the primary workload with 12 keys per phase and makes no budget claim. The
full fcitx/compositor runner, `tools/check-wayland.sh`, also invokes the new
measurement after its existing native interaction and spike checks.

Each full workload types 600 Qt key events per phase: ordinary boundary typing,
PDF export while typing, then Fountain export while typing. The latter two
show the live scene navigator. Requests repeat whenever the real helper is
idle; no artificial helper delay is injected. Request capture and dispatch are
included in that key's latency. A frame acknowledges a key only after scene
graph synchronization following that mutation and then `frameSwapped`.
Actual helper steady-clock intervals identify overlapping keys separately.
The requested cadence is at least 30 ms per key; slow frames and instrumentation
can extend it. The report records phase durations and actual latency samples.

Autosave records mutation-to-verified-acknowledgment age, GUI capture and worker
serialization/atomic-save time. Requested-50-ms GUI-timer PSS observations cover
the editor and any observable PDF/Fountain helper; actual sampling gaps and
helper sample counts accompany maxima. Sampling pauses during GUI work and
short-lived helpers may be missed, so these are sampled maxima, not peak bounds.
The timer covers the three writing/export phases after initial spelling
convergence and before Home/reopen; startup and reopen peaks are unmeasured.
Snapshot copies, comparison models and Qt Test add harness memory overhead.

The first process launch is timed from the Python parent's monotonic timestamp
to the ready writer's synchronized frame, including Qt/font/QML startup. Each
process has fresh isolated application/QML caches, but the OS caches were warmed
by building/checks and project creation. It is a process-first observation,
not a cold-machine benchmark. Same-process Home/open to first writer frame is
the warm-reopen observation. Focus acquisition, initial spelling convergence
and verification follow the initial ready frame.

All output is private beneath a new `build/writing-responsiveness-*` directory;
XDG data/config/state/cache and session state are isolated. The harness refuses
measurement projects outside its output folder. Per-request source snapshots
and PDF line dumps are captured outside key timings. Comparison artifacts use
ordinary writes without fsync; their allocation and I/O still add harness
overhead and their bytes are independently read back afterward. Python independently
applies the recorded input prefixes to the initial saved model, checks all
snapshot identities/text/styles and the final autosave, then reads every PDF
with Poppler for page/grid/column/font/text agreement and every Fountain with
pinned aw-parser 0.1.1 for types, text, emphasis and dual relationships. Source
reopen is also checked through the actual UI. Engine/editor/PDF agreement is
supplemented by the unchanged hand-derived fixture suite in `mise run check`;
agreement alone does not establish profile correctness.

`summary.json`, per-workload `verified.json`, the original harness aggregates,
input edits, helper intervals, snapshots, PDFs, Fountain, environment/build logs
and all failures remain available for review. Missing primary budgets produce
a nonzero runner exit after results have been retained; stress results are
reported separately. A short smoke, unavailable Academy reference, physical
input, fcitx, subjective comfort, overnight memory growth and power-loss behavior
must not be inferred from a successful run.

Measurement results and completion status are recorded in the canonical
[writing plan](writing-build-plan.md#task-5c--production-export-responsiveness-and-remaining-evidence).
