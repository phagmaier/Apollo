# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Exercise the installed, relocated runtime with isolated application state."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", required=True, type=Path)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--spelling-tests", required=True, type=Path)
    parser.add_argument("--package", type=Path, help="verify and relocate this packaged runtime instead of installing")
    parser.add_argument("--platform", default="offscreen", choices=("offscreen", "wayland"))
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output.resolve()
    with tempfile.TemporaryDirectory(prefix="apollo-runtime-") as temporary:
        root = Path(temporary)
        env = os.environ | {"QT_QPA_PLATFORM": args.platform,
                            "XDG_DATA_HOME": str(root / "data"),
                            "XDG_CONFIG_HOME": str(root / "config"),
                            "XDG_CACHE_HOME": str(root / "cache")}

        def run(label, command, success=True, cwd=root):
            result = subprocess.run(list(map(str, command)), env=env, cwd=cwd,
                                    capture_output=True, text=True, timeout=30)
            (output / f"{label}.log").write_text(result.stdout + result.stderr)
            assert (result.returncode == 0) == success, (label, result.returncode, result.stdout, result.stderr)
            return result

        installed = root / "installed"
        if args.package:
            package = args.package.resolve()
            run("manifest", ["sha256sum", "--check", "SHA256SUMS"], cwd=package)
            listed = {line.split(maxsplit=1)[1] for line in (package / "SHA256SUMS").read_text().splitlines()}
            actual = {str(p.relative_to(package)) for p in package.rglob("*")
                      if p.is_file() and p != package / "SHA256SUMS"}
            assert listed == actual, ("manifest omits or invents package files", actual - listed, listed - actual)
            shutil.copytree(package, installed, symlinks=True)
        else:
            run("install", [args.cmake, "--install", args.build.resolve(), "--prefix", installed, "--component", "Runtime"])
        assert (installed / "bin/apollo").is_file(), "installation has no Apollo launcher"
        relocated = root / "moved package with spaces"
        installed.rename(relocated)
        launcher = relocated / "bin/apollo"
        assert launcher.is_file(), "installation has no Apollo launcher"
        linked = root / "linked launcher"
        linked.symlink_to(launcher)
        runtime = relocated / "lib/apollo"
        for relative in ("apollo_editor", "pdf_export", "fountain_export", "assets/app/WritingDesk.qml",
                         "assets/app/ProjectAccess.qml", "assets/app/ImportedMaterial.qml",
                         "assets/app/TitlePageForm.qml", "assets/app/ShortcutReference.qml",
                         "assets/editor/ScreenplayEditor.qml",
                         "assets/courier-prime/OFL.txt", "assets/spelling/README_en_US.txt",
                         "assets/spelling/HUNSPELL-COPYING.MPL", "assets/spelling/HUNSPELL-COPYING.LESSER"):
            assert (runtime / relative).is_file(), f"missing runtime file: {relative}"
        upstream = relocated / "share/apollo/third-party/hunspell"
        assert (upstream / "COPYING.MPL").is_file() and (upstream / "src/hunspell/hunspell.cxx").is_file()
        sample = root / "sample.fountain"
        sample.write_text("INT. ROOM - DAY\n\nA lamp glows.\n\nTHEO\nHello.\n")
        project = root / "Writer with spaces"
        # Relative arguments exercise preservation of the caller's directory.
        run("create", [linked, project.name, "--create", sample.name])
        run("check", [linked, project, "--check"])
        fountain = root / "writing.fountain"
        run("fountain", [linked, project, "--export-fountain", fountain])
        assert fountain.read_text() == "INT. ROOM - DAY\n\nA lamp glows.\n\nTHEO\nHello.\n"
        snapshots = root / "snapshots"
        run("snapshot", [linked, project, "--agreement", snapshots])
        pdf = next(snapshots.glob("*.pdf"))
        extracted = run("pdf-text", ["pdftotext", "-layout", pdf, "-"]).stdout
        assert "INT. ROOM - DAY" in extracted and "A lamp glows." in extracted and "Hello." in extracted
        fonts = run("pdf-fonts", ["pdffonts", pdf]).stdout
        font_rows = [line.split() for line in fonts.splitlines()[2:]]
        assert font_rows and all(row[0].endswith("CourierPrime-Regular")
                                 and row[-5] == "yes" and row[-3] == "yes" for row in font_rows), fonts
        run("typing", [linked, project, "--autosave-typing", output / "typing.tsv", 12, 10])
        source = project / "screenplay.json"
        authored = json.loads(source.read_text())
        assert authored["elements"][-1]["text"] == "Hello.the lamp tur"
        source_before = source.read_bytes()

        # Reuse the existing real spelling test with only the relocated data.
        # This verification binary is never part of the installed runtime.
        spelling_test = runtime / "spelling_tests"
        shutil.copy2(args.spelling_tests.resolve(), spelling_test)
        run("spelling", [spelling_test, "checking_replacement_and_personal_words"])
        dictionary = runtime / "assets/spelling/en_US.dic"
        dictionary.rename(dictionary.with_suffix(".hidden"))
        missing = run("missing-dictionary", [spelling_test, "checking_replacement_and_personal_words"], success=False)
        assert "US English dictionary unavailable" in missing.stdout
        dictionary.with_suffix(".hidden").rename(dictionary)
        spelling_test.unlink()

        # A real normal launch must activate the project through installed QML,
        # then recover its lock and resume after an unclean process exit.
        for label, options in (("open", [project]), ("resume", [])):
            state = root / "data/Apollo/session.json"
            if state.exists():
                previous = json.loads(state.read_text())
                # Keep the identity and element offset, but make activation
                # repair a deliberately wrong fallback coordinate. Lock
                # acquisition during inspection alone cannot satisfy this.
                previous["position"] = 1234
                state.write_text(json.dumps(previous))
            with (output / f"{label}.log").open("w") as log:
                process = subprocess.Popen([str(linked), *map(str, options)], env=env, cwd=root,
                                           stdout=log, stderr=subprocess.STDOUT)
                try:
                    deadline = time.monotonic() + 15
                    while time.monotonic() < deadline:
                        assert process.poll() is None, f"{label} exited: {process.returncode}"
                        try:
                            current = json.loads(state.read_text())
                            owner = (project / ".apollo.lock").read_text().splitlines()[0]
                            if (current["directory"] == str(project) and current["position"] == 0
                                    and owner == str(process.pid)):
                                break
                        except (FileNotFoundError, json.JSONDecodeError, IndexError):
                            pass
                        time.sleep(0.02)
                    else:
                        raise AssertionError(f"{label} did not activate installed writer")
                finally:
                    if process.poll() is None:
                        process.kill()
                    process.wait(timeout=10)
        assert source.read_bytes() == source_before, "normal reopen changed saved writing"

        access = runtime / "assets/app/ProjectAccess.qml"
        access.rename(access.with_suffix(".hidden"))
        missing = run("missing-qml", [linked], success=False)
        assert "cannot load project access QML" in missing.stderr
        access.with_suffix(".hidden").rename(access)

        # Missing installed assets must not silently fall back to the checkout.
        font = runtime / "assets/courier-prime/CourierPrime-Regular.ttf"
        font.rename(font.with_suffix(".hidden"))
        missing = run("missing-font", [linked, project, "--check"], success=False)
        assert "Courier Prime font" in missing.stderr

        # Package publication must not overwrite any existing destination kind.
        packager = Path(__file__).resolve().parents[1] / "tools/package-local.sh"
        directory = root / "existing directory"
        directory.mkdir()
        (directory / "keep").write_bytes(b"retained package")
        regular = root / "existing file"
        regular.write_bytes(b"retained file")
        dangling = root / "dangling link"
        dangling.symlink_to(root / "absent target")
        for index, destination in enumerate((directory, regular, dangling)):
            refused = run(f"collision-{index}", [packager, destination], success=False)
            assert "Destination already exists" in refused.stderr
        assert (directory / "keep").read_bytes() == b"retained package"
        assert regular.read_bytes() == b"retained file" and dangling.is_symlink()
        assert not (root / "absent target").exists()
        print("Relocated runtime: launcher/symlink, QML writer, typing/save/crash/reopen, both helpers, spelling and missing-asset refusals pass.")


if __name__ == "__main__":
    main()
