# US English spelling data and engine

`en_US.aff`, `en_US.dic` and the full upstream `README_en_US.txt` are unmodified
LibreOffice dictionary files from commit
[`4fa94195b8136364dd40bf2b0366a0fe32058899`](https://github.com/LibreOffice/dictionaries/tree/4fa94195b8136364dd40bf2b0366a0fe32058899/en).
The dictionary identifies itself as SCOWL 2020.12.07. Its included README
contains the copyright, permission, source and attribution notices.
`cmake/Spelling.cmake` verifies the two data hashes.

The engine is unmodified [Hunspell 1.7.2](https://github.com/hunspell/hunspell/tree/v1.7.2)
(commit `2969be996acad84b91ab3875b1816636fe61a40e`), fetched at first editor
configuration using a SHA-256-pinned archive. It is compiled statically from its
library source list with `HUNSPELL_STATIC`; no system install is performed.
The generated visibility header follows upstream's template. Engine-only builds
need neither the engine nor dictionary. CMake's standard FetchContent source
cache override can provide an offline checkout (`FETCHCONTENT_SOURCE_DIR_HUNSPELL`).

Hunspell is offered under MPL 1.1 / GPL 2.0-or-later / LGPL 2.1-or-later;
upstream copyright notices remain in the downloaded source. Copies of the MPL
and LGPL texts accompany this directory for the linked engine. Packaging a
binary must retain these notices and meet the selected license's source and
relinking obligations. The private [local runtime](../../docs/local-launch.md)
includes the unmodified engine source and upstream notices; public distribution
and a project license remain undecided.
