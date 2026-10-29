# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Actual executable: retain, reopen, restart, refuse editing/export, keep source bytes."""
import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import tempfile
import time
import signal
import copy


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--editor", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    editor = args.editor.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="apollo-intake-") as temporary:
        root = pathlib.Path(temporary)
        env = os.environ | {"XDG_DATA_HOME": str(root / "data"), "QT_QPA_PLATFORM": "offscreen"}
        source = root / "original.fountain"
        original = b"\xef\xbb\xbfTitle: Retained\r\n\r\nA line. [[note]]\r\n/*omitted*/\r\n"
        source.write_bytes(original)
        project = root / "review"
        result = subprocess.run([str(editor), str(project), "--retain-fountain", str(source)], env=env,
                                capture_output=True, text=True, timeout=15)
        assert result.returncode == 0, result.stderr
        manifest = json.loads((project / "project.json").read_bytes())
        assert manifest["version"] == 2 and manifest["mode"] == "import-review"
        metadata = json.loads((project / "imported-material.json").read_bytes())
        assert metadata["sha256"] == hashlib.sha256(original).hexdigest()
        assert metadata["bytes"] == len(original)
        identity = manifest["id"]
        evidence = {path: path.read_bytes() for path in project.rglob("*") if path.is_file()}
        state_file = root / "data/Apollo/session.json"
        state_file.parent.mkdir(parents=True, exist_ok=True)
        seed = {"format": "apollo-session", "version": 1, "directory": str(project), "projectId": identity,
                "elementId": "", "offset": 0, "position": 123}

        def launch(label, *options):
            # Require a fresh successful reader activation, not only lock acquisition.
            state_file.write_text(json.dumps(seed))
            with (args.output / f"{label}.log").open("w") as log:
                process = subprocess.Popen([str(editor), *map(str, options)], env=env, stdout=log, stderr=subprocess.STDOUT)
                try:
                    deadline = time.monotonic() + 15
                    while time.monotonic() < deadline:
                        assert process.poll() is None, f"editor exited {process.returncode}"
                        state = json.loads(state_file.read_bytes())
                        if state["projectId"] == identity and state["position"] == 0:
                            assert (project / ".apollo.lock").read_text().splitlines()[0] == str(process.pid)
                            break
                        time.sleep(0.02)
                    else:
                        raise AssertionError("review project did not activate")
                finally:
                    process.kill()
                    process.wait(timeout=10)
            assert {path: path.read_bytes() for path in evidence} == evidence

        launch("explicit", project)
        launch("restart-resume")  # Must recover the actual previous process's stale lock.
        for options in (("--check",), ("--agreement", str(root / "pdf-output")), ("--autosave-typing", str(root / "typing-log"))):
            result = subprocess.run([str(editor), str(project), *options], env=env, capture_output=True, text=True, timeout=15)
            assert result.returncode != 0 and "review-only" in result.stderr, result
        assert not (root / "pdf-output/editor.pdf").exists()
        result = subprocess.run([str(editor), str(project), "--retain-fountain", str(source)], env=env,
                                capture_output=True, text=True, timeout=15)
        assert result.returncode != 0 and "already exists" in result.stderr
        assert {path: path.read_bytes() for path in evidence} == evidence
        assert source.read_bytes() == original
        assert (project / "imports/original.fountain").read_bytes() == original
        assert not (project / ".apollo.lock").exists()
        print("actual CLI intake → explicit reopen → SIGKILL → resume retains byte-exact original and metadata")
        print("editing/layout/export instrumentation and collisions are refused without changing evidence")

        old_evidence = evidence
        old_project = project
        project = root / "interpreted"
        result = subprocess.run([str(editor), str(project), "--interpret-fountain", str(source)], env=env,
                                capture_output=True, text=True, timeout=15)
        assert result.returncode == 0, result.stderr
        metadata = json.loads((project / "imported-material.json").read_bytes())
        assert metadata["version"] == 2 and metadata["status"] == "interpreted-review"
        assert metadata["sha256"] == hashlib.sha256(original).hexdigest()
        model = metadata["conversion"]
        assert not model["editable"] and not model["fountainExport"] and not model["pdfExport"]
        assert model["screenplay"]["elements"][0]["text"] == "A line. "
        assert [(r["kind"], r["text"]) for r in model["retained"]] == [("note", "note"), ("stash", "omitted")]
        identity = json.loads((project / "project.json").read_bytes())["id"]
        assert identity != json.loads((old_project / "project.json").read_bytes())["id"]
        seed = seed | {"directory": str(project), "projectId": identity}
        evidence = {path: path.read_bytes() for path in project.rglob("*") if path.is_file()}
        launch("interpreted-explicit", project)
        launch("interpreted-resume")
        for options in (("--check",), ("--snapshot", str(root / "blocked"))):
            result = subprocess.run([str(editor), str(project), *options], env=env, capture_output=True, text=True, timeout=15)
            assert result.returncode != 0 and "review-only" in result.stderr
        assert not (root / "blocked/editor.pdf").exists()
        assert {path: path.read_bytes() for path in old_evidence} == old_evidence
        assert {path: path.read_bytes() for path in evidence} == evidence
        assert (project / "imports/original.fountain").read_bytes() == original
        print("versioned interpretation → reopen → SIGKILL → resume preserves both projects; export stays blocked")

        fixture = pathlib.Path(__file__).resolve().parents[1] / "fixtures/import/writer-subset.fountain"
        expected = json.loads(fixture.with_suffix(".expected.json").read_bytes())
        source.write_bytes(fixture.read_bytes())
        for phase in ("writing", "written", "committed", "acknowledged"):
            project = root / f"working-{phase}"
            result = subprocess.run([str(editor), str(project), "--edit-fountain", str(source)], env=env,
                                    capture_output=True, text=True, timeout=15)
            assert result.returncode == 0, result.stderr
            manifest = json.loads((project / "project.json").read_bytes())
            assert manifest["version"] == 3 and manifest["mode"] == "fountain-working"
            dependencies = {name: (project / name).read_bytes() for name in
                            ("imported-material.json", "imports/original.fountain")}
            assert json.loads((project / "screenplay.json").read_bytes()) == expected
            log_path = args.output / f"working-kill-{phase}.tsv"
            result = subprocess.run([str(editor), str(project), "--autosave-typing", str(log_path),
                                     "100", "10", f"{phase}:1"], env=env, capture_output=True, text=True, timeout=20)
            assert result.returncode == -signal.SIGKILL, (phase, result.returncode, result.stderr)
            rows = [line.split("\t") for line in log_path.read_text().splitlines()]
            target = next(row for row in rows if row[0] == "target")
            assert target[1:3] == ["f21", "17"], target
            assert any(row[0] == "kill" and row[1] == phase for row in rows)

            def check_writing(path):
                actual = json.loads(path.read_bytes())
                wanted = copy.deepcopy(expected)
                actual_text = actual["elements"][-1]["text"]
                count = len(actual_text) - len("The train leaves.")
                assert 0 <= count <= 100
                wanted["elements"][-1]["text"] = "The train leaves." + ("the lamp turns and " * 6)[:count]
                wanted["revision"] = actual["revision"]
                assert actual == wanted, phase
                return actual

            saved = check_writing(project / "screenplay.json")
            for package in (project / ".recovery").glob("copy-*"):
                check_writing(package / "screenplay.json")
                for name, data in dependencies.items():
                    assert (package / name).read_bytes() == data
            # Actual normal app reopen and automatic resume after process death.
            identity = manifest["id"]
            seed = {"format": "apollo-session", "version": 1, "directory": str(project), "projectId": identity,
                    "elementId": "f8", "offset": 5, "position": 0}
            state_file.write_text(json.dumps(seed))
            expected_position = sum(len(e["text"]) + 1 for e in expected["titlePage"] + expected["elements"][:1]) + 5
            for label, options in (("explicit", [str(project)]), ("resume", [])):
                before = state_file.stat().st_mtime_ns
                with (args.output / f"working-{phase}-{label}.log").open("w") as log:
                    process = subprocess.Popen([str(editor), *options], env=env, stdout=log, stderr=subprocess.STDOUT)
                    try:
                        deadline = time.monotonic() + 15
                        while time.monotonic() < deadline:
                            assert process.poll() is None, (phase, label, process.returncode)
                            state = json.loads(state_file.read_bytes())
                            if state_file.stat().st_mtime_ns > before and state["projectId"] == identity and state["position"] == expected_position:
                                assert state["elementId"] == "f8" and state["offset"] == 5
                                break
                            time.sleep(.02)
                        else:
                            raise AssertionError(f"working writer did not activate: {phase} {label}")
                    finally:
                        process.kill(); process.wait(timeout=10)
                assert json.loads((project / "screenplay.json").read_bytes()) == saved
            result = subprocess.run([str(editor), str(project), "--snapshot", str(root / "working-blocked")],
                                    env=env, capture_output=True, text=True, timeout=15)
            assert result.returncode != 0 and "remain blocked" in result.stderr, result.stderr
            assert not (root / "working-blocked/editor.pdf").exists()
            for name, data in dependencies.items():
                assert (project / name).read_bytes() == data
            assert source.read_bytes() == fixture.read_bytes()
        print("editable subset: independent content and associations survive typing/SIGKILL at four save phases, actual reopen/resume; PDF remains gated")


if __name__ == "__main__":
    main()
