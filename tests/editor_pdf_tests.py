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


def break_before_space(editor, output):
    """A saved project whose dialogue has spaces after a deliberate break.

    The spike Fountain parser joins dialogue lines, so the edit is made to the
    saved writing. The editor must lay it out without a QTextLayout mismatch
    (--check), agree with its PDF, and the PDF must match the hand-derived
    expectation in fixtures/editing.
    """
    project = output / "break-before-space"
    shutil.rmtree(project, ignore_errors=True)  # --create never reuses a folder.
    command(editor, project, "--create", ROOT / "fixtures/layout/01-element-geometry.fountain")
    writing = project / "screenplay.json"
    data = json.loads(writing.read_text())
    speech = [e for e in data["elements"] if e["text"].startswith("I left the lights on")]
    assert len(speech) == 1 and speech[0]["type"] == "dialogue", speech
    speech[0]["text"] = "I left the lights on for you.\n  I always leave the lights on, even when nobody comes."
    writing.write_text(json.dumps(data))
    command(editor, project, "--check")
    expected = fixture_pages(ROOT / "fixtures/editing/break-before-space.expected")
    pages, rows, _ = agree(editor, project, output / "break-before-space-agreement")
    assert (pages, rows) == (1, sum(len(p["rows"]) for p in expected)), (pages, rows)
    snapshot = output / "break-before-space-snapshot"
    command(editor, project, "--snapshot", snapshot)
    check_pdf(snapshot / "editor.pdf", expected)
    check_styles(snapshot / "editor.pdf", expected)
    print("PASS deliberate break before spaces: editor/PDF agreement and independent expectation")


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
