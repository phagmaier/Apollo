# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Inspect real PDFs with Poppler; expectations come from the hand fixtures."""

import argparse
import pathlib
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from pdf_lines import read_lines

ROOT = pathlib.Path(__file__).resolve().parents[1]
# Baseline 81 pt + Courier Prime hhea descent (700 / 2048 em at 12 pt).
# read_lines rounds its measured ymax to a tenth of a point.
FIRST_YMAX = 85.1
ADVANCE = 7.1953125


def command(*args):
    result = subprocess.run([str(a) for a in args], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"{args[0]} failed ({result.returncode}):\n{result.stderr}")
    return result.stdout


def expected_x(kind, text):
    """Profile columns, in points; this oracle never reads engine geometry."""
    if kind.startswith("dual-"):
        column = 108 if "-L" in kind else 331.2
        if "cue" in kind:
            return column + 72
        if "paren" in kind:
            return column + (36 if "+" in kind else 28.8)
        return column
    if kind in ("cue", "more"):
        return 288
    if kind in ("dialogue", "lyric"):
        return 216
    if kind == "paren":
        return 244.8
    if kind == "paren+":
        return 252
    if kind == "transition":
        return 511.2 - len(text) * ADVANCE
    if kind == "centered":
        return 324 - len(text) * ADVANCE / 2
    if kind.endswith("@center"):
        return 306 - len(text) * ADVANCE / 2
    if kind == "contact@5.2in":
        return 374.4
    return 108


def fixture_pages(path):
    pages = []
    for line in path.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        if line.startswith("page "):
            _, page, number = line.split()
            pages.append(dict(body=0 if page == "title" else int(page), number=number[7:], rows=[]))
            continue
        slot, kind, text, *styles = line.split("\t")
        pages[-1]["rows"].append((int(slot), kind, text, styles[0] if styles else ""))
    return pages


def check_pdf(pdf, expected):
    info = command("pdfinfo", pdf)
    assert re.search(r"Page size:\s+612 x 792 pts", info), info
    assert re.search(rf"Pages:\s+{len(expected)}\s", info), info
    fonts = command("pdffonts", pdf).splitlines()[2:]
    if any(p["rows"] or p["number"] != "none" for p in expected):
        assert fonts, "no embedded text fonts"
    for font in fonts:
        assert re.fullmatch(r"(?:[A-Z]{6}\+)?CourierPrime-\w+\s+CID TrueType\s+Identity-H\s+yes\s+(?:yes|no)\s+yes\s+\d+\s+\d+", font.strip()), font
    pages = read_lines(pdf, column_gap=10.8)
    assert len(pages) == len(expected), pdf
    for page, reference in zip(pages, expected):
        labels = [(y, x0, x1, text) for y, x0, x1, text in page if y < 60]
        number = reference["number"]
        if number == "none":
            assert not labels, (pdf, labels)
        else:
            assert len(labels) == 1 and labels[0][3] == number, (pdf, labels, number)
            assert abs(labels[0][0] - 49.1) < 0.02 and abs(labels[0][2] - 7.25) <= 0.01, labels
        actual = [(round((y - FIRST_YMAX) / 12) + 1, x0 * 72, x1 * 72, text)
                  for y, x0, x1, text in page if y >= 60]
        for y, *_ in page:
            if y >= 60:
                slot = (y - FIRST_YMAX) / 12 + 1
                assert abs(slot - round(slot)) < 0.02, (pdf, y, "off the body grid")
        rows = sorted(reference["rows"], key=lambda r: (r[0], expected_x(r[1], r[2])))
        assert len(actual) == len(rows), (pdf, reference["body"], len(actual), len(rows))
        for (slot, x0, x1, text), (wanted_slot, kind, wanted_text, _) in zip(actual, rows):
            assert (slot, text) == (wanted_slot, wanted_text), (pdf, slot, text, wanted_slot, wanted_text)
            wanted_x = expected_x(kind, text)
            assert abs(x0 - wanted_x) <= 0.72, (pdf, kind, x0, wanted_x)
            assert abs(x1 - (wanted_x + len(text) * ADVANCE)) <= 0.72, (pdf, kind, x1, text)


def check_styles(pdf, expected):
    xml = ET.fromstring(command("pdftohtml", "-xml", "-hidden", "-fontfullname", "-zoom", 1, "-stdout", pdf))
    specs = {node.attrib["id"]: node.attrib for node in xml.iter("fontspec")}
    assert all(spec["size"] == "12" for spec in specs.values()), specs
    styled = {"italic": [], "bold": []}

    def collect(node, active):
        if node.tag in ("i", "b"):
            active = active | {"italic" if node.tag == "i" else "bold"}
        if node.text:
            for style in active:
                styled[style].append(node.text)
        for child in node:
            collect(child, active)
            if child.tail:
                for style in active:
                    styled[style].append(child.tail)

    for node in xml.iter("text"):
        face = specs[node.attrib["font"]]["family"].lower()
        collect(node, {style for style in styled if style in face})
    for page in expected:
        for _, _, text, annotations in page["rows"]:
            for style, value in re.findall(r"(italic|bold)=([^\s]+)", annotations):
                value = text if value == "all" else value
                assert any(value in run for run in styled[style]), (pdf, style, value, styled)


def check_underline(pdf, output):
    # Fixture 01: "our" at x=216, baseline=237. The pinned font's post
    # underline position/thickness are -185/130 units at 2048 units/em.
    prefix = output / "underline"
    command("pdftoppm", "-f", 1, "-l", 1, "-r", 144, "-gray", "-singlefile", pdf, prefix)
    data = prefix.with_suffix(".pgm").read_bytes()
    header = re.match(rb"P5\s+(\d+)\s+(\d+)\s+255\s", data)
    assert header, "expected binary PGM"
    width = int(header[1])
    pixels = data[header.end():]
    # A continuous horizontal stroke spans the interior of all three glyphs.
    assert any(all(pixels[y * width + x] < 100 for x in range(434, 473))
               for y in range(475, 479)), "underlined word has no underline stroke"
    assert not any(all(pixels[y * width + x] < 100 for x in range(480, 519))
                   for y in range(475, 479)), "underline extends into unmarked text"


def check_engine_agreement(exporter, probe, source, pdf):
    command(exporter, source, pdf, "--no-title-page")
    rows = []
    for line in command(probe, source).splitlines():
        page, slot, kind, text = line.split("\t", 3)
        rows.append((int(page), int(slot), kind, text))
    expected = [dict(body=page, number="none" if page == 1 else f"{page}.",
                     rows=[(slot, kind, text, "") for p, slot, kind, text in rows if p == page])
                for page in range(1, rows[-1][0] + 1)]
    check_pdf(pdf, expected)
    return len(expected), len(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exporter", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--probe", required=True, type=pathlib.Path)
    parser.add_argument("--academy", action="store_true",
                        help="also check engine/PDF agreement on the local Academy transcription")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    fixtures = ROOT / "fixtures/layout"
    for source in sorted(fixtures.glob("*.fountain")):
        pdf = args.output / source.with_suffix(".pdf").name
        command(args.exporter, source, pdf)
        expected = fixture_pages(source.with_suffix(".expected"))
        check_pdf(pdf, expected)
        check_styles(pdf, expected)
        print(f"PASS independent PDF fixture: {source.stem}")
    check_underline(args.output / "01-element-geometry.pdf", args.output)

    source = fixtures / "07-title-page-and-manual-break.fountain"
    pdf = args.output / "without-title.pdf"
    command(args.exporter, source, pdf, "--no-title-page")
    check_pdf(pdf, fixture_pages(source.with_suffix(".expected"))[1:])
    source = args.output / "styles.fountain"
    source.write_text('A *slanted* **heavy** ***both*** _marked_ and be**fo**re. Café “yes”.\n')
    pdf = args.output / "styles.pdf"
    command(args.exporter, source, pdf)
    expected = [dict(body=1, number="none", rows=[(1, "action", 'A slanted heavy both marked and before. Café “yes”.', "italic=slanted bold=heavy italic=both bold=both")])]
    check_pdf(pdf, expected)
    check_styles(pdf, expected)
    assert "CourierPrime-BoldItalic" in command("pdffonts", pdf)

    # Preserve empty pages and number them; no page is inferred only from text.
    source.write_text("===\n\nA lamp.\n\n===\n\n===\n")
    command(args.exporter, source, pdf)
    check_pdf(pdf, [dict(body=1, number="none", rows=[]),
                    dict(body=2, number="2.", rows=[(1, "action", "A lamp.", "")]),
                    dict(body=3, number="3.", rows=[]), dict(body=4, number="4.", rows=[])])
    previous = pdf.read_bytes()
    source.write_text("Unsupported \U0001f680 character.\n")
    failed = subprocess.run([str(args.exporter), str(source), str(pdf)], capture_output=True, text=True)
    assert failed.returncode != 0 and "unsupported" in failed.stderr, failed
    assert pdf.read_bytes() == previous, "failed export replaced a valid destination"
    original_source = source.read_bytes()
    failed = subprocess.run([str(args.exporter), str(source), str(source)], capture_output=True, text=True)
    assert failed.returncode != 0 and "must differ" in failed.stderr, failed
    assert source.read_bytes() == original_source, "export overwrote its input"
    source.write_text("A lamp.\n")
    failed = subprocess.run([str(args.exporter), str(source), str(args.output / "missing/out.pdf")],
                            capture_output=True, text=True)
    assert failed.returncode != 0, "export accepted an unwritable destination"
    print("PASS title exclusion, mixed styles/Unicode, blank pages, and export failure preservation")

    for scenes in (175, 444):
        source = args.output / f"workload-{scenes}.fountain"
        source.write_text(command(sys.executable, ROOT / "tools/generate_script.py", "--scenes", scenes, "--seed", 1))
        pdf = source.with_suffix(".pdf")
        pages, lines = check_engine_agreement(args.exporter, args.probe, source, pdf)
        print(f"PASS engine/PDF agreement: {scenes} scenes, {pages} body pages, {lines} lines")
    if args.academy:
        pages, lines = check_engine_agreement(args.exporter, args.probe, ROOT / "reference/academy/academy.fountain",
                                              args.output / "academy.pdf")
        print(f"PASS engine/PDF Academy agreement: {pages} body pages, {lines} lines")


if __name__ == "__main__":
    main()
