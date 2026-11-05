"""Saved-writing CLI: staging failures and a real stalled-helper watchdog."""

import argparse
import os
import pathlib
import shlex
import shutil
import subprocess
import time


def run(*command, expect=0):
    result = subprocess.run([str(part) for part in command], capture_output=True, text=True, timeout=55)
    assert result.returncode == expect, (command, result.returncode, result.stdout, result.stderr)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    options = parser.parse_args()
    root = options.output.resolve()
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)
    original = root / "input.fountain"
    original.write_text("A lamp.\n")
    project = root / "Project"
    run(options.editor, project, "--create", original)
    failed = run(options.editor, project, "--export-fountain", root / "missing/out.fountain", expect=1)
    assert "Fountain export failed:" in failed.stderr, failed.stderr
    assert not (root / "missing").exists()

    # Isolate the executable's sibling helper, without a production timeout
    # override or replacing anything in the build directory.
    isolated = root / "isolated"
    isolated.mkdir()
    editor = isolated / "apollo_editor"
    shutil.copy2(options.editor, editor)
    pid_file = root / "worker.pid"
    worker = isolated / "fountain_export"
    worker.write_text(f"#!/bin/sh\necho $$ > {shlex.quote(str(pid_file))}\nkill -STOP $$\n")
    worker.chmod(0o700)
    output = root / "existing.fountain"
    output.write_bytes(b"previous Fountain")
    started = time.monotonic()
    try:
        timed_out = run(editor, project, "--export-fountain", output, expect=1)
        elapsed = time.monotonic() - started
        assert 25 <= elapsed < 50, elapsed
        assert "30-second command-line timeout; cancelling" in timed_out.stderr, timed_out.stderr
        assert "Fountain export cancelled." in timed_out.stderr, timed_out.stderr
        assert output.read_bytes() == b"previous Fountain"
        assert original.read_text() == "A lamp.\n"
        assert not list(root.glob(".apollo-fountain-*"))
        try:
            os.kill(int(pid_file.read_text()), 0)
        except ProcessLookupError:
            pass
        else:
            raise AssertionError("CLI left the stopped helper alive or unreaped")
    finally:
        # A regression may leave a stopped helper after subprocess.run kills
        # its timed-out parent. Keep failed checks from leaking that process.
        if pid_file.exists():
            try:
                os.kill(int(pid_file.read_text()), 9)
            except ProcessLookupError:
                pass
    print(f"CLI write-failure exit, watchdog ({elapsed:.2f}s), preserved bytes, reaping and cleanup passed")


if __name__ == "__main__":
    main()
