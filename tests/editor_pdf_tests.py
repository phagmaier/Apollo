# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Editor/PDF agreement (spike check 2).

The editor's line dump is read back from its laid-out QTextLines and
generated decorations; the PDF of the same document is read by Poppler.
Both must agree on page, slot, element, and text. Editor columns are also
checked against the profile, independently of the engine. Agreement is not
correctness on its own: the fixtures' hand-derived expectations are checked
separately, in editor_tests and pdf_tests.
"""

import argparse
import json
import pathlib
import re
import shutil
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from pdf_tests import (ADVANCE, ROOT, check_pdf, check_styles, check_underline,
                       command, expected_x, fixture_pages)


def editor_pages(dump):
    """Expected PDF pages built from the editor dump, after checking its columns."""
    lines = dump.read_text().splitlines()
    _, count, title = lines[0].split("\t")
    first = 0 if title == "1" else 1
    pages = [dict(body=first + i, number="none", rows=[]) for i in range(int(count))]
    for line in lines[1:]:
        page, slot, kind, x, text = line.split("\t", 4)
        page, slot, x = int(page), int(slot), float(x)
        assert slot >= 0, (dump, line, "off the body grid")
        target = pages[page - first]
        if kind == "number":
            assert abs(x + len(text) * ADVANCE - 522) <= 0.01, (dump, line)
            target["number"] = text
            continue
        assert abs(x - expected_x(kind, text)) <= 0.01, (dump, line, expected_x(kind, text))
        target["rows"].append((slot, kind, text, ""))
    return pages, sum(len(p["rows"]) for p in pages)


def agree(editor, source, output, edits=0):
    summary = command(editor, source, "--agreement", output, edits)
    pages, rows = editor_pages(output / "editor.tsv")
    check_pdf(output / "editor.pdf", pages)
    moved = int(re.search(r"(\d+) later elements changed page", summary)[1])
    return len(pages), rows, moved


def edited_fixture(editor, output, name, edits, expected, fixture="01-element-geometry"):
    """A saved project made from a layout fixture with edits to its saved writing.

    The spike Fountain parser cannot express these edits (it joins dialogue
    lines and is not the import path), so they are made to screenplay.json.
    The editor must lay it out without a QTextLayout mismatch (--check), agree
    with its PDF, and the PDF must match the hand-derived expectation.
    """
    project = output / name
    shutil.rmtree(project, ignore_errors=True)  # --create never reuses a folder.
    command(editor, project, "--create", ROOT / f"fixtures/layout/{fixture}.fountain")
    writing = project / "screenplay.json"
    data = json.loads(writing.read_text())
    # A key is an element's text, or (text, n) for its nth occurrence.
    for old, new in edits.items():
        text, n = old if isinstance(old, tuple) else (old, None)
        matches = [e for e in data["elements"] if e["text"] == text]
        assert len(matches) == 1 or (n is not None and n < len(matches)), (old, matches)
        matches[n or 0]["text"] = new
    writing.write_text(json.dumps(data))
    command(editor, project, "--check")
    expected = fixture_pages(expected)
    pages, rows, _ = agree(editor, project, output / f"{name}-agreement")
    assert (pages, rows) == (len(expected), sum(len(p["rows"]) for p in expected)), (pages, rows)
    snapshot = output / f"{name}-snapshot"
    command(editor, project, "--snapshot", snapshot)
    check_pdf(snapshot / "editor.pdf", expected)
    check_styles(snapshot / "editor.pdf", expected)


def break_before_space(editor, output):
    """Spaces after a deliberate break: fixtures/editing/break-before-space."""
    edited_fixture(editor, output, "break-before-space", {
        "I left the lights on for you. I always leave the lights on.":
            "I left the lights on for you.\n  I always leave the lights on, even when nobody comes."},
        ROOT / "fixtures/editing/break-before-space.expected")
    print("PASS deliberate break before spaces: editor/PDF agreement and independent expectation")


def authored_lowercase(editor, output):
    """Headings and cues authored in lowercase print exactly as fixture 01."""
    edited_fixture(editor, output, "authored-lowercase", {
        "INT. LIGHTHOUSE KITCHEN - NIGHT": "int. lighthouse kitchen - night", "MARA": "mara",
        "THEO (V.O.)": "Theo (v.o.)", "EXT. LIGHTHOUSE GALLERY - CONTINUOUS": "ext. Lighthouse gallery - continuous"},
        ROOT / "fixtures/layout/01-element-geometry.expected")
    print("PASS authored lowercase headings and cues: presented in capitals in editor and PDF")
    # A typed lowercase (cont'd) where G1 generates one prints the generated
    # suffix once, as fixture 08's hand-derived page does.
    edited_fixture(editor, output, "authored-lowercase-contd", {
        ("MARA", 1): "mara (cont'd)", "THEO": "theo", "JUNE": "june"},
        ROOT / "fixtures/layout/08-dual-dialogue-and-contd.expected", "08-dual-dialogue-and-contd")
    print("PASS typed lowercase (cont'd): printed once, as the generated continuation")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--editor", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--academy", action="store_true", help="also check the local Academy transcription")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for source in sorted((ROOT / "fixtures/layout").glob("*.fountain")):
        pages, rows, _ = agree(args.editor, source, args.output / source.stem)
        print(f"PASS editor/PDF agreement: {source.stem}, {pages} pages, {rows} lines")
        snapshot = args.output / f"snapshot-{source.stem}"
        command(args.editor, source, "--snapshot", snapshot)
        expected = fixture_pages(source.with_suffix(".expected"))
        check_pdf(snapshot / "editor.pdf", expected)
        check_styles(snapshot / "editor.pdf", expected)
        print(f"PASS immutable editor export against independent fixture: {source.stem}")
    check_underline(args.output / "snapshot-01-element-geometry/editor.pdf", args.output)
    title = ROOT / "fixtures/layout/07-title-page-and-manual-break.fountain"
    no_title = args.output / "snapshot-without-title"
    command(args.editor, title, "--snapshot", no_title, "--no-title-page")
    check_pdf(no_title / "editor.pdf", fixture_pages(title.with_suffix(".expected"))[1:])
    break_before_space(args.editor, args.output)
    authored_lowercase(args.editor, args.output)
    sources = []
    for scenes in (175, 444):
        source = args.output / f"workload-{scenes}.fountain"
        source.write_text(command(sys.executable, ROOT / "tools/generate_script.py", "--scenes", scenes, "--seed", 1))
        sources.append(source)
    if args.academy:
        sources.append(ROOT / "reference/academy/academy.fountain")
    for source in sources:
        for edits in (0, 50):
            pages, rows, moved = agree(args.editor, source, args.output / f"{source.stem}-{edits}", edits)
            assert not edits or moved > 0, "scripted edits moved no element to another page"
            print(f"PASS editor/PDF agreement: {source.stem}, {edits} edits, {pages} pages, {rows} lines, "
                  f"{moved} later elements changed page during edits")


if __name__ == "__main__":
    main()
