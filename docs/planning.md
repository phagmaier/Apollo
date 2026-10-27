# Apollo — planning interview

Status: ongoing. The [product specification](spec.md) holds confirmed intent and requested capabilities. This file tracks recommendations, unanswered choices, and the work needed to make the spec actionable.

## Working method

Use the grill-with-docs workflow: ask a few related questions per round, explain consequences and recommendations, wait for answers, and update the documents as decisions settle. Do not turn silence into agreement. Separate owner decisions, delegated agent choices, proposals, and deferred work.

## Initial discovery

The workspace contained no application code or project documents when planning began. There is no existing implementation or stack to preserve.

The owner has already established the intended user, operating system, primary qualities, required export formats, and a broad feature inventory. Those questions do not need to be asked again.

## Round 1 — settled answers

Owner-confirmed:

1. Features are the primary writing use case; television support would be helpful; shorts are a possibility. Production/shooting-script features were not resolved by the answer.
2. Use a formatted screenplay editor. Fountain is an export format and its markup should not appear during writing. A custom internal format is acceptable if suitable; none has been selected.
3. The owner likes a macOS-style aesthetic and named Bear, iA Writer, StudioBinder, Superhuman, and Things 3 as visual references. Evaluate the look through a later prototype. A dark theme or any particular page color has not been chosen.
4. Industry-standard PDF export was explicitly reaffirmed as mandatory alongside Fountain export.

## Round 2 — settled answers

Owner-confirmed:

1. Editor pages and page numbers must match the exported PDF. Responsiveness remains a primary requirement, so validate the layout strategy before committing to a stack.
2. Enter and Tab should provide sensible element behavior. The next element normally defaults to action; after a character cue it is dialogue, and after a dialogue parenthetical beneath the cue it is dialogue. Detailed keyboard and empty-element behavior remains open.
3. Existing scripts are in Fountain. Converting them into Apollo's internal representation is required. No additional import format was requested.
4. Offer character-name and scene-heading suggestions. Typing `(` after a character name should offer extensions such as `V.O.` and `O.S.`.
5. Extensions do not create new character identities: JOHN with a voice-over extension is still JOHN. The spec distinguishes this cue extension from a separate dialogue parenthetical.

## Round 3 — settled answers

Owner-confirmed:

1. Accept the proposed suggestion behavior: current-script names, arrow-key selection, Tab acceptance, Escape dismissal, and Tab changing element types when the menu is closed. New names remain allowed.
2. Scene-heading suggestions work in pieces only. Do not include the proposed complete-heading shortcuts.
3. Saving is automatic. The answer did not explicitly settle named checkpoints or crash-recovery guarantees; keep those separate from the confirmed save behavior.

## Round 4 — settled answers

Owner-confirmed:

1. Accept the proposed Save Version action backed by Git commits, with comparison and restoration in an app history. Automatic saving continues between named versions.
2. Support parallel alternative drafts using the proposed Git-branch approach.
3. Sync with GitHub through a manual Sync button.

The owner also asked whether standard Git is the best fit for screenplay versioning. The [Git design note](git-design.md) records the researched recommendation and its limits. The owner accepted the recommendation in round 5.

## Round 5 — settled answers and product rename

The owner accepted all three recommendations:

1. One project folder and repository per screenplay, including its notes and stash, with a private GitHub repository by default when connected. Drafts, history, and synchronization remain independent between projects.
2. Switching drafts automatically preserves unfinished work with the draft being left and restores it on return, without requiring a named version first. The implementation must satisfy this contract without silently moving or discarding edits.
3. Manual Sync includes the latest autosaved writing, using a dated automatic checkpoint when needed, distinguished from deliberately named versions. Exact commit/ref structure, history presentation, and sync of other drafts remain open.

The owner also accepted the recommendation to use standard Git with screenplay-aware presentation and renamed the product from Beat to Apollo. No source code or package identifiers existed to rename; the project documents now use Apollo.

## Round 6 — settled answers

Owner-confirmed:

1. Notes and stash are shared across all drafts of a screenplay, with history preserved.
2. Manual Sync includes the whole project: all drafts, notes, stash, and unfinished writing in inactive drafts.
3. Preview merges before application. Resolve competing passages through formatted writing, with choices to keep either version or edit a combined result; cancellation preserves originals. The owner emphasized making this especially friendly and easy for a writing workflow.

Implementation implications still open: shared material across Git branches, checkpoints across inactive drafts, merge granularity, interrupted sync, and conflicts in shared material. These are design tasks, not changes to the agreed behavior.

## Round 7 — settled answers

Owner-confirmed:

1. Accept the complete proposed stash workflow: explicit Copy/Move to Stash, preserved screenplay formatting, optional title/annotation/tags, browsing/search/filtering, and reinserting a copy while retaining the entry. Ordinary deletion does not automatically fill the stash.
2. Named notes, searchable by name/title. Basic note formatting is part of the accepted proposal; exact controls remain open. Links into the screenplay are tentative: the owner said they might be useful but is unsure. Evaluate links in the prototype rather than treating anchoring or inline comments as required.
3. App notes and stash do not accompany either Fountain or PDF exports. Whole-project GitHub sync still includes them.

Preservation of existing Fountain inline annotations and omitted material is a separate import/export compatibility decision.

## Round 8 — settled answers and language scope

Owner-confirmed:

1. Use American feature submission/spec-script formatting by default: US Letter, 12-point Courier, with layout checked against the Academy reference. The owner did not identify an additional studio-specific requirement.
2. Production tools (locked pages, inserted page numbering, revision colors, production scene-number management) can come later.
3. Use a title-page form for title, writing credit, author(s), source/based-on credit, contact, and draft date. Apollo handles placement, and exports can include or omit the title page.
4. Initial language scope is English. Other languages can come later. No emoji support is required; the owner wants text. This does not settle whether unexpected characters should be preserved or rejected on import, nor does it require ASCII-only storage.

The [export profile working note](export-profile.md) records source findings and the remaining layout design work. The exact font, complete numeric profile, fixtures, and rendering engine are not yet selected.

## Round 9 — settled answers

Owner-confirmed:

1. Use US English spell checking, underline potential misspellings, offer replacements on demand, and allow a personal dictionary for names/invented words. Do not automatically replace words.
2. Apply element spacing, indentation, and uppercase character cues/scene headings automatically. Flag ambiguous problems with an undoable suggested correction. Do not silently rewrite dialogue or action.
3. The collapsible scene navigator supports click-to-navigate and drag-and-drop reordering of complete scenes, with Undo available.

Detailed diagnostics, personal-dictionary storage/sync, scene boundaries, and drag interaction remain design work.

## Round 10 — settled answers

The owner accepted all three proposals:

1. Continuous local saving targeting at most one second of lost writing after an application crash with healthy storage. Resume the last project/draft/cursor from the latest completed save. Show quiet saving/saved status, surface failures, and offer Retry or Save a Copy. Exact durability and power-loss guarantees require technical design and verification.
2. Preserve current unfinished work before restoring an earlier screenplay as a new version. Retain later history and leave shared notes/stash unchanged.
3. Reversible project archiving; explicit local deletion is separate. Neither action deletes the GitHub repository.

## Round 11 — settled answers

Owner-confirmed:

1. Dual/simultaneous dialogue, bold/italic/underline, centered text, lyrics, and manual page breaks are standard required capabilities, alongside the previously requested elements. Include editing and Fountain/PDF support from the start.
2. Accept converting imported Fountain inline notes to Apollo notes and omitted passages to stash entries, preserving text and reporting the conversion while leaving the original untouched. Converted material follows app note/stash export exclusion. The owner also permits special representations for notes/stash if a custom format is selected; no storage format has been chosen.
3. Features and shorts initially; dedicated television templates/layouts can come later. Manual act labels and page breaks remain available.

## Round 12 — settled answers and delegated research

Owner-confirmed:

1. Accept staged delivery: first the formatted writer, Fountain import/export, accurate pagination/PDF, and autosave/recovery; then notes/stash and the full Git workflow. The owner can use the earlier writing build; all agreed core features still belong in full v1.
2. Accept a 200-page primary workload and 500-page stress case on the current computer. A 500-page script is not an expected everyday document.
3. No language/toolkit preferences. The owner explicitly requested help evaluating the stack and how low-level to go, emphasizing avoiding a bloated/slow result while also avoiding a text editor written from scratch in C.

Read-only host observations: i5-9400 (6 logical CPUs), about 12 GiB RAM, UHD Graphics 630, x86_64, Omarchy; Qt Core/Quick/Gui and installed Arch Qt packages report 6.12.0, and Git reports 2.56.0. CMake and mise are available; Ninja was not found on PATH. Nothing was installed or benchmarked.

## Technical evaluation — agent recommendation

The [stack evaluation](stack.md) initially recommended Qt/C++/QML, then reopened development-effort weighting. After the owner's clarified native preference and explicit Flutter/Tauri suggestions, the agent shortlists Qt and Tauri, retains Flutter as a serious alternative and Electron as fallback, and assesses native Rust routes. A common editor/pagination feasibility gate remains. Proposed performance budgets are targets, not measurements or owner-approved numeric guarantees.

No owner answer is required to invent library internals. Remaining consequential design choices need a recommendation or prototype result: source format, project-wide shared material across draft branches, recovery/checkpoint representation, and the final editable pagination strategy. A visual prototype is still needed for appearance, tentative script links, and conflict interaction. This planning session has not authorized a full production build.

## Round 13 — development effort and runtime trade-off

The owner asked whether Qt/C++ would be too difficult or overkill and explicitly said they were not against it. This is a request to reassess effort, not a rejection of native development or approval of Electron.

Agent reassessment: the earlier recommendation overemphasized native runtime integration relative to development/maintenance effort. Evaluate an Electron/TypeScript editor first if its runtime costs are acceptable; retain Qt as the native alternative. Screenplay pagination/PDF agreement remain difficult and unproven in both. Tiptap's open-source core is usable without its paid Pages package; do not assume free turnkey pagination or select a subscription dependency.

Resolved by the next clarification: native efficiency is preferred if it remains feasible to build, maintain, and style; Electron is acceptable if necessary. Do not ask this resource/effort preference again without new information.

## Round 14 — clarified native preference and expanded candidates

Owner-confirmed:

- Prefer fast/native/low-level implementation if manageable. C++ is acceptable; concerns are effort/time, code volume, maintenance, customization, and appearance, including when development uses agents.
- Electron is acceptable, although its possible bulk and slowness are concerns. This is not a categorical ban or an accepted Electron selection.
- Assess Rust as an option, and explicitly include Flutter and Tauri.

Agent findings are in the stack evaluation. Qt/QML supports custom visual design independently of the C++ core. Rust has genuine native toolkit routes as well as Tauri. Flutter supports native Linux apps and document-editor packages, with screen/PDF equivalence still to establish. Tauri can combine web editor/UI reuse with Rust project operations, with its Linux PDF path still to validate. No comparative effort multiplier, schedule, code size, or speed result has been established. The current shortlist and two-prototype-evidence recommendation are agent judgments, not owner selections or authorization for a full build.

## Round 15 — provisional stack decision

Owner-confirmed: adopt the [independent stack recommendation](stack-recommendation.md) for now: Qt 6 + C++20 + QML with an Apollo-owned layout engine. If the feasibility spike fails, switch to the Electron + ProseMirror fallback (or reassess). The owner asked what to specify before prototyping. Planned before the spike: record the decision, finish the numeric export profile and page-break rules, write expected-output fixtures, and write a spike brief with tool pins and the editor-behavior subset under test. Source schema, Git storage, autosave protocol, notes/stash, and merge design wait until after the spike.

## Round 16 — pre-spike preparation (agent work, owner-requested)

The owner asked for the Academy reference to set the standards and sizes, and for the pre-spike work to be done before prototyping. Completed:

- [Export profile](export-profile.md) v1: measured from the Academy guide PDF (exact wrap widths 61/35/17, 56-line pages, element columns, spacing, and break behavior), with the PDF's printer-driver distortion documented.
- [Fixtures](../fixtures/README.md): eight hand-derived layout fixtures, a local Academy reference built by `tools/academy_reference.py`, the official Fountain syntax samples, and a deterministic 200/500-page workload generator.
- An independent afterwriting cross-check configured to the Academy widths.
- [Spike brief](spike-brief.md) with tool pins and pass/fail checks.

Owner-confirmed:

1. Where the Academy pages and prose disagree, follow the sample pages: dialogue at 3.0 in, cue at 4.0 in, page number at 7.25 in.
2. Automatic (CONT'D) after action is on by default.

## Subsequent decision branches

Visual exploration: [editor prototype](../prototype/editor.html) presents three
workspace directions (Library, Writing desk, and Story outline). The owner
approved B (Writing desk) as the overall visual direction, especially its compact
left rail, with an expandable scene outline like C's that can be hidden again.
B demonstrates that combination. Individual screens and details can be refined
as they are developed; the QML implementation remains to be validated.

Ask these as their prerequisites settle, rather than presenting the entire list as a questionnaire.

- Daily workflow: creating/opening a script, resuming a session, keyboard behavior (including empty/mid-element Enter and suggestions), paste, element inference and overrides, undo/redo, and selection.
- Visual direction: use the owner's named references in a later prototype to settle typography, light/dark preferences, page presentation, density, distraction, and theme integration.
- Release boundary: staged writing/export milestone accepted; all agreed core capabilities belong in full v1. Define milestone acceptance and implementation order around the technical feasibility results.
- Document model: authoritative content, required Fountain import/export and accepted note/stash conversion, external editing, round-trip expectations, remaining unsupported/organizational constructs, character identities and extensions, renaming, and custom cue extensions.
- Persistence and recovery: settled one-second application-crash target, accurate save status, failure-copy/retry behavior, and session resumption; still define durability, scheduling, external modifications, and simultaneous app instances.
- Project organization: settled one-screenplay project/repository and reversible archiving with no remote deletion; still define detailed home-screen behavior, storage location, naming, moving, and local deletion/recovery.
- Notes and stash: settled project-wide sharing, explicit capture/reinsertion, annotations/tags, title search for notes, and export exclusion; still define detailed search/tag controls, partial selections, deletion/undo, history restoration, and tentative script links through a prototype.
- History: settled distinction between local recovery and deliberate versions, alternative drafts, and restoration as a new version after preserving current work; still define comparison granularity and shared-material restoration.
- Git/GitHub: settled whole-project manual sync and merge previews; still define detailed tracked-file layout, existing repositories, credentials, sync failures, offline behavior, remote divergence, conflict details, and safe restoration.
- Formatting assistance: settled US English and personal-dictionary behavior; still define dictionary persistence/sync, diagnostic rules and intentional exceptions. Formatting suggestions are explicitly applied and undoable.
- Export: selected American feature submission profile, title-page form, required element coverage, and note/stash conversion/exclusion; still define full numeric rules, remaining Fountain mappings, pagination, font packaging, failures, and acceptance checks.
- Performance: 200-page primary workload and 500-page stress test on the current machine accepted; proposed numeric targets are in the stack evaluation and need measurement.
- Delivery: Qt/C++/QML provisionally selected (round 15), with Electron + ProseMirror as fallback, pending the feasibility spike. Still define package/install workflow on Arch/Omarchy, dependency pins, maintenance, and license if relevant.
- Verification: representative writing tasks, reference exports, compatibility, recovery scenarios, and focused prototypes for unresolved feasibility risks.

## Research notes

Consulted 2026-10-27:

- [Official Fountain syntax](https://fountain.io/syntax/): source for screenplay markup and title-page representation. A complete compatibility inventory is still needed before selecting a parser or defining import/export acceptance.
- [Academy screenwriting resources](https://www.oscars.org/nicholl/screenwriting-resources): professional formatting allows variations. The owner selected this reference for Apollo's default submission profile; it does not establish every recipient's requirements.
- [Export profile working note](export-profile.md): the owner has now selected the Academy reference for the default profile; initial PDF reference findings and unresolved measurements are captured separately.
- [Git design note](git-design.md): standard Git supports the requested history/branch workflow; source representation and screenplay-aware merge behavior need separate design and validation.
- [Stack evaluation](stack.md): cited comparison, native-versus-development effort reassessment, pagination limitations, performance targets, and required feasibility work.

Further export work must complete and verify the selected submission profile. Stack choice remains open and requires technical validation; no source schema or merge algorithm has been selected.
