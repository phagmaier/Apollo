# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Compositor-delivered element-type authoring in a disposable ordinary project.

Saved source is evidence, never an authoring mechanism. No clipboard, private
screenplay, owner application state, or desktop-wide capture is used.
"""

import argparse
import copy
import json
import os
import pathlib
import signal
import subprocess
import time

from pdf_tests import check_pdf


SOURCE = "Title: THE NATIVE ELEMENTS\n\nACT ONE\n\nWe follow the light.\n"
FOUNTAIN = "Title: THE NATIVE ELEMENTS\n\n> ACT ONE <\n\n~We follow the light.\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--editor", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)  # Refuse reuse, never touch another run's state.
    editor, project = args.editor.resolve(), output / "project"
    env = dict(os.environ)
    for name in ("XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"):
        directory = output / name.lower()
        directory.mkdir(mode=0o700)
        env[name] = str(directory)
    assert env.get("QT_QPA_PLATFORM") == "wayland", "native Wayland is required"
    instances = json.loads(subprocess.check_output(["hyprctl", "-j", "instances"], env=env, timeout=5))
    matches = [i for i in instances if i["wl_socket"] == pathlib.Path(env["WAYLAND_DISPLAY"]).name]
    assert len(matches) == 1, "cannot identify the selected socket's Hyprland instance"
    env["HYPRLAND_INSTANCE_SIGNATURE"] = matches[0]["instance"]
    source = output / "public-synthetic.fountain"
    source.write_text(SOURCE)
    with (output / "create.log").open("w") as log:
        subprocess.run([str(editor), str(project), "--create", str(source)], env=env,
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=30)
    writing = project / "screenplay.json"
    initial = json.loads(writing.read_text())
    initial.pop("revision")
    assert [(e["type"], e["text"]) for e in initial["elements"]] == [
        ("action", "ACT ONE"), ("action", "We follow the light.")], initial
    child = None
    evidence = []

    def focused_pid():
        return json.loads(subprocess.check_output(["hyprctl", "-j", "activewindow"],
                                                 env=env, timeout=5)).get("pid")

    def keys(*arguments):
        assert child is not None and child.poll() is None and focused_pid() == child.pid, \
            "Apollo lost compositor focus; no keys sent"
        subprocess.run(["wtype", "-d", "15", *arguments], check=True, timeout=20)
        time.sleep(0.12)

    def shortcut(key, shift=False):
        keys("-M", "ctrl", *(["-M", "shift"] if shift else []), "-k", key,
             *(["-m", "shift"] if shift else []), "-m", "ctrl")

    def saved(label, first="action", second="action", text="We follow the light."):
        expected = copy.deepcopy(initial)
        expected["elements"][0]["type"] = first
        expected["elements"][1]["type"] = second
        expected["elements"][1]["text"] = text
        deadline, actual = time.monotonic() + 20, None
        while time.monotonic() < deadline:
            assert child.poll() is None, f"editor exited during {label}"
            try:
                actual = json.loads(writing.read_text())
                revision = actual.pop("revision")
                if actual == expected:
                    evidence.append(dict(step=label, revision=revision, screenplay=actual))
                    (output / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
                    return
            except (OSError, ValueError):
                pass
            time.sleep(0.1)
        raise AssertionError((label, "autosave differs from independent text/type/ID expectation", actual, expected))

    def launch(phase):
        nonlocal child
        data = output / ("data-" + phase)
        data.mkdir(mode=0o700)
        env["XDG_DATA_HOME"] = str(data)
        with (output / ("editor-" + phase + ".log")).open("w") as log:
            child = subprocess.Popen([str(editor), str(project)], env=env, stdout=log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            assert child.poll() is None, "editor exited before ordinary-project readiness"
            try:
                session = json.loads((data / "Apollo/session.json").read_text())
                if session["directory"] == str(project) and focused_pid() == child.pid:
                    time.sleep(0.5)
                    return
            except (OSError, ValueError, KeyError):
                pass
            time.sleep(0.1)
        raise AssertionError("No ready focused Apollo window")

    def stop():
        nonlocal child
        if child and child.poll() is None:
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
        shortcut("End")
        # Every direct shortcut traverses the compositor and the focused TextEdit.
        types = ["sceneHeading", "action", "character", "parenthetical", "dialogue",
                 "transition", "lyric", "centered"]
        for number, kind in enumerate(types, 1):
            shortcut(str(number))
            saved("native-type-" + kind, second=kind)
        shortcut("z")
        saved("native-type-undo", second="lyric")
        shortcut("z", shift=True)
        saved("native-type-redo", second="centered")
        keys("X")
        saved("native-centered-typing", second="centered", text="We follow the light.X")
        shortcut("z")
        saved("native-typing-undo", second="centered")
        keys("-k", "Tab")
        saved("native-outside-cycle-tab-returns-action")
        shortcut("7")
        saved("native-lyric", second="lyric")
        # Home/Up from the final line reaches the preceding body element.
        shortcut("End")
        keys("-k", "Home", "-k", "Up", "-k", "Home")
        shortcut("8")
        saved("native-act-label", first="centered", second="lyric")
        stop()
        launch("reopen")
        shortcut("End")
        shortcut("2")
        saved("fresh-process-type-change", first="centered")
        shortcut("z")
        saved("fresh-process-type-undo", first="centered", second="lyric")
        shortcut("e")
        pdf = pathlib.Path(str(project) + ".pdf")
        deadline = time.monotonic() + 20
        while not pdf.exists() and time.monotonic() < deadline:
            time.sleep(0.1)
        assert pdf.read_bytes().startswith(b"%PDF-")
        stop()
        fountain = output / "native.fountain"
        subprocess.run([str(editor), str(project), "--export-fountain", str(fountain)], env=env,
                       check=True, capture_output=True, timeout=30)
        assert fountain.read_text() == FOUNTAIN
        check_pdf(pdf, [dict(body=0, number="none", rows=[(4, "title@center", "THE NATIVE ELEMENTS", "")]),
                        dict(body=1, number="none", rows=[(1, "centered", "ACT ONE", ""),
                            (3, "lyric", "We follow the light.", "")])])
        assert source.read_text() == SOURCE
        print(f"PASS native wtype element types: all eight shortcuts, lyric/act labels, typing, Tab, "
              f"Undo/Redo, exact IDs, autosave/reopen, canonical Fountain and independent PDF; {output}")
        print("Evidence: evidence.json and own-process logs; no desktop screenshot or clipboard capture.")
    finally:
        stop()


if __name__ == "__main__":
    main()
