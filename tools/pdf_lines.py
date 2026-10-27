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


def read_lines(pdf):
    """Return pages as lists of (ymax, x0_in, x1_in, text), top to bottom."""
    bbox = subprocess.run(["pdftotext", "-bbox", pdf, "-"], capture_output=True, text=True, check=True).stdout
    pages = []
    for page in bbox.split("<page ")[1:]:
        rows = collections.defaultdict(list)
        for x0, x1, ymax, word in WORD.findall(page):
            rows[round(float(ymax), 1)].append((float(x0), float(x1), html.unescape(word)))
        lines = []
        for y in sorted(rows):
            words = sorted(rows[y])
            lines.append((y, words[0][0] / 72, words[-1][1] / 72, " ".join(w for _, _, w in words)))
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
    args = parser.parse_args()

    pages = read_lines(args.pdf)
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
