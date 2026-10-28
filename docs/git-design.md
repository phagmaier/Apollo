# Git for screenplay versioning

Investigated 2026-10-27. The owner accepted the recommendation to use standard Git with screenplay-aware presentation. The ongoing [specification](spec.md) holds the agreed product behavior; storage schema and merge implementation remain open.

## Question and recommendation

The owner agreed to named versions, parallel alternative drafts, and manual GitHub sync, and asked whether standard Git is appropriate for screenplays.

Accepted recommendation: use standard Git for committed history, draft branches, and remote exchange. Build Apollo's comparison and resolution interface around screenplay content. A custom version-control engine would add history, branching, and synchronization work to a personal application without an established need. This is an engineering judgment for Apollo's scope, not proof that Git is optimal for every writing application.

## Evidence and implications

- Git commits record snapshots that can be compared and revisited. Local working changes remain distinct from committed snapshots. Apollo distinguishes automatic saving from deliberate named versions and creates automatic checkpoints when Sync needs to include newer writing. The checkpoint representation still needs design. [Git: recording changes](https://git-scm.com/book/en/v2/Git-Basics-Recording-Changes-to-the-Repository).
- Git provides branching and merging. Switching with uncommitted changes can be blocked or otherwise require work preservation. Apollo must define draft-local unfinished work explicitly rather than relying on a bare branch-switch command. [Git: branching and merging](https://git-scm.com/book/en/v2/Git-Branching-Basic-Branching-and-Merging).
- Git's standard file merge operates on text and reports textual conflicts; custom merge drivers are supported. This provides integration options, but does not establish screenplay-aware merging or guarantee that a textually clean result preserves screenplay structure or narrative intent. [Git: merge attributes and drivers](https://git-scm.com/docs/gitattributes#_performing_a_three_way_merge).

Design implications, pending validation:

- The owner has now selected a portable project folder with versioned structured
  text as authoritative source, and Fountain as import/export (planning round
  19). The [writing build plan](writing-build-plan.md) proposes the initial
  layout; the exact schema still needs design and validation. Keep serialization
  stable so small edits do not rewrite unrelated content.
- Evaluate comparison by screenplay elements, with word-level changes where useful. Scene moves, concurrent dialogue edits, element-type changes, and cue extensions are representative cases for the design spike.
- Validate merged content against the document model and show unresolved choices through Apollo. The owner requires a merge preview and approachable conflict resolution using formatted writing. A successful Git merge alone does not demonstrate a correct screenplay merge.
- Keep authoritative editable source in history; generated PDFs are candidates to regenerate on demand. The actual tracked-file policy is undecided.
- The owner selected draft-local preservation of unfinished work and dated automatic checkpoints when Sync needs to include newer writing. Sync covers every draft and shared project notes/stash, including unfinished writing in inactive drafts. Design and verify the persistence mechanism, checkpoint representation, and shared-material handling across branches. Git commits do not by themselves provide crash recovery for later uncommitted work.

## Remaining verification

Before choosing the storage and merge strategy, exercise representative screenplay edits across two drafts: concurrent edits to one dialogue block, scene movement versus scene edits, deletion versus editing, character extensions, and notes/stash changes. Verify both content preservation and comprehensible resolution. No implementation or local Git integration was tested in this research.
