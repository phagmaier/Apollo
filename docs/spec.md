# Apollo — product specification

Status: product requirements consolidated through the first writing milestone
interview, 2026-10-28. The [milestone plan](writing-build-plan.md) defines the
implementation sequence and validation gates; detailed engineering contracts
remain work for its early slices. [Planning](planning.md) records owner answers,
proposals, and later questions. Recommendations are not accepted requirements
until the owner confirms or delegates them.

## Purpose and owner

Apollo is a Linux desktop screenwriting application built for its owner's preferences, computer, and writing workflow. The target environment is Omarchy on Arch Linux with Hyprland. Supporting other operating systems or satisfying a wider audience is not a product goal. Open sourcing the application is a possibility, not a settled distribution or licensing decision.

The owner renamed the project from Beat to Apollo during planning to avoid sharing its original name with an existing screenwriting application. Apollo is the current product name throughout these documents.

The owner wants an alternative to paid screenwriting applications. The central qualities are a beautiful, comfortable writing environment, responsive interaction, essential screenplay writing tools, and reliable exports.

## Confirmed priorities

- Beautiful appearance and comfortable extended writing sessions.
- Fast, responsive interaction without noticeable typing or navigation lag. Use
  the 200-page primary and 500-page stress workloads below; the milestone plan
  carries forward the spike's measured targets and requires fresh verification
  with saving and writing assistance enabled.
- The essential tools needed to write a screenplay.
- Fountain export and industry-standard screenplay PDF export are both mandatory. The owner explicitly reaffirmed the PDF requirement. Export correctness is a primary requirement, not optional polish.
- Linux desktop operation on the owner's machine.

## Delivery and performance workload

The owner accepted staged delivery. First validate the complete formatted writing experience, Fountain import/export, accurate editor/PDF pagination and PDF export, and autosave/recovery. Then add notes/stash and the Git history, alternative-draft, whole-project sync, and conflict workflows. All agreed core features remain part of full v1; this sequencing does not remove them.

The first usable writing milestone also includes character/location suggestions
(with the previously agreed cue-extension and scene-heading behavior), US English
spell checking and a personal dictionary, find/replace, structured title-page
editing, and the working scene navigator with undoable scene reordering. The
owner reaffirmed that these are essentials and should not be cut from this
milestone. Imported notes and omitted passages have basic read-only access in
this build; the full notes/stash authoring and reinsertion workflows remain
later staged work.

The [first writing build plan](writing-build-plan.md) consolidates this milestone's
scope, proposed implementation slices, acceptance checks, and settled decisions.
Its engineering proposals are distinct from confirmed product requirements.

Use a 200-page screenplay as the primary responsiveness workload and a 500-page screenplay as a stress test on the current computer. The owner does not expect to write 500-page scripts; that size is a robustness check. The current host reports an i5-9400, about 12 GiB RAM, Intel UHD Graphics 630, and Omarchy. Budgets are in the [stack evaluation](stack.md); measured spike results and their limitations are in the [spike report](spike-report.md).

## Confirmed writing experience and direction

- Feature screenplays are the primary use case.
- The initial language scope is English. Multilingual writing features, additional dictionaries, and interface localization can come later. Emoji support is out of scope; the owner wants text. Detailed handling of unexpected imported characters remains to be specified, without assuming this means ASCII-only text or silent character deletion.
- Features and shorts use the initial screenplay editor and submission profile. Dedicated television templates/layouts are deferred; television remains a possible later addition. Manual act labels and page breaks are available initially.
- The editor displays a formatted screenplay with element-specific appearance while the owner writes. Fountain markup stays hidden; a Fountain source-editing mode is not requested.
- The owner selected a portable Apollo project folder containing versioned
  structured text as the authoritative working format. Fountain remains an
  import/export format. Exact schema, serialization, and recovery storage
  require technical design and verification. Correct Fountain and PDF exports
  remain required.
- Visual inspiration: macOS-style applications, especially Bear, iA Writer, StudioBinder, Superhuman, and Things 3. These are aesthetic references; Apollo remains a Linux application.
- The owner prefers evaluating something visible to describing visual details abstractly. On 2026-10-28 they accepted the QML Writing desk reproduction of [prototype B](../prototype/editor.html?variant=b) as the visual foundation, with its compact rail and expandable/hideable outline. Individual visual details and later screens can be refined as they develop; this does not freeze all colors, theme behavior, typography, or layout choices.

## Requested capabilities

These capabilities come from the owner's brief. Except where explicitly noted, their release order and detailed behavior remain undecided.

### Writing and formatting

- Required screenplay coverage includes scene headings, action, character cues, dialogue, parentheticals, transitions, lyrics, centered text, dual/simultaneous dialogue, bold, italics, underline, and manual page breaks. The owner considers all of these standard, required capabilities. Support them in the formatted editor, Fountain import/export, and PDF output.
- To create dual dialogue, put the cursor in the second speech and use the
  Dual Dialogue control/menu action or shortcut to pair it with the immediately
  preceding speech. Turning it off restores sequential dialogue without
  changing the words.
- Select or change element types through keyboard shortcuts and a graphical control.
- Provide a discoverable way to look up keyboard shortcuts, including Undo and
  Redo. The exact presentation and any keybinding customization remain
  unspecified. The agent schedules basic shortcut reference access in the
  first usable writing milestone; customization is not an agreed requirement.
- Use sensible next-element defaults for Enter and support Tab-based element
  selection. At the end of an element, default to action except for contextual
  exceptions: a character cue leads to dialogue, and a dialogue parenthetical
  beneath a character cue leads back to dialogue. Inside action, split at the
  cursor into two action elements. Enter on empty dialogue changes it to action;
  on an empty dialogue parenthetical, it changes to dialogue. Enter midway
  through dialogue continues the same speech on the next line, without an extra
  blank row or repeated cue. Preserve that deliberate break through editing,
  persistence, Fountain interchange, and PDF layout; do not flag it as invalid.
  Internal chunk boundaries must not change the visible speech semantics.
- With suggestions closed, Tab cycles scene heading → action → character →
  parenthetical → dialogue → transition; Shift+Tab reverses. Other types remain
  available through the element selector and keyboard shortcuts. Suggestion-menu
  Tab acceptance retains priority while a menu is open.
- Ordinary external text paste keeps the current element type; paragraphs
  pasted into dialogue remain dialogue. Apollo-to-Apollo paste preserves
  screenplay element types and emphasis. Ordinary paste does not guess Fountain
  syntax; Fountain conversion uses explicit import. Detailed handling of line
  breaks, selections, and partial elements remains to be designed.
- Apply element spacing and indentation automatically, and uppercase character cues and scene headings as normal editing behavior. Flag structural or ambiguous formatting problems with suggested corrections the owner can apply and undo. Do not silently rewrite dialogue or action. Exact diagnostics and exceptions remain to be specified.
- Dialogue without an associated character cue is a structural problem to flag
  with an undoable suggested correction. Intentional blank spacing within
  dialogue remains possible and is not silently removed; its creation controls
  remain to be specified. A normal deliberate line break is not an error.
- Edit title pages through structured fields: title, writing credit (such as Written by), author(s), source/based-on credit, contact information, and draft date. Apollo handles placement. Choose whether to include the title page in an export.
- Provide find and replace.
- Provide US English spell checking: underline potential misspellings, offer
  replacements on demand, and support a personal dictionary for names and
  invented words. Added words apply across all the owner's scripts on this
  computer. Do not automatically replace words. Character-name completion
  remains specific to the current script. Additional languages and personal
  dictionary sync are deferred; exact storage is an engineering detail.
- Show page boundaries and page numbers while writing. Editor pagination must match exported PDF pagination for the same document and export settings. The owner confirmed this requirement; implementation must validate a layout strategy that also meets typing responsiveness requirements.
- Provide a collapsible scene navigator beside the writing area. Click a scene
  to navigate to it; drag and drop to reorder an entire scene in one undoable
  action. A scene starts at its heading and includes every screenplay element
  before the next heading. Material before the first heading stays at the
  beginning; title-page fields and project-wide imported notes/stash do not move
  with scenes. Selection and detailed drag interaction remain to be designed.

### Suggestions and character identity

- Suggest character names from the current script while entering a character cue. New names remain allowed; the suggestions never require selecting an existing name.
- Automatic completion is specific to the element: character names/extensions
  in character cues, and heading components in scene headings. Do not show
  these completion menus in action, dialogue, or parentheticals. The owner
  mentioned transitions and other applicable elements as possibilities; no
  additional automatic-completion contexts are required yet. This does not
  exclude the agreed on-demand spelling replacements.
- Build scene-heading suggestions from individual parts: INT./EXT., locations already used in the current script, and time-of-day values such as DAY and NIGHT. The owner chose suggestions in pieces only; do not offer complete previous headings as suggestion shortcuts.
- When the owner types `(` after a character name in a cue, offer character extensions such as `V.O.` and `O.S.`.
- A character extension qualifies the cue without creating another character: `JOHN`, `JOHN (V.O.)`, and `JOHN (O.S.)` refer to the same character, JOHN. Preserve the extension on the individual cue and in exported output.
- A character extension in a cue is distinct from a dialogue parenthetical element, such as `(whispering)`, beneath that cue. This distinction must remain clear in the document model and editor.
- In an open suggestion menu, arrow keys select an option, Tab accepts it, and Escape dismisses the menu. When the menu is closed, Tab cycles element types.
- Suggestions open automatically in the applicable contexts, with the first
  match highlighted. Enter keeps its normal writing behavior without accepting
  that match; Escape dismisses without changing text. Ranking, reopening after
  dismissal, and custom extension details remain to be designed. Suggestions
  must not silently turn cue variants into duplicate characters.

### Fountain import

- Import existing Fountain scripts into Apollo's editable representation. The owner already has scripts in Fountain; import is required for the first usable writing workflow.
- Import creates a new Apollo-managed project. Apollo leaves the original
  source untouched and does not autosave back into it. Edits belong to the
  imported project; Fountain and PDF exports are separate explicit outputs.
- Keep an untouched copy of the imported Fountain inside the new project for
  reference, separate from the editable screenplay.
- Import converts into the selected structured Apollo working format; the
  detailed internal representation remains to be designed.
- Convert Fountain inline notes into named Apollo notes and commented-out/omitted passages into stash entries, preserving their text. Show an import summary explaining what moved. Leave the original Fountain file untouched. Converted notes/stash follow the agreed exclusion from screenplay exports.
- In the first usable writing milestone, provide basic read-only access to
  those converted notes and stashed passages while retaining them in the
  project. Full editing, capture, and reinsertion tools follow later.
- Preserve recognized sections, synopses, and unsupported title-page metadata
  as read-only imported information outside the screenplay/PDF, with an import
  summary. Preserve screenplay content that cannot be interpreted or rendered
  faithfully and clearly mark it for review; do not silently discard or
  substitute it, or declare a clean export while unresolved issues affect that
  output. The compatibility table must define Fountain re-export separately
  from PDF treatment. A retained raw source copy alone is not an editable
  round-trip guarantee.
- Define content/formatting preservation and import/export round-trip acceptance for screenplay content. Byte-for-byte source preservation is not an agreed requirement; the agreed note/stash conversion intentionally changes what a clean screenplay export includes.

### Projects, notes, and saved material

- A home screen provides access to multiple scripts.
- For the first usable writing build, use a simple home screen listing scripts,
  a default project location, and the option to create/open projects elsewhere.
  The owner delegated this workflow choice and accepted it as reasonable.
  The implementation-selected default is the platform Documents location plus
  `Apollo`, with a home-folder fallback. The portable source and app-local recent
  list contracts are documented in [source-format.md](source-format.md).
- Archive a project to hide it from the main home view without deleting it; allow restoring archived projects. Keep explicit local deletion separate. Archiving or deleting locally must not delete the GitHub repository. Detailed local deletion/recovery behavior remains to be specified.
- Provide named project notes with basic formatting. Search notes by their name/title. Detailed formatting controls and any body-text search remain to be specified.
- Links from notes to positions in the screenplay are tentative. The owner expressed interest but uncertainty; evaluate the idea in the later prototype before treating scene links or text-range comments as required.
- Each script has a stash of saved screenplay passages, including material removed from the current script for possible later reuse.
- Capture material explicitly: select a passage and choose Copy to Stash or Move to Stash, preserving screenplay formatting. Ordinary deletion does not automatically create a stash entry.
- A stash entry can have an optional title, annotation, and tags. Provide browsing, search, and filtering. Exact search fields and tag behavior remain to be specified.
- Reinsert a copy of a stash passage at the screenplay cursor while retaining the saved stash entry. Detailed partial-element selection and insertion behavior remain to be specified.
- Notes and stash belong to the screenplay project and are shared across its alternative drafts. Switching drafts keeps this shared material available. Preserve their history; storage, merge, and restoration semantics remain to be specified.
- The owner permits dedicated representations for notes and stash if Apollo uses its own format. This reinforces treating them as project data rather than visible Fountain markup; it does not select a schema, file layout, or storage engine.
- App notes and stash are excluded from both Fountain and PDF screenplay exports. They remain part of the project and its GitHub sync.
- A project contains one screenplay, its alternative drafts, notes, and stash, in its own project folder and Git repository. A shared repository containing all screenplay projects is outside the agreed design.
- When connected to GitHub, each screenplay project has its own repository, private by default. Detailed on-disk layout, folder location, and repository setup workflow remain to be specified.

### History and synchronization

- Saving is automatic and continuous locally. Routine editing must not depend on remembering a manual Save action. Target at most one second of lost recent writing after an application crash when storage is healthy; verify that target during implementation. Recover the latest completed save and resume the last project, draft, and cursor on restart. This is an application-crash target, not an established guarantee for hardware failure or power loss.
- Show quiet, accurate saving/saved status and a visible error when saving fails. Retain in-memory writing on failure and offer Retry or Save a Copy. Exact durability, save scheduling, retry behavior, and crash-recovery storage still require technical design and testing.
- A saving failure must be made clearly visible to the owner. If Save a Copy
  succeeds, make the copy the active project, continue saving there, and clearly
  show the changed location. Leave the original intact.
- When closing after a project saving failure, attempt an emergency copy and
  clearly show whether current writing is protected. Offer Retry, Save a Copy,
  or Keep Open. If writing remains unprotected, closing requires an explicit
  discard confirmation. A successful emergency copy does not falsely mark the
  original project destination as saved. Exact emergency storage and scheduling
  remain engineering proposals in the [milestone plan](writing-build-plan.md).
- If the latest save is damaged or unreadable, explain the problem and offer
  an available verified recovery copy with its timestamp. Preserve the damaged
  data, and do not silently reopen older writing as though it were current.
- Support Git version control through an accessible visual interface.
- Provide a Save Version action with a name/message, backed by a Git commit. Show versions in an app history with comparison and restoration; Git details may be exposed where useful. Automatic local saving continues between deliberate versions.
- To restore an earlier screenplay version, preserve current unfinished work first and restore the selected content as a new version in the active draft's history. Keep later history available. Do not rewind shared notes or stash as a side effect. Restoration of shared material remains a separate workflow to specify.
- Support named parallel alternative drafts backed by Git branches, so the owner can experiment separately from the main draft, switch between them, and merge changes through the preview and resolution workflow below. Bringing selected changes back without a full merge remains to be specified.
- Switching drafts automatically preserves unfinished work with the draft being left. Returning restores that draft's work without requiring a named version first; work must not be silently transferred to another draft or discarded. The preservation mechanism remains to be designed and verified.
- Support GitHub-based synchronization using the per-project repositories described above.
- Synchronization is explicitly triggered through a manual Sync button. Show local/remote status and allow offline writing.
- Sync covers the entire screenplay project: every draft, notes, and stash, including unfinished writing in drafts that are not currently active.
- Sync includes the latest autosaved writing even when Save Version has not been used, creating dated automatic checkpoints when needed. Distinguish automatic checkpoints from deliberately named versions. Exact commit/ref structure, history presentation, and checkpoint handling across drafts remain to be specified.
- Preview combined changes before applying a merge. For conflicting screenplay passages, show formatted local and incoming alternatives, allow choosing either or editing a combined result, and allow cancellation with originals preserved. Ordinary synchronization that requires no merge does not need a merge-preview step.
- Conflict resolution must be approachable for a writing workflow: use readable screenplay passages and clear source/draft labels, with choices that make the resulting writing understandable. Git conflict-marker editing is not the intended user interaction. Refine layout and interaction in the later visual prototype.
- Detailed merge granularity, conflicts in shared notes/stash, interrupted or partial sync, and crash-recovery implementation remain to be specified.
- Use standard Git for history and synchronization, with comparison and resolution presented in screenplay terms. The owner accepted this direction after discussing the [Git recommendation](git-design.md). Storage and merge implementation choices remain open.

### Lower-priority possibilities

Storyboards, diagrams, statistics, and a focus mode are explicitly lower priority. Their inclusion is not required for the first usable release unless the owner later changes priorities.

Production-specific tools are deferred: locked pagination, inserted page numbering (such as 12A), revision colors, and production scene-number management. This does not permit silent loss of such data in imported files; import compatibility remains to be defined.

Dedicated television templates/layouts are also deferred. Initial scope includes features/shorts, manual act labels, and page breaks.

## Export contract

The owner's requirement is an export that can be handed to a professional recipient in the expected format. The selected default is American feature submission/spec-script formatting: US Letter pages, 12-point Courier, and layout checked against the Academy's published reference. A recipient-specific requirement can still call for a different profile, but no additional profile has been requested.

Confirmed content boundary: app notes and stash do not accompany either screenplay export. Imported Fountain inline notes and omitted passages become app notes/stash as described above and are consequently excluded from clean screenplay exports; retain them in the project.

Fountain is a plain-text screenplay syntax; PDF is a rendered deliverable. The [official Fountain syntax](https://fountain.io/syntax/) defines how screenplay content and title-page metadata are represented, but it does not establish one universal PDF layout.

The Academy's [screenwriting resources](https://www.oscars.org/nicholl/screenwriting-resources) explain that professional American screenplays follow recognizable conventions with variations rather than one absolute format. Apollo's selected reference establishes the default profile; it is not a guarantee of acceptance by every studio. The [export profile working note](export-profile.md) separates confirmed choices, reference findings, and technical details still to resolve.

The [export profile](export-profile.md) defines numeric profile v1, the pinned
Courier Prime, title placement, and break rules. Its two owner confirmations
are settled; other explicitly delegated geometry details have been exercised
in the spike. Keep the [spike report](spike-report.md)'s known deviations and
coverage limits visible. The writing milestone still needs the complete
Fountain compatibility table, manual act-label mapping, deliberate-break and
whitespace handling, unsupported-character policy, production export lifecycle,
and additional fixtures for editing states absent from the spike.

PDF acceptance must check layout and pagination against independent expectations
as well as editor/PDF agreement. The existing hand-derived fixtures and Academy
comparison remain required; a PDF opening or agreement between two outputs of
the same engine does not establish formatting correctness.

## Confirmed behavior to verify

These are acceptance examples for the settled requirements, not an exhaustive test plan:

- At a fixed document revision and selected export settings, editor page boundaries and numbers agree with the PDF, including material near a page break.
- Creating the next element after a character cue selects dialogue; after a dialogue parenthetical it selects dialogue; the normal fallback is action.
- Import an existing Fountain script and edit its screenplay elements through the formatted editor without exposing Fountain markup.
- Enter `JOHN`, then `JOHN (V.O.)` and `JOHN (O.S.)`: suggestions treat the base name as one character, while each qualified cue keeps its extension in Fountain and PDF exports.
- Typing `(` after a character name offers `V.O.` and `O.S.` extensions; a dialogue parenthetical remains a separate element.
- With a suggestion menu open, arrow keys change selection, Tab accepts, and Escape dismisses. With it closed, Tab changes the element type. A new character name can be entered without accepting a suggestion.
- Scene-heading suggestions offer individual components, without whole-heading completion shortcuts.
- During continuous typing with healthy storage, interrupt the application and reopen it: recover the latest completed save, meet the one-second loss target, and resume the last project/draft/cursor. Test the storage and recovery mechanism before claiming this target is met.
- Save a named version, continue editing, and compare the named version against the current screenplay through the app history.
- Edit an alternative draft without saving a named version, switch to the main draft, and return: unfinished changes remain in the alternative draft and do not appear in the main draft.
- Local saving and named version creation work without network access; synchronization is requested through the Sync button.
- After a named version, write more and click Sync without saving another named version. Following successful sync, the newer writing is recoverable from the remote through an automatic checkpoint; the named milestone remains distinguishable.
- Two screenplay projects have independent repositories and draft histories; creating or switching a draft in one does not change the other.
- Add a note and stash entry, then switch drafts: both remain available as shared project material.
- Find a named note by its title. A link from that note into the screenplay is not yet a required acceptance condition.
- Copy a formatted passage to the stash: it remains in the screenplay. Move a passage to the stash: it is removed from the screenplay and retained in the stash. Ordinary deletion creates no stash entry.
- Add a stash annotation and tags, locate the entry through browsing/search/filtering, and reinsert it: screenplay formatting is preserved and the saved entry remains available.
- Export a project containing distinctive text in app notes and stash to Fountain and PDF: neither export contains that text unless the owner has explicitly inserted it into the screenplay itself.
- Modify two different drafts and shared notes/stash, then Sync while viewing only one draft. Following successful sync, all project material, including unfinished writing in the other draft, is recoverable from the remote.
- For conflicting screenplay edits, preview both formatted passages, edit or select a resolution, and apply the reviewed result. Cancelling the merge leaves the original drafts intact. A merge is not applied without its preview being accepted.
- Edit the title-page fields and export with and without the title page. Both Fountain and PDF honor the choice; including a title page does not change the screenplay's logical body-page numbering.
- Flag a misspelling without changing the word; apply a suggested replacement on demand. Add an invented word to the personal dictionary and stop flagging it.
- Apply a suggested formatting correction and undo it without losing writing. Ordinary dialogue/action text is not silently rewritten.
- Reorder a scene through the navigator: its heading and complete contents move together, neighboring scene contents remain intact, and Undo restores the original order.
- Restore an older screenplay version after additional unfinished edits: preserve those edits, create a new restored version, retain later history, and leave shared notes/stash unchanged.
- Archive and restore a project without losing its content/history. Neither archiving nor explicit local deletion removes its remote GitHub repository.
- Simulate a failed local save: status reports failure, in-memory writing remains accessible, and Retry or Save a Copy can preserve it without a false saved indication.
- Edit and import/export dual dialogue, emphasis, lyrics, centered text, and manual page breaks with supported formatting preserved; no Fountain markup appears in the writing view.
- Import a Fountain file with inline notes and omitted passages: retain their text in named notes and stash, show the conversion summary, leave the original file unchanged, and exclude converted material from subsequent clean screenplay exports.

## Technical direction and remaining decisions

The owner delegated stack research and recommendations to the agent. Their clarified preference is native/low-level efficiency if development, maintenance, and visual customization remain manageable; Electron is acceptable if necessary. Concerns about C++ are time, code volume, maintainability, and achieving the desired appearance, including when agents write the code. They explicitly asked to consider Rust, Flutter, and Tauri.

Owner decisions: provisionally adopt Qt 6 + C++20 + QML on 2026-10-27, then keep that stack on 2026-10-28 after the editor/export feasibility spike and acceptance of the QML look. The Apollo-owned screenplay layout engine is shared by the editor and PDF export. The [spike report](spike-report.md) records the measured evidence and remaining limits; it does not establish production readiness. Electron + TypeScript + ProseMirror remains the fallback if later evidence warrants reassessment. The [independent stack recommendation](stack-recommendation.md) and earlier [stack evaluation](stack.md) remain as background research.

The source schema, shared-material Git storage, exact dependency/tool version pins, and release packaging remain open.
