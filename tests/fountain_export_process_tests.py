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


AW_TYPES = {"scene_heading": "sceneHeading", "action": "action", "shot": "action", "character": "character", "parenthetical": "parenthetical",
            "dialogue": "dialogue", "transition": "transition", "centered": "centered", "page_break": "pageBreak"}


def aw_reading(node, parser, path):
    reading = json.loads(run(node, ROOT / "tools/aw_tokens.cjs", parser, path).stdout)
    title = [({"draft date": "draftDate"}.get(t["type"], t["type"]), decode(t["text"])[0]) for t in reading["title_page"]]
    elements, lines, previous = [], [], None
    for token in reading["tokens"] + [{"type": "separator"}]:
        assert token["type"] in AW_TYPES or token["type"] == "separator", f"Unhandled independent token type: {token['type']}"
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


SPACING_NOTE = ("Written without the spaces at the start or end of %s, which Apollo does not show or print. "
                "The project keeps them as typed.")


def presented_spacing(screenplay):
    """No spaces at an authored line's edges (docs/spacing.md); for unstyled fixtures."""
    result = json.loads(json.dumps(screenplay))
    for e in result["elements"]:
        e["text"] = "\n".join(line.strip(" ") for line in e["text"].split("\n"))
    return result


def spacing_subset(editor, root):
    """Spacing whose meaning is kept: import, type, export as presented."""
    fixture = FIXTURES / "import/spacing-subset.fountain"
    original, project, out = root / "spacing-subset.fountain", root / "spaced", root / "spaced.fountain"
    shutil.copyfile(fixture, original)
    run(editor, project, "--edit-fountain", original)
    assert type_at_end(editor, project, root / "typing-spacing.log", len(TYPED), "f8") == len("The lamp turns.")
    exported = run(editor, project, "--export-fountain", out)
    assert (SPACING_NOTE % "2 lines") in exported.stdout, exported.stdout
    expected = (FIXTURES / "export/spacing-subset.fountain").read_text()
    assert out.read_text() == expected.replace("The lamp turns.\n", "The lamp turns." + TYPED.rstrip() + "\n"), out.read_text()
    # The writing keeps every authored space; the imported original is untouched.
    model = json.loads((FIXTURES / "import/spacing-subset.expected.json").read_text())
    typed = json.loads(json.dumps(model))
    typed["elements"][-1]["text"] = "The lamp turns." + TYPED
    writing = json.loads((project / "screenplay.json").read_text())
    assert [(e["type"], e["text"]) for e in writing["elements"]] == [(e["type"], e["text"]) for e in typed["elements"]], writing
    assert (project / "imports/original.fountain").read_bytes() == fixture.read_bytes()
    assert original.read_bytes() == fixture.read_bytes()
    # Action indentation, which Fountain prints and Apollo does not show, stays review-only.
    indented = root / "indented.fountain"
    indented.write_text("Rain.\n\n    Indented.\n")
    refused = run(editor, root / "indented", "--edit-fountain", indented, expect=1)
    assert "action-whitespace" in refused.stderr, refused.stderr
    assert not (root / "indented").exists()
    # aw-parser reads the original itself as Apollo presents it, too.
    return [(out, presented_spacing(typed), True), (FIXTURES / "export/spacing-subset.fountain", presented_spacing(model), True),
            (fixture, presented_spacing(model), True)]


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


def punctuation_subset(editor, root):
    """Keep typographic Unicode and code-point emphasis through real typing/reopen."""
    fixture = FIXTURES / "import/punctuation-subset.fountain"
    original, project, out = root / "punctuation.fountain", root / "punctuated", root / "punctuated.fountain"
    shutil.copyfile(fixture, original)
    run(editor, project, "--edit-fountain", original)
    assert type_at_end(editor, project, root / "typing-punctuation.log", 1, "f8") == len("The lamp turns.")
    run(editor, project, "--export-fountain", out)
    expected = (FIXTURES / "export/punctuation-subset.fountain").read_text()
    assert out.read_text() == expected.replace("The lamp turns.\n", "The lamp turns.t\n")
    authored = json.loads((FIXTURES / "import/punctuation-subset.expected.json").read_text())
    typed = json.loads(json.dumps(authored))
    typed["elements"][-1]["text"] += "t"
    saved = json.loads((project / "screenplay.json").read_text())
    assert (saved["titlePage"], saved["elements"]) == (typed["titlePage"], typed["elements"]), saved
    assert original.read_bytes() == fixture.read_bytes()
    assert (project / "imports/original.fountain").read_bytes() == fixture.read_bytes()
    return [(out, typed, True), (FIXTURES / "export/punctuation-subset.fountain", authored, True), (fixture, authored, True)]


def scene_spacing_subset(editor, root):
    """Explicit spacing choice; unfinished speech stays saveable but unexportable."""
    fixture = FIXTURES / "import/scene-spacing.fountain"
    project, out = root / "scene-spaced", root / "scene-spaced.fountain"
    refused = run(editor, project, "--edit-fountain", fixture, expect=1)
    assert "Choose normal scene spacing explicitly" in refused.stderr
    assert not project.exists()
    run(editor, project, "--edit-fountain", fixture, "--normalize-scene-spacing")
    authored = json.loads((FIXTURES / "import/scene-spacing.expected.json").read_text())
    saved = json.loads((project / "screenplay.json").read_text())
    assert (saved["titlePage"], saved["elements"]) == (authored["titlePage"], authored["elements"])
    out.write_bytes(b"Existing export")
    refused = run(editor, project, "--export-fountain", out, expect=3)
    assert "has no dialogue" in refused.stderr and out.read_bytes() == b"Existing export"
    # Set up the independently specified corrected model. The real one-step
    # correction/undo UI is exercised in editor and project-access flow tests.
    corrected = json.loads(json.dumps(authored))
    for e in corrected["elements"]:
        if e["id"] in ("f4", "f5", "f6", "f7"):
            e["type"] = "action"
    (project / "screenplay.json").write_text(json.dumps(corrected))
    assert type_at_end(editor, project, root / "typing-scene-spacing.log", 1, "f11") == len("The dial stops.")
    exported = run(editor, project, "--export-fountain", out)
    assert "2 extra empty lines before scene headings" in exported.stdout
    assert "not spacing-equivalent output" in exported.stdout
    expected = (FIXTURES / "export/scene-spacing.fountain").read_text()
    assert out.read_text() == expected.replace("The dial stops.\n", "The dial stops.t\n")
    assert (project / "imports/original.fountain").read_bytes() == fixture.read_bytes()
    typed = json.loads(json.dumps(corrected))
    typed["elements"][-1]["text"] += "t"
    # aw-parser calls an unforced isolated uppercase action a "shot", not
    # dialogue. Its words must not disappear from the independent comparison.
    shot = root / "shot-action.fountain"
    shot.write_text("A signal blinks.\n\nHOLD.\n\nThe dial turns.\n")
    shot_model = {"titlePage": [], "elements": [{"type": "action", "text": "A signal blinks."},
                  {"type": "action", "text": "HOLD."}, {"type": "action", "text": "The dial turns."}]}
    run(editor, root / "shot-action", "--edit-fountain", shot)
    shot_out = root / "shot-action-out.fountain"
    run(editor, root / "shot-action", "--export-fountain", shot_out)
    assert shot_out.read_text() == "A signal blinks.\n\n!HOLD.\n\nThe dial turns.\n"
    return [(fixture, authored, True), (out, typed, True), (FIXTURES / "export/scene-spacing.fountain", corrected, True),
            (shot, shot_model, True), (shot_out, shot_model, True)]


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

    # Typing that ends in a space is saved as typed. A new process exports it
    # without the space, which Apollo does not show, and says so.
    assert type_at_end(editor, project, root / "typing-1.log", len(TYPED)) == len("The train leaves.")
    assert json.loads((project / "screenplay.json").read_text())["elements"][-1]["text"] == "The train leaves." + TYPED
    spaced = run(editor, project, "--export-fountain", out)
    assert (SPACING_NOTE % "1 line") in spaced.stdout, spaced.stdout
    assert out.read_text() == expected.replace("The train leaves.\n", "The train leaves." + TYPED.rstrip() + "\n"), out.read_text()

    # A refused snapshot leaves an existing file as it was.
    unverified = root / "unverified.fountain"
    unverified.write_text("Wait\u00a0now.\n")
    run(editor, root / "unverified", "--create", unverified)
    out.write_bytes(b"Existing export")
    blocked = run(editor, root / "unverified", "--export-fountain", out, expect=3)
    assert "contains “ ” (U+00A0)" in blocked.stderr, blocked.stderr
    assert out.read_bytes() == b"Existing export"

    # One more typed letter: nothing is left out.
    assert type_at_end(editor, project, root / "typing-2.log", 1) == len("The train leaves." + TYPED)
    final = "The train leaves." + TYPED + "t"
    exported = run(editor, project, "--export-fountain", out)
    assert "exported to" in exported.stdout and "Not included in clean Fountain: 1 imported note" in exported.stdout, exported.stdout
    assert "Written without the spaces" not in exported.stdout, exported.stdout
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
    spacing_cases = spacing_subset(editor, root)
    punctuation_cases = punctuation_subset(editor, root)
    scene_spacing_cases = scene_spacing_subset(editor, root)

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
                 (FIXTURES / "export/writer-subset-edited-no-title.fountain", edited, False)] + case_cases + spacing_cases + punctuation_cases + scene_spacing_cases
        for path, screenplay, with_title in cases:
            title, elements = model(screenplay)
            want = (title if with_title else [], elements)
            got = aw_reading(options.node, options.aw_parser, path)
            assert got == want, (path, got, want)
        # The spacing choice is deliberately non-equivalent. Check the raw
        # oracle separators too; semantic-element comparison omits separators.
        for path, count in [(FIXTURES / "import/scene-spacing.fountain", 2), (root / "scene-spaced.fountain", 0)]:
            tokens = json.loads(run(options.node, ROOT / "tools/aw_tokens.cjs", options.aw_parser, path).stdout)["tokens"]
            extra = sum(a["type"] == b["type"] == "separator" for a, b in zip(tokens, tokens[1:]))
            assert extra == count, (path, extra, count)
        print(f"aw-parser 0.1.1 independently read {len(cases)} exports as their hand-worked semantic models")
    print("Fountain export process checks passed")


if __name__ == "__main__":
    main()
