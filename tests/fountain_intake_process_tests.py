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


if __name__ == "__main__":
    main()
