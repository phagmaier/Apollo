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
import os
import json
import pathlib
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
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


def blank_dialogue_line(editor, output):
    """Line-edge spaces and a blank authored line: fixtures/editing/blank-dialogue-line."""
    edited_fixture(editor, output, "blank-dialogue-line", {
        "I left the lights on for you. I always leave the lights on.":
            "I left the lights on for you. \n   \nI always leave the lights on, even when nobody comes.  "},
        ROOT / "fixtures/editing/blank-dialogue-line.expected")
    print("PASS blank dialogue line and line-edge spaces: editor/PDF agreement and independent expectation")


def break_at_page_split(editor, output):
    """A deliberate break where a speech splits: fixtures/editing/break-at-page-split."""
    edited_fixture(editor, output, "break-at-page-split", {
        "We should have left before the tide turned. Now the road is under water and the radio is dead. Nobody is coming tonight.":
            "We should have left before the tide turned. \n  Now the road is under water and the radio is dead.\nNobody is coming tonight."},
        ROOT / "fixtures/editing/break-at-page-split.expected", "02-dialogue-split")
    print("PASS deliberate break at a speech split: editor/PDF agreement and independent expectation")


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

def authored_emphasis(editor, output):
    """Real command-authored ordinary project, not a screenplay.json mutation."""
    authored = output / "emphasis-authored"
    shutil.rmtree(authored, ignore_errors=True)  # Only this test's disposable output.
    authored.mkdir(mode=0o700)
    env = dict(os.environ, APOLLO_EMPHASIS_OUTPUT=str(authored.resolve()))
    result = subprocess.run(
        [str(editor.resolve().parent / "project_access_flow_tests"),
         "emphasis_authoring_survives_save_reopen_and_export"],
        env=env, capture_output=True, text=True)
    (authored / "authoring.log").write_text(result.stdout + result.stderr)
    assert result.returncode == 0, (result.stdout, result.stderr)
    project = authored / "project"
    expected = fixture_pages(ROOT / "fixtures/editing/emphasis-authoring.expected")
    assert (authored / "authored.fountain").read_bytes() == (
        ROOT / "fixtures/export/emphasis-authoring.fountain").read_bytes()
    command(editor, project, "--check")
    pages, rows, _ = agree(editor, project, authored / "agreement")
    assert (pages, rows) == (1, 8), (pages, rows)
    snapshot = authored / "snapshot"
    command(editor, project, "--snapshot", snapshot)
    pdf = snapshot / "editor.pdf"
    check_pdf(pdf, expected)
    check_styles(pdf, expected)
    # Positive checks alone miss leaking emphasis into adjacent text or CONT'D.
    xml = ET.fromstring(command("pdftohtml", "-xml", "-hidden", "-fontfullname",
                               "-zoom", 1, "-stdout", pdf))
    faces = {node.attrib["id"]: node.attrib["family"].lower()
             for node in xml.iter("fontspec")}
    styled = {"bold": [], "italic": []}

    def collect(node, active):
        if node.tag in ("b", "i"):
            active = active | {"bold" if node.tag == "b" else "italic"}
        if node.text:
            for style in active:
                styled[style].append(node.text)
        for child in node:
            collect(child, active)
            if child.tail:
                for style in active:
                    styled[style].append(child.tail)

    for node in xml.iter("text"):
        # Poppler merges mixed-face runs under the first run's font id;
        # nested b/i tags, not that id, describe the merged line's styles.
        tagged = any(child.tag in ("b", "i") for child in node.iter())
        collect(node, set() if tagged else {style for style in styled if style in faces[node.attrib["font"]]})
    assert "".join(styled["bold"]) == "Café", styled
    assert "".join(styled["italic"]) == "bells", styled
    # Slot 6 baseline 141pt; red starts nine fixed-width glyphs after x=216pt.
    # At 144dpi the pinned font's underline is ~2.17px below that baseline.
    prefix = authored / "underline"
    command("pdftoppm", "-f", 1, "-l", 1, "-r", 144, "-gray", "-singlefile", pdf, prefix)
    data = prefix.with_suffix(".pgm").read_bytes()
    header = re.match(rb"P5\s+(\d+)\s+(\d+)\s+255\s", data)
    assert header, "expected binary PGM"
    width = int(header[1])
    pixels = data[header.end():]
    assert any(all(pixels[y * width + x] < 100 for x in range(564, 602))
               for y in range(283, 287)), "authored red has no underline stroke"
    assert not any(all(pixels[y * width + x] < 100 for x in range(621, 659))
                   for y in range(283, 287)), "underline leaks into plain lamp"
    print("PASS actual emphasis authoring: ordinary-project reopen, canonical Fountain, "
          "independent editor/PDF geometry, bold/italic/underline and plain generated CONT'D")


def title_pages(editor, output):
    """Multiline title geometry and actual form-authored PDF content."""
    fixture = ROOT / "fixtures/editing/multiline-title.screenplay.json"
    project = output / "multiline-title-project"
    shutil.rmtree(project, ignore_errors=True)  # Only this test's disposable project.
    command(editor, project, "--create")
    shutil.copyfile(fixture, project / "screenplay.json")
    expected = fixture_pages(fixture.with_name("multiline-title.expected"))
    command(editor, project, "--check")
    for included in (True, False):
        snapshot = output / ("multiline-title" if included else "multiline-title-omitted")
        command(editor, project, "--snapshot", snapshot, *([] if included else ["--no-title-page"]))
        check_pdf(snapshot / "editor.pdf", expected if included else expected[1:])
        check_styles(snapshot / "editor.pdf", expected if included else expected[1:])
        # --snapshot deliberately edits its live project after capture. Restore
        # only the disposable fixture project before the next request.
        shutil.copyfile(fixture, project / "screenplay.json")
    authored = output / "title-authored"
    shutil.rmtree(authored, ignore_errors=True)
    result = subprocess.run(
        [str(editor.resolve().parent / "project_access_flow_tests"),
         "title_form_apply_cancel_undo_reopen_and_export"],
        env=dict(os.environ, APOLLO_TITLE_OUTPUT=str(authored.resolve())),
        capture_output=True, text=True)
    assert result.returncode == 0, (result.stdout, result.stderr)
    body = [dict(body=1, number="none", rows=[
        (1, "action", "Current writing.", ""), (3, "action", "Second paragraph.", "")])]
    check_pdf(authored / "body.pdf", body)
    title = dict(body=0, number="none", rows=[
        (4, "title@center", "THE LATER SIGNAL", ""),
        (10, "credit@center", "Written by", ""),
        (15, "author@center", "June Vale", ""),
        (17, "source@center", "Based on a radio play", ""),
        (48, "contact@5.2in", "june@example.test", ""),
        (48, "draft-date@1.5in", "October 2026", "")])
    command(editor, authored / "project", "--snapshot", authored / "snapshot")
    check_pdf(authored / "snapshot/editor.pdf", [title] + body)
    print("PASS title form PDF and multiline title: independent slots, columns, styles, "
          "blank lines, omission and unchanged body-page numbering")



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
    blank_dialogue_line(args.editor, args.output)
    break_at_page_split(args.editor, args.output)
    authored_lowercase(args.editor, args.output)
    authored_emphasis(args.editor, args.output)
    title_pages(args.editor, args.output)
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
