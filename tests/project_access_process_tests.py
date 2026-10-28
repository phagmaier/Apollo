# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Exercise actual project-open and no-argument resume dispatch in isolated processes."""

import argparse
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
    with tempfile.TemporaryDirectory(prefix="apollo-access-") as temporary:
        root = pathlib.Path(temporary)
        env = os.environ | {"XDG_DATA_HOME": str(root / "data"), "QT_QPA_PLATFORM": "offscreen"}
        state_file = root / "data/Apollo/session.json"
        sample = root / "sample.fountain"
        sample.write_text("One saved paragraph.\n")

        def command(*options):
            result = subprocess.run([str(editor), *map(str, options)], env=env, capture_output=True, text=True, timeout=15)
            assert result.returncode == 0, result.stderr
            return result

        first, second = root / "first", root / "second"
        for project in (first, second):
            command(project, "--create", sample)
        source = json.loads((first / "screenplay.json").read_text())
        first_id = json.loads((first / "project.json").read_text())["id"]
        second_id = json.loads((second / "project.json").read_text())["id"]
        state_file.parent.mkdir(parents=True)
        # Independent UTF-16 example: element offset 3 must restore position 3,
        # even though the fallback coordinate deliberately names position 0.
        state_file.write_text(json.dumps({"format": "apollo-session", "version": 1, "directory": str(first),
                                         "projectId": first_id, "elementId": source["elements"][0]["id"],
                                         "offset": 3, "position": 0}))
        source_before = (first / "screenplay.json").read_bytes()

        def await_state(process, project, identity, position):
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                assert process.poll() is None, f"editor exited {process.returncode}"
                state = json.loads(state_file.read_text())
                lock = project / ".apollo.lock"
                try:
                    lock_owner = lock.read_text().splitlines()[0]
                except (FileNotFoundError, IndexError):
                    lock_owner = ""
                if (lock_owner == str(process.pid)
                        and state["directory"] == str(project) and state["projectId"] == identity
                        and state["position"] == position):
                    return
                time.sleep(0.01)
            raise AssertionError("editor did not activate and persist the expected project/cursor")

        def launch(label, project, identity, position, *options):
            with (args.output / f"{label}.log").open("w") as log:
                process = subprocess.Popen([str(editor), *map(str, options)], env=env, stdout=log, stderr=subprocess.STDOUT)
                try:
                    await_state(process, project, identity, position)
                finally:
                    process.kill()
                    process.wait(timeout=10)
            assert (project / ".apollo.lock").exists(), "SIGKILL did not retain the lock"

        launch("resume", first, first_id, 3)
        assert (first / "screenplay.json").read_bytes() == source_before
        state_before = state_file.read_bytes()
        command(first, "--check")
        assert not (first / ".apollo.lock").exists(), "instrumentation did not release the stale lock"
        assert state_file.read_bytes() == state_before, "instrumentation changed the remembered project/cursor"
        assert (first / "screenplay.json").read_bytes() == source_before
        # Make only this process's fresh cursor capture satisfy the oracle;
        # acquiring the lock during inspection is not document activation.
        restart_state = json.loads(state_file.read_text())
        restart_state["position"] = 0
        state_file.write_text(json.dumps(restart_state))
        launch("restart", first, first_id, 3)
        launch("explicit-open", second, second_id, 0, second)
        state_before = state_file.read_bytes()
        missing = root / "missing"
        with (args.output / "missing.log").open("w") as log:
            process = subprocess.Popen([str(editor), str(missing)], env=env, stdout=log, stderr=subprocess.STDOUT)
            try:
                # A failed ordinary open stays in the access UI, rather than
                # exiting as a failed Fountain import or recording an empty writer.
                time.sleep(0.5)
                assert process.poll() is None, "failed open did not retain the access UI"
                assert state_file.read_bytes() == state_before
                assert not missing.exists()
            finally:
                if process.poll() is None:
                    process.kill()
                process.wait(timeout=10)
        print("actual CLI: no-argument resume restores element offset 3; SIGKILL restart recovers edit access")
        print("explicit open activates the selected identity; --check and failed opens preserve resume/evidence")


if __name__ == "__main__":
    main()
