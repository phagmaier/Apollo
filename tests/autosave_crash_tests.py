# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Crash recovery under continuous typing (writing-build-plan slice 1).

The editor types into a copy of a project with autosave running and logs
each key and save phase with its CLOCK_MONOTONIC time. It is killed with
SIGKILL, either by this script at a random moment or by itself at a chosen
save phase. The saved screenplay is then read with Python's json module,
not Apollo's reader, and must be complete and valid: every element as in
the original project except the typed one, whose text must be the original
with a prefix of the typed keys inserted. Lost writing is the age, at the
kill, of the oldest key missing from the file; it must not exceed the
target. The input log is independent of the saved file but comes from the
same process, so it times key handling, not physical input.
"""

import argparse
import json
import os
import pathlib
import random
import re
import shutil
import signal
import statistics
import subprocess
import sys
import time

PHASES = ["capturing", "captured", "writing", "written", "committed", "acknowledged"]
ID = re.compile(r"^[A-Za-z0-9_-]{1,64}$")


def read_log(path):
    log = dict(keys=[], phases=[], kill=None, target=None, done=None)
    for line in path.read_text().splitlines():
        fields = line.split("\t")
        if fields[0] == "target":
            log["target"] = dict(id=fields[1], offset=int(fields[2]), typed=fields[3], interval=int(fields[4]))
        elif fields[0] == "key":
            log["keys"].append(dict(count=int(fields[1]), ns=int(fields[2]), revision=int(fields[3])))
        elif fields[0] == "phase":
            log["phases"].append(dict(phase=fields[1], revision=int(fields[2]), ns=int(fields[3])))
        elif fields[0] == "kill":
            log["kill"] = dict(phase=fields[1], revision=int(fields[2]), ns=int(fields[3]))
        elif fields[0] == "done":
            log["done"] = dict(state=fields[1], revision=int(fields[2]), ns=int(fields[3]))
    return log


def check_source(source, original):
    """The saved source is a complete, valid screenplay with the original's elements."""
    assert set(source) == {"format", "version", "revision", "titlePage", "elements"}, sorted(source)
    assert source["format"] == "apollo-screenplay" and source["version"] == 1, (source["format"], source["version"])
    assert isinstance(source["revision"], int) and source["revision"] >= original["revision"]
    ids = [e["id"] for e in source["titlePage"] + source["elements"]]
    assert len(ids) == len(set(ids)) and all(ID.match(i) for i in ids), "invalid or repeated ids"
    assert source["titlePage"] == original["titlePage"]
    assert len(source["elements"]) == len(original["elements"])


def typed_count(source, original, target):
    """How many typed keys the saved target element holds."""
    before = next(e for e in original["elements"] if e["id"] == target["id"])
    after = next(e for e in source["elements"] if e["id"] == target["id"])
    for old, new in zip(original["elements"], source["elements"]):
        if old["id"] != target["id"]:
            assert old == new, ("an untyped element changed", old["id"])
    head, tail = before["text"][: target["offset"]], before["text"][target["offset"]:]
    text = after["text"]
    assert text.startswith(head) and text.endswith(tail), "the typed element lost original text"
    inserted = text[len(head): len(text) - len(tail)]
    typed = target["typed"]
    expected = (typed * (len(inserted) // len(typed) + 1))[: len(inserted)]
    assert inserted == expected, ("saved text is not a prefix of the typed keys", inserted[-40:])
    return len(inserted)


def run_once(editor, pristine, directory, keys, interval, kill_at=None, kill_after=None):
    project = directory / "project"
    shutil.copytree(pristine, project)
    log_path = directory / "events.tsv"
    command = [str(editor), str(project), "--autosave-typing", str(log_path), str(keys), str(interval)]
    if kill_at:
        command.append(kill_at)
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    kill_ns = None
    if kill_after is not None:
        # Kill only after typing has started.
        deadline = time.monotonic() + 30
        while (not log_path.exists() or "\nkey\t" not in log_path.read_text()) and process.poll() is None:
            if time.monotonic() > deadline:
                process.kill()
                raise AssertionError("typing never started")
            time.sleep(0.01)
        time.sleep(kill_after)
        kill_ns = time.monotonic_ns()
        process.send_signal(signal.SIGKILL)
    output, _ = process.communicate(timeout=60)
    (directory / "editor-output.txt").write_text(output)
    log = read_log(log_path)
    if kill_at:
        assert process.returncode == -signal.SIGKILL and log["kill"], (kill_at, process.returncode, output[-400:])
        kill_ns = log["kill"]["ns"]
    elif process.returncode != -signal.SIGKILL:
        raise AssertionError(f"editor was not killed (exit {process.returncode}): {output[-400:]}")
    return project, log, kill_ns


def evaluate(project, original, log, kill_ns):
    source = json.loads((project / "screenplay.json").read_text(encoding="utf-8"))
    check_source(source, original)
    count = typed_count(source, original, log["target"])
    keys = [k for k in log["keys"] if k["ns"] <= kill_ns]
    # A key can be handled but not yet logged when the kill arrives.
    assert count <= len(keys) + 1, (count, len(keys))
    lost = [k for k in keys if k["count"] > count]
    loss_ms = (kill_ns - lost[0]["ns"]) / 1e6 if lost else 0.0
    acknowledged = [p["revision"] for p in log["phases"] if p["phase"] == "acknowledged" and p["ns"] <= kill_ns]
    if acknowledged:
        protected = max([k["count"] for k in keys if k["revision"] <= max(acknowledged)], default=0)
        assert count >= protected, ("acknowledged writing is missing", count, protected)
    extra = sorted(p.name for p in project.iterdir() if p.name not in ("project.json", "screenplay.json"))
    return dict(typed=len(keys), saved=count, loss_ms=loss_ms, revision=source["revision"], extra=extra)


def phase_statistics(logs):
    """Save timings from completed phases, and how long each key waited to be committed."""
    spans = {"capture": ("capturing", "captured"), "queue": ("captured", "writing"),
             "serialize+write": ("writing", "written"), "commit": ("written", "committed"),
             "acknowledge": ("committed", "acknowledged")}
    durations = {name: [] for name in spans}
    protection = []
    for log in logs:
        by_revision = {}
        for event in log["phases"]:
            by_revision.setdefault(event["revision"], {}).setdefault(event["phase"], event["ns"])
        for phases in by_revision.values():
            for name, (start, end) in spans.items():
                if start in phases and end in phases:
                    durations[name].append((phases[end] - phases[start]) / 1e6)
        commits = sorted((e["revision"], e["ns"]) for e in log["phases"] if e["phase"] == "committed")
        for key in log["keys"]:
            later = [ns for revision, ns in commits if revision >= key["revision"]]
            if later:
                protection.append((min(later) - key["ns"]) / 1e6)
    return durations, protection


def describe(values):
    if not values:
        return "no samples"
    ordered = sorted(values)
    p95 = ordered[min(len(ordered) - 1, round(0.95 * (len(ordered) - 1)))]
    return f"n={len(values)} p50 {statistics.median(values):.1f} ms, p95 {p95:.1f} ms, max {max(values):.1f} ms"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--editor", required=True, type=pathlib.Path)
    parser.add_argument("--source", required=True, type=pathlib.Path, help="Fountain file to import as the project")
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--keys", type=int, default=60)
    parser.add_argument("--interval", type=int, default=30, help="milliseconds between keys")
    parser.add_argument("--random", type=int, default=4, help="randomly timed kills")
    parser.add_argument("--phase-occurrence", type=int, default=3, help="kill at this occurrence of each save phase")
    parser.add_argument("--max-loss-ms", type=float, default=1000)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    shutil.rmtree(args.output, ignore_errors=True)
    args.output.mkdir(parents=True)
    pristine = args.output / "pristine"
    created = subprocess.run([str(args.editor), str(pristine), "--create", str(args.source)], capture_output=True, text=True)
    assert created.returncode == 0, created.stdout + created.stderr
    original = json.loads((pristine / "screenplay.json").read_text(encoding="utf-8"))
    chooser = random.Random(args.seed)
    typing_seconds = args.keys * args.interval / 1000
    runs = [(f"phase-{phase}", dict(kill_at=f"{phase}:{args.phase_occurrence}")) for phase in PHASES]
    runs += [(f"random-{i + 1}", dict(kill_after=chooser.uniform(0.05, typing_seconds))) for i in range(args.random)]
    results, logs, failures = [], [], 0
    for name, kill in runs:
        directory = args.output / name
        directory.mkdir()
        try:
            project, log, kill_ns = run_once(args.editor, pristine, directory, args.keys, args.interval, **kill)
            result = evaluate(project, original, log, kill_ns)
            logs.append(log)
            verdict = "ok" if result["loss_ms"] <= args.max_loss_ms else "LOSS OVER TARGET"
            failures += verdict != "ok"
            print(f"{name}: {verdict}; {result['saved']}/{result['typed']} logged keys saved, lost writing "
                  f"{result['loss_ms']:.0f} ms, revision {result['revision']}, other files {result['extra'] or 'none'}")
            results.append(dict(run=name, **result))
        except AssertionError as error:
            failures += 1
            print(f"{name}: FAILED {error}")
            results.append(dict(run=name, error=str(error)))
    durations, protection = phase_statistics(logs)
    summary = [f"source {args.source}, {len(original['elements'])} elements, {args.keys} keys every {args.interval} ms",
               f"lost writing at kill: {describe([r['loss_ms'] for r in results if 'loss_ms' in r])} (target {args.max_loss_ms:.0f} ms)",
               f"key to committed save (runs before kill): {describe(protection)}"]
    summary += [f"{name}: {describe(values)}" for name, values in durations.items()]
    print("\n".join(summary))
    (args.output / "summary.txt").write_text("\n".join(summary) + "\n")
    (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
