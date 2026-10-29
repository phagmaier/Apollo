# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Real compositor-delivered emphasis keys; disposable ordinary project only.

No clipboard, owner state, QTest key injection, or private screenplay input.
Saved JSON is an observable autosave oracle, never an authoring mechanism.
"""

import argparse
import copy
import json
import os
import pathlib
import signal
import subprocess
import time


TEXT = "Café bells red ring."
STYLES = [dict(start=0, end=4, style="bold"),
          dict(start=5, end=10, style="italic"),
          dict(start=11, end=14, style="underline")]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--editor", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    # Refuse reuse rather than touching someone else's artifacts or state.
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    assert output.stat().st_mode & 0o777 == 0o700
    editor = args.editor.resolve()
    project = output / "project"
    source = output / "original-public-synthetic.fountain"
    source.write_text("!" + TEXT + "\n")
    env = dict(os.environ)
    for variable in ("XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"):
        directory = output / variable.lower()
        directory.mkdir(mode=0o700)
        env[variable] = str(directory)
    assert env.get("QT_QPA_PLATFORM") == "wayland", "native Wayland environment required"
    instances = subprocess.run(["hyprctl", "-j", "instances"], env=env,
                               capture_output=True, text=True, check=True, timeout=5)
    matches = [instance for instance in json.loads(instances.stdout)
               if instance["wl_socket"] == pathlib.Path(env["WAYLAND_DISPLAY"]).name]
    assert len(matches) == 1, "cannot identify the Hyprland instance for the selected Wayland socket"
    env["HYPRLAND_INSTANCE_SIGNATURE"] = matches[0]["instance"]
    with (output / "create.log").open("w") as log:
        subprocess.run([str(editor), str(project), "--create", str(source)],
                       env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
    writing = project / "screenplay.json"
    initial = json.loads(writing.read_text())
    assert len(initial["elements"]) == 1, initial
    element = initial["elements"][0]
    assert element["text"] == TEXT and element["type"] == "action", element
    assert not element.get("styles"), element
    child = None
    evidence = []

    def focused_pid():
        result = subprocess.run(["hyprctl", "-j", "activewindow"], env=env,
                                capture_output=True, text=True, check=True, timeout=5)
        return json.loads(result.stdout).get("pid")

    def keys(*arguments):
        assert child is not None and child.poll() is None, "editor exited before key delivery"
        # wtype targets the compositor's active window, not our subprocess.
        # Refuse injection rather than changing an unrelated application.
        assert focused_pid() == child.pid, "Apollo lost compositor focus; no keys sent"
        subprocess.run(["wtype", "-d", "15", *arguments], check=True, timeout=20)
        time.sleep(0.12)

    def shortcut(key, shift=False):
        keys("-M", "ctrl", *(["-M", "shift"] if shift else []), "-k", key,
             *(["-m", "shift"] if shift else []), "-m", "ctrl")

    def select(start, end):
        shortcut("Home")
        if start:
            keys(*[argument for _ in range(start) for argument in ("-k", "Right")])
        keys("-M", "shift", *[argument for _ in range(end - start)
                              for argument in ("-k", "Right")], "-m", "shift")

    def saved(label, styles, text=TEXT):
        expected = copy.deepcopy(initial)
        expected.pop("revision")
        expected["elements"][0]["text"] = text
        expected["elements"][0].pop("styles", None)
        if styles:
            expected["elements"][0]["styles"] = styles
        deadline = time.monotonic() + 20
        actual = None
        while time.monotonic() < deadline:
            assert child.poll() is None, f"editor exited during {label}"
            try:
                actual = json.loads(writing.read_text())
                revision = actual.pop("revision")
                if actual == expected:
                    evidence.append(dict(step=label, revision=revision, screenplay=actual))
                    (output / "evidence.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n")
                    return
            except (OSError, ValueError):
                pass
            time.sleep(0.1)
        raise AssertionError((label, "autosave did not match independent code-point expectation", expected, actual))

    def launch(phase):
        nonlocal child
        # A new data root gives a fresh-process readiness witness, not stale state.
        data = output / f"data-{phase}"
        data.mkdir(mode=0o700)
        env["XDG_DATA_HOME"] = str(data)
        log = (output / f"editor-{phase}.log").open("w")
        try:
            child = subprocess.Popen([str(editor), str(project)], env=env,
                                     stdout=log, stderr=subprocess.STDOUT)
        finally:
            log.close()
        deadline = time.monotonic() + 30
        session = data / "Apollo/session.json"
        while time.monotonic() < deadline:
            assert child.poll() is None, "editor exited before ordinary-project readiness"
            try:
                if json.loads(session.read_text())["directory"] == str(project) and focused_pid() == child.pid:
                    # Require both document readiness and compositor activation.
                    time.sleep(0.5)
                    return
            except (OSError, ValueError, KeyError):
                pass
            time.sleep(0.1)
        raise AssertionError("ordinary-project editor did not become ready")

    def stop():
        nonlocal child
        if child is None:
            return
        if child.poll() is None:
            # Signal only our Popen child, never a process-name or desktop-wide kill.
            child.send_signal(signal.SIGTERM)
            try:
                child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)
        child = None

    def interrupted(signum, _frame):
        raise SystemExit(128 + signum)

    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)

    try:
        launch("author")
        for index, (start, end, key) in enumerate(((0, 4, "b"), (5, 10, "i"), (11, 14, "u"))):
            select(start, end)
            shortcut(key)
            saved(f"native-{key}-on", STYLES[:index + 1])
        # Repeating the fully covered selection removes just that style.
        select(0, 4)
        shortcut("b")
        saved("bold-off", STYLES[1:])
        shortcut("z")
        saved("undo-bold-off", STYLES)
        shortcut("z", shift=True)
        saved("redo-bold-off", STYLES[1:])
        shortcut("z")
        saved("restore-bold", STYLES)
        shortcut("End")
        for key in ("b", "i", "u"):
            shortcut(key)
        keys("Z")
        saved("caret-shortcuts-do-not-style-typing", STYLES, TEXT + "Z")
        shortcut("z")
        saved("undo-temporary-typing", STYLES)
        stop()
        launch("reopen")
        # Real-key inverse operations prove the new process loaded styles, not
        # merely that an unchanged old file still exists on disk.
        for index, (start, end, key) in enumerate(((0, 4, "b"), (5, 10, "i"), (11, 14, "u"))):
            select(start, end)
            shortcut(key)
            saved(f"fresh-process-{key}-off", [s for n, s in enumerate(STYLES) if n != index])
            shortcut("z")
            saved(f"fresh-process-{key}-undo", STYLES)
        print(f"PASS native wtype emphasis: exact code-point spans/text/IDs/types, toggle-off, "
              f"undo/redo, caret-only plain typing, autosave and fresh-process reopen; {output}")
        print("Evidence: evidence.json and own-process logs; no desktop screenshot or clipboard capture.")
    finally:
        stop()


if __name__ == "__main__":
    main()
