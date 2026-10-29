"""Executable Fountain export: import → typed edits → autosave → new process → export.

The exported bytes are compared with hand-written expected text. With
--aw-parser, afterwriting's independent parser must also read the exports and
the hand-written expected files as the hand-worked semantic models: types,
text, emphasis, dual relationships, title fields and authored breaks.
"""

import argparse
import json
import os
import pathlib
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "fixtures"
TYPED = "the lamp turns and "  # tools/apollo_editor.cpp --autosave-typing


def run(*command, expect=0):
    result = subprocess.run([str(part) for part in command], capture_output=True, text=True, timeout=60)
    assert result.returncode == expect, (command, result.returncode, result.stdout, result.stderr)
    return result


def type_at_end(editor, project, log, keys, element="f21"):
    run(editor, project, "--autosave-typing", log, keys, 20)
    lines = [line.split("\t") for line in pathlib.Path(log).read_text().splitlines()]
    target = next(line for line in lines if line[0] == "target")
    assert target[1] == element, target  # The final action; these fixtures have no page-crossing block.
    done = next(line for line in lines if line[0] == "done")
    assert done[1] == "saved", done
    return int(target[2])


# Independent decoder for the canonical spelling: \* \_ \\ escapes, then
# paired runs _ * ** ***. Offsets count code points, as in the source format.
STYLES = {"_": ["underline"], "*": ["italic"], "**": ["bold"], "***": ["bold", "italic"]}


def decode(marked):
    text, spans, opened, i = "", [], {}, 0
    while i < len(marked):
        if marked[i] == "\\" and i + 1 < len(marked) and marked[i + 1] in "*_\\":
            text += marked[i + 1]
            i += 2
        elif marked[i] in "*_":
            j = i
            while j < len(marked) and marked[j] == marked[i]:
                j += 1
            run_ = marked[i:j]
            if run_ in opened:
                start = opened.pop(run_)
                spans += [(start, len(text), style) for style in STYLES[run_]]
            else:
                opened[run_] = len(text)
            i = j
        else:
            text += marked[i]
            i += 1
    assert not opened, marked
    return text, spans


def joined(lines):
    text, spans = "", []
    for index, line in enumerate(lines):
        if index:
            text += "\n"
        decoded, styles = decode(line)
        spans += [(start + len(text), end + len(text), style) for start, end, style in styles]
        text += decoded
    return text, sorted(spans)


AW_TYPES = {"scene_heading": "sceneHeading", "action": "action", "character": "character", "parenthetical": "parenthetical",
            "dialogue": "dialogue", "transition": "transition", "centered": "centered", "page_break": "pageBreak"}


def aw_reading(node, parser, path):
    reading = json.loads(run(node, ROOT / "tools/aw_tokens.cjs", parser, path).stdout)
    title = [({"draft date": "draftDate"}.get(t["type"], t["type"]), decode(t["text"])[0]) for t in reading["title_page"]]
    elements, lines, previous = [], [], None
    for token in reading["tokens"] + [{"type": "separator"}]:
        kind = AW_TYPES.get(token["type"])
        # Consecutive action or dialogue lines are one authored element.
        if lines and (kind != previous or kind not in ("action", "dialogue")):
            text, spans = joined([line["text"] for line in lines])
            elements.append((previous, text, spans, lines[0]["dual"] == "right" and previous == "character"))
            lines = []
        previous = kind
        if kind:
            lines.append(token)
    return title, elements


def model(screenplay):
    title = [(field["field"], field["text"]) for field in screenplay["titlePage"]]
    elements = []
    for e in screenplay["elements"]:
        spans = sorted((s["start"], s["end"], s["style"]) for s in e.get("styles", []))
        if e["type"] == "lyric":
            # Known representation difference: aw-parser reads ~lyric as italic action.
            elements.append(("action", e["text"], sorted(spans + [(0, len(e["text"]), "italic")]), False))
        else:
            elements.append((e["type"], e["text"], spans, bool(e.get("dual"))))
    return title, elements


def presented(screenplay):
    """Headings and cue names in capitals (docs/case-handling.md); the fixtures' are ASCII."""
    result = json.loads(json.dumps(screenplay))
    for e in result["elements"]:
        if e["type"] in ("sceneHeading", "character"):
            e["text"] = e["text"].upper()
    return result


def case_subset(editor, root):
    """Lowercase headings and cue extensions: import, type, export in capitals."""
    fixture = FIXTURES / "import/case-subset.fountain"
    original, project, out = root / "case-subset.fountain", root / "cased", root / "cased.fountain"
    shutil.copyfile(fixture, original)
    run(editor, project, "--edit-fountain", original)
    assert type_at_end(editor, project, root / "typing-case.log", 1, "f13") == len("The lamp turns.")
    final = "The lamp turns." + TYPED[0]
    exported = run(editor, project, "--export-fountain", out)
    assert ("Written in capitals, as Apollo shows and prints them: 3 scene headings and 3 character cues typed in lowercase. "
            "The project keeps the case as typed.") in exported.stdout, exported.stdout
    expected = (FIXTURES / "export/case-subset.fountain").read_text()
    assert out.read_text() == expected.replace("The lamp turns.\n", final + "\n"), out.read_text()
    # Authored case stays in the writing; the imported original is untouched.
    writing = json.loads((project / "screenplay.json").read_text())
    authored = json.loads((FIXTURES / "import/case-subset.expected.json").read_text())
    assert [(e["type"], e["text"]) for e in writing["elements"][:-1]] == [(e["type"], e["text"]) for e in authored["elements"][:-1]]
    assert (project / "imports/original.fountain").read_bytes() == fixture.read_bytes()
    assert original.read_bytes() == fixture.read_bytes()
    # A forced mixed-case name stays review-only; nothing is published.
    mixed = root / "mixed.fountain"
    mixed.write_text("@McKay\nWords.\n")
    refused = run(editor, root / "mixed", "--edit-fountain", mixed, expect=1)
    assert "forced mixed-case character name requires review" in refused.stderr, refused.stderr
    assert not (root / "mixed").exists()
    authored["elements"][-1]["text"] = final
    return [(out, presented(authored), True),
            (FIXTURES / "export/case-subset.fountain", presented(json.loads((FIXTURES / "import/case-subset.expected.json").read_text())), True)]


def main():
    arguments = argparse.ArgumentParser()
    arguments.add_argument("--editor", required=True, type=pathlib.Path)
    arguments.add_argument("--output", required=True, type=pathlib.Path)
    arguments.add_argument("--aw-parser", type=pathlib.Path)
    arguments.add_argument("--node", default="node")
    options = arguments.parse_args()
    editor, root = options.editor, options.output
    shutil.rmtree(root, ignore_errors=True)
    root.mkdir(parents=True)
    os.environ["QT_QPA_PLATFORM"] = "offscreen"

    original = root / "writer-subset.fountain"
    shutil.copyfile(FIXTURES / "import/writer-subset.fountain", original)
    project, out = root / "project", root / "out.fountain"
    expected = (FIXTURES / "export/writer-subset.fountain").read_text()
    run(editor, project, "--edit-fountain", original)

    # Typing that ends in a space is saved but blocks export; the existing
    # file stays as it was.
    assert type_at_end(editor, project, root / "typing-1.log", len(TYPED)) == len("The train leaves.")
    out.write_bytes(b"Existing export")
    blocked = run(editor, project, "--export-fountain", out, expect=3)
    assert ("Fountain export blocked: Element 21, Action “The train leaves.the lamp turns and ”: "
            "has an empty line or leading, trailing or repeated spaces.") in blocked.stderr, blocked.stderr
    assert out.read_bytes() == b"Existing export"

    # One more typed letter makes the reopened snapshot eligible.
    assert type_at_end(editor, project, root / "typing-2.log", 1) == len("The train leaves." + TYPED)
    final = "The train leaves." + TYPED + "t"
    exported = run(editor, project, "--export-fountain", out)
    assert "exported to" in exported.stdout and "Not included in clean Fountain: 1 imported note" in exported.stdout, exported.stdout
    assert out.read_text() == expected.replace("The train leaves.\n", final + "\n"), out.read_text()
    body = root / "body.fountain"
    run(editor, project, "--export-fountain", body, "--no-title-page")
    assert body.read_text() == out.read_text()[out.read_text().index("INT. PLATFORM - DAY"):]

    # The project folder and the imported original are never replaced.
    source = (project / "screenplay.json").read_bytes()
    run(editor, project, "--export-fountain", project / "screenplay.json", expect=3)
    assert (project / "screenplay.json").read_bytes() == source
    run(editor, project, "--export-fountain", original, expect=3)
    assert original.read_bytes() == (FIXTURES / "import/writer-subset.fountain").read_bytes()
    review = root / "review"
    run(editor, review, "--retain-fountain", original)
    refused = run(editor, review, "--export-fountain", root / "review.fountain", expect=1)
    assert "review-only" in refused.stderr and not (root / "review.fountain").exists()

    case_cases = case_subset(editor, root)

    if options.aw_parser:
        subset = json.loads((FIXTURES / "import/writer-subset.expected.json").read_text())
        typed = json.loads(json.dumps(subset))
        typed["elements"][-1]["text"] = final
        # The QML flow's edits, applied by hand to the semantic fixture.
        edited = json.loads(json.dumps(subset))
        edited["elements"][6]["text"] = "At the\nsame time."
        edited["elements"] += [{"type": "sceneHeading", "text": "INT. DEPOT - DAY"}, {"type": "action", "text": "Mara waits."},
                               {"type": "character", "text": "MARA"}, {"type": "dialogue", "text": "Here."}]
        cases = [(out, typed, True), (body, typed, False), (FIXTURES / "export/writer-subset.fountain", subset, True),
                 (FIXTURES / "export/writer-subset-edited.fountain", edited, True),
                 (FIXTURES / "export/writer-subset-edited-no-title.fountain", edited, False)] + case_cases
        for path, screenplay, with_title in cases:
            title, elements = model(screenplay)
            want = (title if with_title else [], elements)
            got = aw_reading(options.node, options.aw_parser, path)
            assert got == want, (path, got, want)
        print(f"aw-parser 0.1.1 independently read {len(cases)} exports as their hand-worked semantic models")
    print("Fountain export process checks passed")


if __name__ == "__main__":
    main()
