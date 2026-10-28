# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Exercise editable project locks through independent editor processes."""

import argparse
import os
import pathlib
import shutil
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--editor", required=True, type=pathlib.Path)
    parser.add_argument("--source", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    editor = args.editor.resolve()
    shutil.rmtree(args.output, ignore_errors=True)
    args.output.mkdir(parents=True)
    project = args.output.resolve() / "project"

    def run(path, *options):
        return subprocess.run([str(editor), str(path), *map(str, options)], capture_output=True, text=True, timeout=10)

    result = run(project, "--create", args.source.resolve())
    assert result.returncode == 0, result.stderr
    alias = project.with_name("alias")
    alias.symlink_to(project, target_is_directory=True)
    log = project.parent / "typing.tsv"
    owner = subprocess.Popen([str(editor), str(project), "--autosave-typing", str(log), "10000", "10"],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        deadline = time.monotonic() + 10
        while not log.exists() or "\nkey\t" not in log.read_text():
            assert owner.poll() is None, owner.communicate()[0]
            assert time.monotonic() < deadline, "owner never started typing"
            time.sleep(0.01)
        lock = project / ".apollo.lock"
        assert lock.exists(), "editable owner has no project lock"
        # Age alone must never expire an active document lock.
        old = time.time() - 86400
        os.utime(lock, (old, old))
        lock_bytes = lock.read_bytes()
        for path in (project, alias, project / ".." / "project"):
            before = time.monotonic()
            result = run(path, "--check")
            assert result.returncode != 0 and "already open or locked" in result.stderr, result.stderr
            assert time.monotonic() - before < 3, "competing open blocked"
            assert lock.read_bytes() == lock_bytes, "competing open changed the active lock"
        print("active aged lock rejects independent opens through canonical, symlink, and relative aliases")
    finally:
        owner.kill()
        output, _ = owner.communicate(timeout=10)
        (args.output / "owner-output.txt").write_text(output)

    # SIGKILL leaves the lock, and the next process must recover it by PID.
    assert (project / ".apollo.lock").exists(), "SIGKILL did not leave a stale lock"
    before = (project / "screenplay.json").read_bytes()
    result = run(alias, "--check")
    assert result.returncode == 0, result.stderr
    assert (project / "screenplay.json").read_bytes() == before
    assert not (project / ".apollo.lock").exists(), "normal exit did not release the recovered lock"
    result = run(project, "--check")
    assert result.returncode == 0, result.stderr
    print("dead owner is recovered without changing source; normal exit releases edit access")


if __name__ == "__main__":
    main()
