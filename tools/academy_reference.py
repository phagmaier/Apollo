# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Build the local Academy reference fixture from the Academy format guide PDF.

Reads reference/academy/scriptsample.pdf (download it manually; see
fixtures/README.md) and writes, next to it:

- expected.tsv: every body line Final Draft produced, as page, slot, element, text.
- academy.fountain: a Fountain transcription of the same body text.

The guide was printed to PDF through a Windows PostScript driver: absolute
positions are shifted about 0.24 in left and glyphs are compressed about 2.7%.
Element classification therefore uses the sample's own column offsets, which
the driver did not change. Both outputs stay in the gitignored reference/
folder because the guide's text belongs to the Academy.

    uv run tools/academy_reference.py
"""

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from pdf_lines import read_lines  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent / "reference" / "academy"
FIRST_SLOT_YMAX = 70.8          # ymax of body line 1 in the guide
ACTION_X0 = 1.264               # left edge of action in the shifted guide (true 1.5 in)
COLUMNS = {0.0: "action", 1.5: "dialogue", 1.903: "paren", 2.0: "paren+", 2.5: "cue", 2.498: "cue"}
TRANSITION_RIGHT = 6.866        # right edge of right-aligned transitions in the guide


def classify(x0, x1, text, slot):
    if text == "(MORE)":
        return "more"
    offset = round(x0 - ACTION_X0, 3)
    for known, kind in COLUMNS.items():
        if abs(offset - known) < 0.01:
            if kind == "cue" and slot < 1:
                return "contcue"
            if kind == "action" and re.match(r"(INT|EXT)\.", text):
                return "heading"
            return kind
    if abs(x1 - TRANSITION_RIGHT) < 0.01:
        return "transition"
    return "centered"


def body_lines(pdf):
    lines = []
    for number, page in enumerate(read_lines(pdf)[1:12], start=1):  # body pages 1-11; skip title and notes
        for ymax, x0, x1, text in page:
            if ymax < 40:
                continue  # page number
            text = text.replace("CONT'D", "CONT’D")
            slot = round((ymax - FIRST_SLOT_YMAX) / 12) + 1
            kind = classify(x0, x1, text, slot)
            lines.append({"page": number, "slot": max(slot, 0), "kind": kind, "text": text})
    return lines


def paragraphs(lines):
    """Join wrapped lines back into Fountain paragraphs, skipping generated page continuations."""
    result, previous = [], None
    for line in lines:
        kind = line["kind"]
        if kind in ("more", "contcue"):
            continue
        same_block = previous and line["page"] == previous["page"] and line["slot"] == previous["slot"] + 1
        continues = (kind == previous["kind"] or (kind == "paren+" and previous["kind"] in ("paren", "paren+"))) if previous else False
        across_page = previous and kind == "dialogue" and previous["kind"] == "dialogue" and line["page"] == previous["page"] + 1
        if (same_block and continues and kind != "cue") or across_page:
            result[-1]["text"] += " " + line["text"]
        else:
            result.append({"kind": "paren" if kind == "paren+" else kind, "text": line["text"]})
        previous = line
    return result


def fountain(paras):
    out = ["Title: For a Few Days More", "Credit: Written by", "Author: April Rider", ""]
    for i, para in enumerate(paras):
        kind, text = para["kind"], para["text"]
        following = paras[i + 1]["kind"] if i + 1 < len(paras) else None
        if kind == "cue":
            out.append(text)
        elif kind in ("paren", "dialogue"):
            out.append(text)
            if following not in ("paren", "dialogue"):
                out.append("")
        elif kind == "heading":
            out += [text, ""]
        elif kind == "transition":
            out += [text if text.endswith("TO:") else "> " + text, ""]
        elif kind == "centered":
            out += [f"> {text} <", ""]
        else:
            # Uppercase or colon-terminated action would parse as a cue or transition; force action.
            out += ["!" + text if text.isupper() or text.endswith(":") else text, ""]
    return "\n".join(out) + "\n"


def main():
    pdf = ROOT / "scriptsample.pdf"
    if not pdf.exists():
        sys.exit(f"missing {pdf}; see fixtures/README.md")
    lines = body_lines(pdf)
    with open(ROOT / "expected.tsv", "w") as handle:
        handle.write("# page\tslot\telement\ttext   (Final Draft output measured from the Academy guide)\n")
        for line in lines:
            handle.write(f"{line['page']}\t{line['slot']}\t{line['kind']}\t{line['text']}\n")
    (ROOT / "academy.fountain").write_text(fountain(paragraphs(lines)))
    print(f"wrote {len(lines)} lines across {lines[-1]['page']} body pages to {ROOT}")


if __name__ == "__main__":
    main()
