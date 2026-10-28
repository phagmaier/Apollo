# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Dump every text line of a screenplay PDF as page, body slot, left edge, and text.

Uses poppler's `pdftotext -bbox`, so the dump is independent of whichever
engine produced the PDF. Slots are 12-point body lines counted from the first
body line; the page-number line is reported as slot "num".

    uv run tools/pdf_lines.py export.pdf [--first-line-ymax 70.8] [--pages 2-5]
"""

import argparse
import collections
import html
import re
import subprocess

WORD = re.compile(r'<word xMin="([\d.]+)" yMin="[\d.]+" xMax="([\d.]+)" yMax="([\d.]+)">(.*?)</word>')


def read_lines(pdf, column_gap=None):
    """Return pages as (ymax, x0_in, x1_in, text) rows, top to bottom.

    When column_gap is supplied (points), split rows at larger gaps, retaining
    dual columns and side-by-side title fields. The default preserves the
    Academy measurement procedure. Touching styled word fragments are joined
    without adding spaces in column mode.
    """
    bbox = subprocess.run(["pdftotext", "-bbox", pdf, "-"], capture_output=True, text=True, check=True).stdout
    pages = []
    for page in bbox.split("<page ")[1:]:
        rows = collections.defaultdict(list)
        for x0, x1, ymax, word in WORD.findall(page):
            rows[round(float(ymax), 1)].append((float(x0), float(x1), html.unescape(word)))
        lines = []
        for y in sorted(rows):
            words = sorted(rows[y])
            if column_gap is None:
                lines.append((y, words[0][0] / 72, words[-1][1] / 72, " ".join(w for _, _, w in words)))
                continue
            start, end, text = words[0]
            for x0, x1, word in words[1:]:
                if x0 - end > column_gap:
                    lines.append((y, start / 72, end / 72, text))
                    start, text = x0, word
                else:
                    text += (" " if x0 - end > 1 else "") + word
                end = x1
            lines.append((y, start / 72, end / 72, text))
        pages.append(lines)
    return pages


def page_range(spec, count):
    if not spec:
        return range(1, count + 1)
    first, _, last = spec.partition("-")
    return range(int(first), int(last or first) + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("pdf")
    parser.add_argument("--first-line-ymax", type=float,
                        help="ymax of body slot 1; default: most common first-line ymax below the number band")
    parser.add_argument("--number-band", type=float, default=50.0, help="lines above this ymax are page numbers")
    parser.add_argument("--pages", help="page range such as 2-5")
    parser.add_argument("--column-gap", type=float,
                        help="split rows at gaps larger than this many points (10.8 for Apollo dual columns)")
    args = parser.parse_args()

    pages = read_lines(args.pdf, args.column_gap)
    first = args.first_line_ymax
    if first is None:
        tops = [next(y for y, *_ in p if y > args.number_band) for p in pages if any(y > args.number_band for y, *_ in p)]
        first = collections.Counter(tops).most_common(1)[0][0]
    for number in page_range(args.pages, len(pages)):
        for y, x0, x1, text in pages[number - 1]:
            slot = "num" if y <= args.number_band else f"{(y - first) / 12 + 1:.1f}"
            print(f"{number}\t{slot}\t{x0:.3f}\t{x1:.3f}\t{text}")


if __name__ == "__main__":
    main()
