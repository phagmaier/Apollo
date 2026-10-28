# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Verify the local Academy reference page by page, allowing only profile deviations.

Run after building layout_probe:
    uv run tools/compare_academy.py --probe build/release/layout_probe

The reference PDF, TSV, Fountain transcription, temporary inputs, and line dumps
stay local. No Academy screenplay text is embedded in this tool.
"""

import argparse
import difflib
import pathlib
import subprocess
import tempfile

from academy_reference import body_lines, fountain, paragraphs


def read_tsv(text):
    rows = []
    for line in text.splitlines():
        if not line or line.startswith("#"):
            continue
        page, slot, kind, text = line.split("\t", 3)
        rows.append(dict(page=int(page), slot=int(slot), kind=kind, text=text))
    return rows


def dump(rows):
    return [f"{r['slot']}\t{r['kind']}\t{r['text']}\n" for r in rows]


def expected_page(rows, page):
    """Apply the four documented deviations, in *body* page numbers.

    Transformations use the independently measured rows, never probe output.
    Every other slot, element, text, and page-end decision must match exactly.
    """
    expected = [r.copy() for r in rows if r["page"] == page]
    deviation = "exact"
    if page == 4:
        if expected[0]["kind"] != "contcue" or expected[0]["slot"] != 0:
            raise ValueError("expected a margin continuation cue on body page 4")
        expected[0]["kind"] = "cue"
        for row in expected:
            row["slot"] += 1
        deviation = "continuation cue uses slot 1; following slots +1"
    elif page == 5:
        speech = [r.copy() for r in rows if r["page"] == 6 and r["slot"] <= 3]
        if [(r["slot"], r["kind"]) for r in speech] != [(1, "cue"), (2, "dialogue"), (3, "dialogue")]:
            raise ValueError("expected a three-line speech at the top of body page 6")
        for row in speech:
            row["slot"] += 53
        expected.extend(speech)
        deviation = "three-line speech retained in slots 54–56"
    elif page == 6:
        if not any(r["slot"] == 28 and r["kind"] == "centered" for r in expected):
            raise ValueError("expected centered date at slot 28 on body page 6")
        for row in expected:
            if row["slot"] > 28:
                row["slot"] -= 2
        deviation = "automatic spacing after centered date; following slots −2"
    elif page == 7:
        shot = next(r.copy() for r in rows if r["page"] == 8)
        if shot["kind"] != "action" or shot["slot"] != 1:
            raise ValueError("expected Shot transcribed as action at the top of body page 8")
        shot["slot"] = 56
        expected.append(shot)
        deviation = "Shot transcribed as action fits at slot 56"
    return expected, deviation


def run_probe(probe, source, workdir):
    path = workdir / "input.fountain"
    path.write_text(source)
    result = subprocess.run([str(probe), str(path)], capture_output=True, text=True, check=True)
    return read_tsv(result.stdout)


def verify(probe, reference):
    rows = read_tsv((reference / "expected.tsv").read_text())
    # Check that the local expectations still come from the independent PDF,
    # and that the transcription used for the full run is the same source as
    # the anchored suffixes. Never silently regenerate either reference file.
    if rows != body_lines(reference / "scriptsample.pdf"):
        raise ValueError("expected.tsv differs from the local PDF measurement")
    source = (reference / "academy.fountain").read_text()
    if source != fountain(paragraphs(rows)):
        raise ValueError("academy.fountain differs from the reference transcription")
    if sorted({r["page"] for r in rows}) != list(range(1, 12)):
        raise ValueError("expected all 11 Academy body pages")

    passed = True
    with tempfile.TemporaryDirectory(prefix="apollo-academy-") as directory:
        workdir = pathlib.Path(directory)
        actual = run_probe(probe, source, workdir)
        # Ignore only generated page continuation markers when checking text
        # integrity through the unanchored, full-document run. This permits
        # rewrapping after a split without hiding lost or repeated source text.
        for index, row in enumerate(actual):
            if index and row["kind"] == "cue" and actual[index - 1]["kind"] == "more":
                row["kind"] = "contcue"
        if paragraphs(actual) != paragraphs(rows):
            print("FAIL full-document paragraph/element text sequence")
            passed = False
        else:
            print(f"PASS full-document text sequence ({len(paragraphs(rows))} body elements)")

        for page in range(1, 12):
            # Restart from this reference page's first line, retaining all later
            # text. In particular, do not truncate at its last line: that would
            # conceal incorrect split, push, and keep-with-next decisions.
            tail = [r.copy() for r in rows if r["page"] >= page]
            if tail[0]["kind"] == "contcue":
                tail[0]["kind"] = "cue"
            anchored = run_probe(probe, fountain(paragraphs(tail)), workdir)
            actual_page = [r for r in anchored if r["page"] == 1]
            expected, deviation = expected_page(rows, page)
            if dump(actual_page) == dump(expected):
                print(f"PASS body page {page}: {len(expected)} lines; {deviation}")
            else:
                passed = False
                print(f"FAIL body page {page}: allowed deviation: {deviation}")
                print("".join(difflib.unified_diff(dump(expected), dump(actual_page),
                                                fromfile="reference + documented deviation", tofile="Apollo")), end="")
    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--probe", type=pathlib.Path, default=pathlib.Path("build/release/layout_probe"))
    parser.add_argument("--reference", type=pathlib.Path, default=pathlib.Path("reference/academy"))
    args = parser.parse_args()
    try:
        passed = verify(args.probe.resolve(), args.reference)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Academy verification failed: {error}\n")
    parser.exit(0 if passed else 1)


if __name__ == "__main__":
    main()
