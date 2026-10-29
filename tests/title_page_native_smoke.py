# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Compositor-delivered title form editing in an isolated synthetic project.

No clipboard, private screenplay, desktop capture, or owner application state.
Saved JSON is an autosave oracle, never an authoring mechanism.
"""

import argparse
import json
import os
import pathlib
import signal
import subprocess
import time


FIELDS = [("title", "THE NATIVE SIGNAL"), ("credit", "Written by"), ("author", "June Vale"),
          ("source", "Based on a radio play"), ("contact", "june@example.test"), ("draftDate", "October 2026")]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--editor", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)  # Refuse reuse of someone else's artifacts/state.
    editor = args.editor.resolve()
    environment = dict(os.environ)
    for name in ("XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"):
        directory = output / name.lower()
        directory.mkdir(mode=0o700)
        environment[name] = str(directory)
    assert environment.get("QT_QPA_PLATFORM") == "wayland", "native Wayland is required"
    instances = json.loads(subprocess.check_output(["hyprctl", "-j", "instances"], env=environment, timeout=5))
    matches = [i for i in instances if i["wl_socket"] == pathlib.Path(environment["WAYLAND_DISPLAY"]).name]
    assert len(matches) == 1, "cannot identify the prepared socket's Hyprland instance"
    environment["HYPRLAND_INSTANCE_SIGNATURE"] = matches[0]["instance"]
    source = output / "public-synthetic.fountain"
    source.write_text("The receiver waits.\n")
    project = output / "project"
    subprocess.run([str(editor), str(project), "--create", str(source)], env=environment,
                   check=True, capture_output=True, timeout=30)
    initial = json.loads((project / "screenplay.json").read_text())
    child = None
    evidence = []

    def focused_pid():
        return json.loads(subprocess.check_output(["hyprctl", "-j", "activewindow"],
                                                 env=environment, timeout=5)).get("pid")

    def keys(*arguments):
        assert child.poll() is None and focused_pid() == child.pid, "Apollo lost focus; no keys sent"
        subprocess.run(["wtype", "-d", "15", *arguments], check=True, timeout=20)
        time.sleep(0.15)

    def shortcut(key, shift=False):
        keys("-M", "ctrl", *(["-M", "shift"] if shift else []), "-k", key,
             *(["-m", "shift"] if shift else []), "-m", "ctrl")

    def saved(label, fields):
        deadline = time.monotonic() + 15
        actual = None
        while time.monotonic() < deadline:
            assert child.poll() is None, f"editor exited during {label}"
            actual = json.loads((project / "screenplay.json").read_text())
            if [(e["field"], e["text"]) for e in actual["titlePage"]] == fields:
                assert actual["elements"] == initial["elements"], actual
                evidence.append(dict(step=label, screenplay=actual))
                (output / "evidence.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n")
                return
            time.sleep(0.1)
        raise AssertionError((label, "autosave did not match independent title fields", actual, fields))

    def launch(phase):
        nonlocal child
        data = output / ("data-" + phase)
        data.mkdir(mode=0o700)
        environment["XDG_DATA_HOME"] = str(data)
        with (output / ("editor-" + phase + ".log")).open("w") as log:
            child = subprocess.Popen([str(editor), str(project)], env=environment,
                                     stdout=log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            session = data / "Apollo/session.json"
            if session.exists() and focused_pid() == child.pid:
                assert json.loads(session.read_text())["directory"] == str(project)
                time.sleep(0.5)
                return
            assert child.poll() is None, "editor exited before readiness"
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
        shortcut("t", shift=True)
        for index, (_, text) in enumerate(FIELDS):
            if index:
                # Leave the populated text area, then its Remove/Add controls.
                keys("-k", "Tab", "-k", "Tab", "-k", "Tab")
            keys(text)
        shortcut("Return")
        saved("native-six-fields-apply", FIELDS)
        shortcut("z")
        saved("native-one-step-undo", [])
        shortcut("z", shift=True)
        saved("native-one-step-redo", FIELDS)
        shortcut("t", shift=True)
        shortcut("a")
        keys("CANCELLED")
        keys("-k", "Escape")
        saved("native-cancel", FIELDS)
        stop()
        launch("reopen")
        shortcut("t", shift=True)
        shortcut("a")
        keys("REOPENED SIGNAL")
        shortcut("Return")
        saved("fresh-process-title-edit", [("title", "REOPENED SIGNAL")] + FIELDS[1:])
        shortcut("z")
        saved("fresh-process-undo", FIELDS)
        shortcut("e")
        pdf = pathlib.Path(str(project) + ".pdf")
        deadline = time.monotonic() + 15
        while not pdf.exists() and time.monotonic() < deadline:
            time.sleep(0.1)
        assert pdf.read_bytes().startswith(b"%PDF-")
        stop()
        fountain = output / "native.fountain"
        subprocess.run([str(editor), str(project), "--export-fountain", str(fountain)],
                       env=environment, check=True, capture_output=True, timeout=30)
        assert fountain.read_text() == (
            "Title: THE NATIVE SIGNAL\nCredit: Written by\nAuthor: June Vale\n"
            "Source: Based on a radio play\nContact: june@example.test\nDraft date: October 2026\n\n"
            "The receiver waits.\n")
        pdf_text = subprocess.check_output(["pdftotext", "-layout", str(pdf), "-"], text=True, timeout=10)
        assert all(text in pdf_text for _, text in FIELDS) and "The receiver waits." in pdf_text, pdf_text
        print("PASS native wtype title form: six fields, Tab focus/scroll, Apply/Cancel, one-step Undo/Redo, "
              f"autosave, fresh-process reopen, PDF content and exact Fountain; {output}")
    finally:
        stop()


if __name__ == "__main__":
    main()
