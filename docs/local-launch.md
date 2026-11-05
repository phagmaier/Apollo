# Apollo local launch and owner handoff

Task 6a packages a private local runtime for the owner's Arch/Omarchy machine.
This is a writing build with the existing import/export and recovery limits,
not milestone completion or a public distribution. Task 6b requires the owner's
writing session and observations; automated checks cannot supply them.

From the checkout, run `tools/package-local.sh build/local-apollo` with a new
destination. It configures and builds the release editor, validates pinned
assets, installs into a temporary sibling and publishes without replacing an
existing destination. Failed staging is removed. Existing packages are kept:
choose a different destination for a newer build. No root access, system font
installation or desktop registration is needed.

Run `build/local-apollo/bin/apollo` to resume, or append an Apollo project
directory to open it. The entire package may move together, including to a
directory containing spaces; the launcher also accepts a user-created symlink.
Arguments and the caller's working directory are preserved. On a desktop with
`WAYLAND_DISPLAY`, the launcher defaults Qt to Wayland; explicit
`QT_QPA_PLATFORM` remains available for checks. Input-method selection is
inherited. `QT_IM_MODULE=compose` is the tested fallback here; fcitx requires
the installed Qt input plugin and running daemon. No install-time environment
setting changes the owner's session.

The runtime contains:

| Location | Contents |
| --- | --- |
| `bin/apollo` | Launcher |
| `lib/apollo/` | Editor, `pdf_export`, `fountain_export` |
| `lib/apollo/assets/` | All application/editor QML, four Courier Prime faces and OFL, US English dictionary and notices |
| `share/apollo/third-party/hunspell/` | Unmodified pinned Hunspell engine source and upstream notices |
| `BUILD-INFO.txt` | Source revision/dirty status, build tools, system/Qt package versions and resolved shared libraries |
| `SHA256SUMS` | Package file hashes; run `sha256sum --check SHA256SUMS` from the package directory |

The editor and helpers resolve assets beside their executable. They never use
the source checkout or current working directory for runtime assets. Missing
QML/fonts fail explicitly; missing dictionary data visibly disables spelling.
Both helpers remain private siblings, so exports need neither `PATH` lookup
nor a checkout. Application state and projects retain the existing
[source-format](source-format.md) locations and behavior. Removing a runtime
does not delete projects or personal dictionary data. Recovery, imports and
export gates are unchanged.

This package uses **system Qt and system libraries**. The recorded baseline is
Qt 6.12.0 (`qt6-base` 6.12.0-3, `qt6-declarative` 6.12.0-4,
`qt6-wayland`/`qt6-svg` 6.12.0-1), `gcc-libs`
16.2.1+r23+gd564253eb6c8-1 and `glibc` 2.44+r50+g1848099f063e-1.
`BUILD-INFO.txt` records actual versions for each package, including resolved
transitive libraries. QML Controls Basic, Dialogs, the Wayland platform plugin
and graphics/input dependencies come from the system installation. A system
upgrade may require rebuilding and rerunning verification. CMake 4.4.4 and
Ninja 1.13.2 remain pinned in `mise.toml`; GCC 16.2.1 is the system compiler.
The dictionary and statically compiled Hunspell 1.7.2 remain as described in
[spelling data](../assets/spelling/README.md). Runtime launch has no network,
Python, uv, mise, Poppler or Node requirement. Building/checking additionally
uses mise, uv, pkg-config, the system toolchain, pacman (version inventory),
ripgrep, coreutils and `ldd`; independent PDF checks use Poppler and Fountain
oracle checks use the separately installed pinned afterwriting parser.
This task makes no project license or public distribution decision.

Run **`tools/check-owner-handoff.sh`** from the desktop for the full handoff.
It saves every stage under a new `build/owner-handoff-*` directory and stops on
failure. It runs required release/sanitizer/engine checks, creates a runtime,
tests native relocation and existing writing/recovery/export controls against
the packaged assets, then opens a disposable synthetic project. Application
state for the manual session stays inside that run directory. Close normally,
then use the runner's resume checkpoint and record observations. The package
and disposable writing remain available afterward. No private screenplay is
copied or changed. Existing UI checks exercise the system clipboard; avoid
concurrent writing while they run.

`tools/check-owner-handoff.sh --smoke` runs the automated stages, prepares the
multipage disposable writer and checks its PDF/Fountain commands, then skips
the manual session. Automated input is Qt Test on native Wayland; the actual
runtime uses OpenGL and the existing project-flow fixture uses software
rendering. This does not measure compositor/physical input or assert comfort. The copied
verification binaries used by the runner are removed before handoff. Their
fixture/font expectations still come from the checkout; the relocated actual
editor test checks every packaged file against the manifest, then separately
exercises its bundled fonts, assets and helpers from a
temporary directory outside the checkout, with exact saved text and independent
PDF/Fountain expectations. Missing installed QML/font/dictionary controls
demonstrate that source-tree fallback cannot mask packaging omissions.

The [responsiveness runner](writing-responsiveness.md) remains the separate
sustained measurement, with its recorded stress/save-age/memory limits. The
broader [Wayland runner](../tools/check-wayland.sh) additionally requires
`wtype`, Hyprland and fcitx; those checks are not implied by this handoff smoke.
Fresh Academy comparison, general Fountain compatibility, imported PDF,
heavy-I/O crash-loss limits and the slice 2 follow-ups remain open. The owner
should record observed comfort and defects without treating the automated
pass as production PDF or full milestone acceptance.
